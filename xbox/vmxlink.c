// The VMXs in the controllers: see vmxlink.h.
#include "vmxlink.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct { VmxInfo info; Fatx vol; void *dev; int used; } Slot;
static Slot S[MAXVMX]; static int n, sel = -1, others; static char event[64];
static void info_refresh(Slot *s) {
  uint32_t ids[48]; s->info.games = vol_games(&s->vol, ids, 48);
  s->info.freeBlocks = (int)(fatx_free_bytes(&s->vol) / 16384);
}
static void vlog(const char *m);
static void add(void *dev, Fatx vol, int port, int slot) {   // a memory unit: keep it if it's a VMX
  VmxSys sy; int mounted = n < MAXVMX && fatx_mount(&vol), isVmx = mounted && vol_sys(&vol, &sy);
  { char b[80]; snprintf(b, sizeof b, "VMX: mounted %d, VMX.SYS %d, serial %08X\n", mounted, isVmx, isVmx ? (unsigned)sy.serial : 0u); vlog(b); }
  if (!isVmx) { fatx_unmount(&vol); others++; snprintf(event, sizeof event, "THAT MEMORY UNIT ISN'T A VMX"); return; }
  Slot *s = &S[n++]; memset(s, 0, sizeof *s);
  s->info.sys = sy; s->info.port = port; s->info.slot = slot; s->vol = vol; s->dev = dev; s->used = 1; info_refresh(s);
  char l[40]; vmxlink_label(n - 1, l, sizeof l); snprintf(event, sizeof event, "%s PLUGGED IN", l);
  uint32_t selSerial = sel >= 0 ? S[sel].info.sys.serial : sy.serial;   // the first VMX to arrive is the one World uses
  for (int i = n - 1; i > 0; i--) {                    // listed by controller and slot
    int a = S[i - 1].info.port * 2 + S[i - 1].info.slot, b = S[i].info.port * 2 + S[i].info.slot;
    if ((a > b && b >= 0) || a < 0) { Slot t = S[i]; S[i] = S[i - 1]; S[i - 1] = t; }
  }
  for (int i = 0; i < n; i++) if (S[i].info.sys.serial == selSerial) sel = i;
}
static void drop(int i) {
  char l[40]; vmxlink_label(i, l, sizeof l); snprintf(event, sizeof event, "%s UNPLUGGED", l);
  fatx_unmount(&S[i].vol);
  uint32_t selSerial = sel >= 0 ? S[sel].info.sys.serial : 0;
  memmove(&S[i], &S[i + 1], sizeof S[0] * (size_t)(n - i - 1)); n--;
  sel = -1; for (int k = 0; k < n; k++) if (S[k].info.sys.serial == selSerial) sel = k;
  if (sel < 0 && n) sel = 0;                           // the selected one went: the next VMX, else the hard disk
}
int vmxlink_count(void) { return n; }
const VmxInfo *vmxlink_info(int i) { return i >= 0 && i < n ? &S[i].info : NULL; }
int vmxlink_selected(void) { return sel; }
void vmxlink_select(int i) { sel = i >= 0 && i < n ? i : -1; }
Fatx *vmxlink_vol(void) { return sel >= 0 ? &S[sel].vol : NULL; }
Fatx *vmxlink_vol_of(int i) { return i >= 0 && i < n ? &S[i].vol : NULL; }
void vmxlink_refresh(void) { if (sel >= 0) info_refresh(&S[sel]); }
int vmxlink_others(void) { return others; }
const char *vmxlink_event(void) { return event; }
void vmxlink_label(int i, char *out, int len) {
  const VmxInfo *v = vmxlink_info(i);
  if (!v) { snprintf(out, (size_t)len, "WORLD'S VMX"); return; }
  if (v->sys.name[0]) snprintf(out, (size_t)len, "%.8s'S VMX", v->sys.name);
  else { char t[10]; vol_serial_text(v->sys.serial, t); snprintf(out, (size_t)len, "VMX %s", t); }
}
void vmxlink_where(int i, char *out, int len) {
  const VmxInfo *v = vmxlink_info(i);
  if (!v) snprintf(out, (size_t)len, "HARD DISK");
  else if (v->port > 0) snprintf(out, (size_t)len, "CONTROLLER %d SLOT %c", v->port, v->slot ? 'B' : 'A');
  else snprintf(out, (size_t)len, "USB");
}

#ifdef NXDK
#include <windows.h>
#include <xboxkrnl/xboxkrnl.h>
#include <usbh_lib.h>
#include <usbh_msc.h>
#include <hub.h>
void fold_serial(const char *s);
static void vlog(const char *m) { fold_serial(m); }
static MSC_T *pending[MAXVMX]; static int npending; static MSC_T *gone[MAXVMX]; static int ngone;
static uint8_t *dma;                                  // transfers go through one physically contiguous buffer
#define DMA_SECTORS 32
static int rd(void *c, uint32_t s, int k, uint8_t *b) {
  while (k > 0) { int m = k < DMA_SECTORS ? k : DMA_SECTORS; if (usbh_umas_read((MSC_T *)c, s, m, dma)) return 0; memcpy(b, dma, (size_t)m * 512); s += (uint32_t)m; b += m * 512; k -= m; }
  return 1;
}
static int wr(void *c, uint32_t s, int k, const uint8_t *b) {
  while (k > 0) { int m = k < DMA_SECTORS ? k : DMA_SECTORS; memcpy(dma, b, (size_t)m * 512); if (usbh_umas_write((MSC_T *)c, s, m, dma)) return 0; s += (uint32_t)m; b += m * 512; k -= m; }
  return 1;
}
static void on_conn(MSC_T *m, int status) { if (npending < MAXVMX) pending[npending++] = m; }
static void on_disconn(MSC_T *m, int status) {
  for (int i = 0; i < npending; i++) if (pending[i] == m) { pending[i] = pending[--npending]; return; }
  if (ngone < MAXVMX) gone[ngone++] = m;
}
// which controller and slot: the memory unit is on the controller's hub (its port = the slot), the controller on
// the console's (internal) hub, whose ports are controllers 3, 4, 1, 2 (as nxdk's SDL pad driver maps them)
static void where(MSC_T *m, int *port, int *slot) {
  UDEV_T *u = m->iface->udev; ULONG internal = XboxHardwareInfo.Flags & XBOX_HW_FLAG_INTERNAL_USB_HUB;
  *port = 0; *slot = -1;
  if (u->parent) *slot = u->port_num >= 3 ? 1 : 0;     // hub port 2 = slot A, 3 = slot B (port 1: the pad itself)
  char b[80]; snprintf(b, sizeof b, "VMX: on hub port %d\n", u->port_num); fold_serial(b);
  while (u) {
    UDEV_T *p = u->parent ? u->parent->iface->udev : NULL;
    if ((internal && p && p->parent == NULL) || (!internal && u->parent == NULL)) {
      static const int map[5] = { 0, 3, 4, 1, 2 }; *port = u->port_num <= 4 ? map[u->port_num] : 0; break;
    }
    u = p;
  }
  if (*port == 0) *slot = -1;
}
void vmxlink_init(void) {
  usbh_core_init(); usbh_umas_init();                 // (SDL's usbh_core_init then does nothing; its pad driver joins)
  usbh_install_msc_conn_callback(on_conn, on_disconn);
  dma = MmAllocateContiguousMemoryEx(DMA_SECTORS * 512, 0, 0xFFFFFFFF, 0, PAGE_READWRITE | PAGE_NOCACHE);
}
int vmxlink_poll(void) {
  int changed = 0;
  while (ngone) {
    MSC_T *m = gone[--ngone]; int found = 0;
    for (int i = 0; i < n; i++) if (S[i].dev == m) { drop(i); found = 1; fold_serial("VMX: unplugged\n"); break; }
    if (!found && others) { others--; snprintf(event, sizeof event, "MEMORY UNIT UNPLUGGED"); }
    changed = 1;
  }
  while (npending) {
    MSC_T *m = pending[--npending]; int port, slot; where(m, &port, &slot);
    char b[96]; snprintf(b, sizeof b, "VMX: memory unit, %u sectors of %u, controller %d slot %d\n", (unsigned)m->uTotalSectorN, (unsigned)m->nSectorSize, port, slot); fold_serial(b);
    Fatx v = { .read = rd, .write = wr, .ctx = m, .sectors = m->uTotalSectorN };
    if (m->nSectorSize != 512) { others++; snprintf(event, sizeof event, "THAT MEMORY UNIT ISN'T A VMX"); }
    else add(m, v, port, slot);
    { int ok = 0; for (int i = 0; i < n; i++) ok |= S[i].dev == m; fold_serial(ok ? "VMX: found\n" : "VMX: not a VMX\n"); }
    changed = 1;
  }
  return changed;
}
#else
static void vlog(const char *m) { (void)m; }
// the PC build: build/vmx.mu is controller 1 slot A, build/vmxK.mu (K 2..8) the next slots, read once at start
static FILE *img[MAXVMX];
static int rd(void *c, uint32_t s, int k, uint8_t *b) { FILE *f = c; return !fseek(f, (long)s * 512, SEEK_SET) && fread(b, 512, (size_t)k, f) == (size_t)k; }
static int wr(void *c, uint32_t s, int k, const uint8_t *b) { FILE *f = c; int ok = !fseek(f, (long)s * 512, SEEK_SET) && fwrite(b, 512, (size_t)k, f) == (size_t)k; fflush(f); return ok; }
void vmxlink_init(void) {}
int vmxlink_poll(void) {
  static int done; if (done) return 0; done = 1;
  for (int k = 0; k < MAXVMX; k++) {
    char p[40]; if (k) snprintf(p, sizeof p, "build/vmx%d.mu", k + 1); else snprintf(p, sizeof p, "build/vmx.mu");
    if (!(img[k] = fopen(p, "r+b"))) continue;
    fseek(img[k], 0, SEEK_END); Fatx v = { .read = rd, .write = wr, .ctx = img[k], .sectors = (uint32_t)(ftell(img[k]) / 512) };
    add(img[k], v, k / 2 + 1, k & 1);
  }
  return n + others > 0;
}
#endif
