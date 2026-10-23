// VMX World's files: see hdd.h. One VMX game = one save folder, UDATA\56580001\<game ID>0000\ holding GAME.VXP and
// SaveMeta.xbx ("Name=..." in UTF-16, which the dashboard's Memory screen shows), under a TitleMeta.xbx that names the
// title "VMX World". The VMX's storage (the firmware's plat_* keys) is UDATA\56580001\VMX\<key>.BIN for now: the
// VMX in the controller isn't real yet (XMU GADGET), so World keeps a VMX of its own on the hard disk.
#include "hdd.h"
#include "../../vmx-firmware/core/vmxfw.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef NXDK
#include <windows.h>
#include <nxdk/mount.h>
#define DISC "D:\\"
#define UDATA "E:\\UDATA\\"
#define SEP "\\"
#undef fopen                                    // (xbox_compat.h: our paths are absolute already)
#else
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#define DISC "build/disc/"
#define UDATA "build/hdd/UDATA/"
#define SEP "/"
#endif
#define WORLD UDATA "56580001" SEP

Vxp CATALOG[MAXVXP], INSTALLED[MAXVXP]; int NCATALOG, NINSTALLED;
#ifdef NXDK
void fold_serial(const char *s);
static void hdd_log(const char *m) { char b[120]; snprintf(b, sizeof b, "HDD: %s\n", m); fold_serial(b); }
#else
static void hdd_log(const char *m) { (void)m; }
#endif

static void mkdir_(const char *p) {
#ifdef NXDK
  CreateDirectoryA(p, NULL);
#else
  mkdir(p, 0755);
#endif
}
static int each_entry(const char *dir, void (*fn)(const char *dir, const char *name, int isDir, void *u), void *u) {
  int n = 0;
#ifdef NXDK
  char pat[260]; snprintf(pat, sizeof pat, "%s*", dir);
  WIN32_FIND_DATAA fd; HANDLE h = FindFirstFileA(pat, &fd); if (h == INVALID_HANDLE_VALUE) return 0;
  do { if (fd.cFileName[0] != '.') { fn(dir, fd.cFileName, (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0, u); n++; } } while (FindNextFileA(h, &fd));
  FindClose(h);
#else
  DIR *d = opendir(dir); if (!d) return 0;
  struct dirent *e;
  while ((e = readdir(d))) {
    if (e->d_name[0] == '.') continue;
    char p[400]; snprintf(p, sizeof p, "%s%s", dir, e->d_name); struct stat st; int isDir = !stat(p, &st) && S_ISDIR(st.st_mode);
    fn(dir, e->d_name, isDir, u); n++;
  }
  closedir(d);
#endif
  return n;
}
uint8_t *hdd_read(const char *path, int *len) {
  FILE *f = fopen(path, "rb"); if (!f) return NULL;
  int cap = 65536, n = 0; uint8_t *b = malloc((size_t)cap);       // (no ftell: unreliable on nxdk)
  while (b) {
    if (n == cap) { uint8_t *nb = realloc(b, (size_t)(cap *= 2)); if (!nb) { free(b); b = NULL; break; } b = nb; }
    size_t r = fread(b + n, 1, (size_t)(cap - n), f); if (!r) break; n += (int)r;
  }
  fclose(f); if (b && len) *len = n;
  return b;
}
int hdd_exists(const char *path) { FILE *f = fopen(path, "rb"); if (f) fclose(f); return f != NULL; }
static int write_file(const char *path, const void *d, int n) {
  FILE *f = fopen(path, "wb"); if (!f) return 0;
  int ok = fwrite(d, 1, (size_t)n, f) == (size_t)n; fclose(f); return ok;
}
// "Key=Value" as the dashboard's .xbx metadata wants it: UTF-16 little-endian with a byte-order mark, CR LF
static int write_meta(const char *path, const char *key, const char *value) {
  char line[120]; int n = snprintf(line, sizeof line, "%s=%s\r\n", key, value);
  uint8_t b[260]; int k = 0; b[k++] = 0xFF; b[k++] = 0xFE;
  for (int i = 0; i < n; i++) { b[k++] = (uint8_t)line[i]; b[k++] = 0; }
  return write_file(path, b, k);
}

static uint32_t rd32(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
static int parse_vxp(const char *path, Vxp *v) {           // the header, NAME and CARD of a GAME.VXP
  int n = 0; uint8_t *d = hdd_read(path, &n); if (!d) return 0;
  int ok = 0;
  if (n >= 32 && !memcmp(d, "VXP1", 4) && rd32(d + 12) <= (uint32_t)n) {
    memset(v, 0, sizeof *v); v->id = rd32(d + 4); v->bytes = n; v->casino = ((d[10] | d[11] << 8) & 8) != 0;
    snprintf(v->path, sizeof v->path, "%s", path);
    uint32_t total = rd32(d + 12);
    for (uint32_t o = 32; o + 8 <= total; ) {
      uint32_t len = rd32(d + o + 4); const uint8_t *p = d + o + 8;
      if (o + 8 + len > total) break;
      if (!memcmp(d + o, "CARD", 4) && len >= 72) memcpy(v->card, p, 72);
      if (!memcmp(d + o, "NAME", 4)) {                     // title \n publisher \n line1|line2 \n blurb
        char t[400]; size_t k = len < sizeof t - 1 ? len : sizeof t - 1; memcpy(t, p, k); t[k] = 0;
        char *f[4] = { t, 0, 0, 0 };
        for (int i = 1; i < 4; i++) { f[i] = f[i - 1] ? strchr(f[i - 1], '\n') : NULL; if (f[i]) *f[i]++ = 0; }
        snprintf(v->title, sizeof v->title, "%s", f[0]);
        if (f[1]) snprintf(v->pub, sizeof v->pub, "%s", f[1]);
        if (f[2]) { char *bar = strchr(f[2], '|'); if (bar) { *bar = 0; snprintf(v->l2, sizeof v->l2, "%s", bar + 1); } snprintf(v->l1, sizeof v->l1, "%s", f[2]); }
        if (f[3]) snprintf(v->blurb, sizeof v->blurb, "%s", f[3]);
        ok = 1;
      }
      o += 8 + ((len + 3) & ~3u);
    }
  }
  free(d); return ok;
}
static int by_title(const void *a, const void *b) { return strcmp(((const Vxp *)a)->title, ((const Vxp *)b)->title); }
static void cat_entry(const char *dir, const char *name, int isDir, void *u) {
  size_t l = strlen(name);
  if (isDir || l < 5 || NCATALOG >= MAXVXP) return;
  if (strcmp(name + l - 4, ".VXP") && strcmp(name + l - 4, ".vxp")) return;
  char p[300]; snprintf(p, sizeof p, "%s%s", dir, name);
  if (parse_vxp(p, &CATALOG[NCATALOG])) NCATALOG++;
}
static void inst_entry(const char *dir, const char *name, int isDir, void *u) {   // a save folder: 12 hex digits
  if (!isDir || strlen(name) != 12 || NINSTALLED >= MAXVXP) return;
  char p[300]; snprintf(p, sizeof p, "%s%s" SEP "GAME.VXP", dir, name);
  if (parse_vxp(p, &INSTALLED[NINSTALLED])) NINSTALLED++;
}
void hdd_scan(void) {
  NCATALOG = NINSTALLED = 0;
  each_entry(DISC "catalog" SEP, cat_entry, NULL);
  each_entry(WORLD, inst_entry, NULL);
  qsort(CATALOG, (size_t)NCATALOG, sizeof(Vxp), by_title);
  qsort(INSTALLED, (size_t)NINSTALLED, sizeof(Vxp), by_title);
}
int hdd_installed(uint32_t id) { for (int i = 0; i < NINSTALLED; i++) if (INSTALLED[i].id == id) return i; return -1; }
static void save_dir(uint32_t id, char *out, int n) { snprintf(out, (size_t)n, WORLD "%08X0000", (unsigned)id); }
int hdd_install(const Vxp *v) {
  int n = 0; uint8_t *d = hdd_read(v->path, &n); if (!d) return 0;
  char dir[200], p[260]; save_dir(v->id, dir, sizeof dir); mkdir_(dir);
  snprintf(p, sizeof p, "%s" SEP "GAME.VXP", dir); int ok = write_file(p, d, n); free(d);
  snprintf(p, sizeof p, "%s" SEP "SaveMeta.xbx", dir); ok = ok && write_meta(p, "Name", v->title);
  hdd_scan(); return ok;
}
static void rm_entry(const char *dir, const char *name, int isDir, void *u) {
  char p[300]; snprintf(p, sizeof p, "%s%s", dir, name);
  if (!isDir) remove(p);
}
int hdd_remove(uint32_t id) {
  char dir[200], d2[210]; save_dir(id, dir, sizeof dir); snprintf(d2, sizeof d2, "%s" SEP, dir);
  each_entry(d2, rm_entry, NULL);
#ifdef NXDK
  int ok = RemoveDirectoryA(dir) != 0;
#else
  int ok = rmdir(dir) == 0;
#endif
  hdd_scan(); return ok;
}
static void count_entry(const char *dir, const char *name, int isDir, void *u) { if (isDir) (*(int *)u)++; }
int hdd_title_saves(uint32_t tid) {
  char d[80]; snprintf(d, sizeof d, UDATA "%08X" SEP, (unsigned)tid); int n = 0;
  each_entry(d, count_entry, &n); return n;
}
const char *hdd_vmx_dir(void) { return WORLD "VMX" SEP; }
int hdd_day(void) {
  time_t t = time(NULL); long d = (long)((t - 1005782400L) / 86400L);   // 1005782400 = 15 Nov 2001 00:00 UTC
  return t > 1005782400L && d < 65535 ? (int)d : -1;
}
void hdd_init(void) {
#ifdef NXDK
  nxMountDrive('E', "\\Device\\Harddisk0\\Partition1\\");
#else
  mkdir_("build"); mkdir_("build/hdd");
#endif
  mkdir_(UDATA); mkdir_(UDATA "56580001"); mkdir_(WORLD "VMX");
  hdd_log("dirs made");
  if (!hdd_exists(WORLD "TitleMeta.xbx")) write_meta(WORLD "TitleMeta.xbx", "TitleName", "VMX World");
  hdd_scan(); { char b[60]; snprintf(b, sizeof b, "catalog %d installed %d", NCATALOG, NINSTALLED); hdd_log(b); }
}

// ---- the VMX firmware's platform calls (vmx-firmware/core/vmxfw.h), on World's VMX storage
void *plat_alloc(size_t n) { return calloc(1, n); }
void *plat_alloc_fast(size_t n) { return calloc(1, n); }
int plat_load(const char *key, void *buf, int max) {
  char p[200]; snprintf(p, sizeof p, "%s%s.BIN", hdd_vmx_dir(), key);
  FILE *f = fopen(p, "rb"); if (!f) return 0;
  int n = (int)fread(buf, 1, (size_t)max, f); fclose(f); return n;
}
void plat_save(const char *key, const void *buf, int n) {
  char p[200]; snprintf(p, sizeof p, "%s%s.BIN", hdd_vmx_dir(), key); write_file(p, buf, n);
}
int plat_day(void) { return hdd_day(); }
const char *plat_name(void) { return "VMX WORLD"; }
int plat_wifi(char lines[4][16]) { (void)lines; return 0; }
void plat_wifi_setup(void) {}
