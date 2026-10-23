// The VMX in a controller: see vmxlink.h.
#include "vmxlink.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static Fatx vol; static int haveVol, other, changed;
#ifdef NXDK
#include <windows.h>
#include <xboxkrnl/xboxkrnl.h>
#include <usbh_lib.h>
#include <usbh_msc.h>
void fold_serial(const char *s);
static MSC_T *dev, *pending; static int gone;
static uint8_t *dma;                                  // transfers go through one physically contiguous buffer
#define DMA_SECTORS 32
static int rd(void *c, uint32_t s, int n, uint8_t *b) {
  while (n > 0) { int k = n < DMA_SECTORS ? n : DMA_SECTORS; if (!dev || usbh_umas_read(dev, s, k, dma)) return 0; memcpy(b, dma, (size_t)k * 512); s += (uint32_t)k; b += k * 512; n -= k; }
  return 1;
}
static int wr(void *c, uint32_t s, int n, const uint8_t *b) {
  while (n > 0) { int k = n < DMA_SECTORS ? n : DMA_SECTORS; memcpy(dma, b, (size_t)k * 512); if (!dev || usbh_umas_write(dev, s, k, dma)) return 0; s += (uint32_t)k; b += k * 512; n -= k; }
  return 1;
}
static void on_conn(MSC_T *m, int status) { if (!dev && !pending) pending = m; }
static void on_disconn(MSC_T *m, int status) { if (m == dev || m == pending) { if (m == pending) pending = NULL; else gone = 1; } }
void vmxlink_init(void) {
  usbh_core_init(); usbh_umas_init();                 // (SDL's usbh_core_init then does nothing; its pad driver joins)
  usbh_install_msc_conn_callback(on_conn, on_disconn);
  dma = MmAllocateContiguousMemoryEx(DMA_SECTORS * 512, 0, 0xFFFFFFFF, 0, PAGE_READWRITE | PAGE_NOCACHE);
}
int vmxlink_poll(void) {
  if (gone) { gone = 0; dev = NULL; if (haveVol) fatx_unmount(&vol); haveVol = other = 0; fold_serial("VMX: unplugged\n"); return 1; }
  if (pending && !dev) {
    dev = pending; pending = NULL;
    char b[96]; snprintf(b, sizeof b, "VMX: memory unit, %u sectors of %u\n", (unsigned)dev->uTotalSectorN, (unsigned)dev->nSectorSize); fold_serial(b);
    vol = (Fatx){ .read = rd, .write = wr, .sectors = dev->uTotalSectorN };
    if (dev->nSectorSize == 512 && fatx_mount(&vol) && fatx_size(&vol, "56580001\\VMX") == -2) { haveVol = 1; fold_serial("VMX: found\n"); }
    else { other = 1; fold_serial("VMX: not a VMX\n"); }
    return 1;
  }
  return 0;
}
#else
static FILE *img;
static int rd(void *c, uint32_t s, int n, uint8_t *b) { return !fseek(img, (long)s * 512, SEEK_SET) && fread(b, 512, (size_t)n, img) == (size_t)n; }
static int wr(void *c, uint32_t s, int n, const uint8_t *b) { int ok = !fseek(img, (long)s * 512, SEEK_SET) && fwrite(b, 512, (size_t)n, img) == (size_t)n; fflush(img); return ok; }
void vmxlink_init(void) {}
int vmxlink_poll(void) {
  if (changed) return 0;
  changed = 1;
  if (!(img = fopen("build/vmx.mu", "r+b"))) return 0;
  fseek(img, 0, SEEK_END); vol = (Fatx){ .read = rd, .write = wr, .sectors = (uint32_t)(ftell(img) / 512) };
  haveVol = fatx_mount(&vol) && fatx_size(&vol, "56580001\\VMX") == -2; other = !haveVol;
  return 1;
}
#endif
Fatx *vmxlink_vol(void) { return haveVol ? &vol : NULL; }
int vmxlink_other(void) { return other; }
