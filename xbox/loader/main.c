/* VMX World's network loader, copied from FOLD's (microgames/fold/xbox/loader, itself from the mh1-xbox port):
 * installed once as E:\Games\VMXWORLD\default.xbe (Evolution X lists it as VMX World) by tools/xbox_upload.sh. Evolution X's FTP drops files bigger than about half a megabyte at random
 * points, so the game and its files come over HTTP instead. On each start it:
 *   1. asks the laptop (tools/xbox_serve.py, address in loader.txt) for the build's version,
 *   2. when that differs from world.ver, downloads the whole build in one request (/pack: world.xbe, assets/,
 *      catalog/) and writes it next to itself (any folder the pack names is made),
 *   3. starts world.xbe (its D: is then this folder, where it finds assets\ and catalog\).
 * Two connections a start at most (the MH1 port saw the Xbox's network freeze after many). Without network or
 * an answer it starts the copy already on the disk. World's own data (E:\UDATA\56580001) is never touched.
 */
#include <hal/debug.h>
#include <hal/video.h>
#include <hal/xbox.h>
#include <nxdk/mount.h>
#include <nxdk/net.h>
#include <lwip/netif.h>
#include <lwip/ip4_addr.h>
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "xnet_http.h"
#include "xbox_path.h"

extern struct netif *g_pnetif;
static char DIR[200] = "E:\\Games\\VMXWORLD\\";
static int net_up;

static char *read_small(const char *path, int *n) {   /* no fseek / ftell (unreliable in nxdk's C library) */
  FILE *f = fopen(path, "rb"); char *b; size_t got;
  if (!f) return NULL;
  b = malloc(4097); got = b ? fread(b, 1, 4096, f) : 0; fclose(f);
  if (!b) return NULL;
  b[got] = 0; *n = (int)got; return b;
}
static int write_file(const char *path, const void *p, int n) {   /* one WriteFile (pdclib's fwrite goes 1 KB at a time) */
  HANDLE h = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
  DWORD done = 0; BOOL ok;
  if (h == INVALID_HANDLE_VALUE) return -1;
  ok = WriteFile(h, p, (DWORD)n, &done, NULL);
  return CloseHandle(h) && ok && done == (DWORD)n ? 0 : -1;
}
static void launch(const char *xbe) {          /* stop the NIC first: the next program hangs in nxNetInit otherwise */
  if (net_up) nxNetShutdown();
  XLaunchXBE(xbe);
}
static void start_game(void) {
  char game[240]; FILE *f;
  snprintf(game, sizeof game, "%sworld.xbe", DIR);
  if (!(f = fopen(game, "rb"))) {
    debugPrint("\nNo game at %s yet, and the laptop did not send one.\n", game);
    debugPrint("Start tools/xbox_serve.py on the laptop, then VMX World again.\nBack to the dashboard in 30 s.\n");
    Sleep(30000); launch(NULL);
  }
  fclose(f);
  debugPrint("Starting VMX World.\n");
  launch(game);
}
static long num(const char **p) { long v = 0; while (**p == ' ') (*p)++; while (**p >= '0' && **p <= '9') v = v * 10 + (*(*p)++ - '0'); return v; }

/* "FOLDPACK <files>\n", then per file "<size> <path>\n", then the files one after another */
static int unpack(const char *b, int n) {
  const char *p = b, *end = b + n, *data; long files, i, size[64]; char path[64][96]; long total = 0;
  if (strncmp(p, "FOLDPACK ", 9)) return -1;
  p += 9; files = num(&p); if (*p++ != '\n' || files < 1 || files > 64) return -1;
  for (i = 0; i < files; i++) {
    int k = 0;
    size[i] = num(&p); if (*p++ != ' ') return -1;
    while (p < end && *p != '\n' && k < 95) path[i][k++] = *p++ == '/' ? '\\' : p[-1];
    path[i][k] = 0; if (p >= end || *p++ != '\n' || strstr(path[i], "..")) return -1;
    total += size[i];
  }
  if (p + total != end) return -1;
  data = p;
  for (i = 0; i < files; i++) {                 /* every folder a file sits in */
    char d[300], *s; snprintf(d, sizeof d, "%s%s", DIR, path[i]);
    for (s = d + strlen(DIR); (s = strchr(s, '\\')) != NULL; s++) { *s = 0; CreateDirectoryA(d, NULL); *s = '\\'; }
  }
  for (i = 0; i < files; i++) {
    char full[300]; snprintf(full, sizeof full, "%s%s", DIR, path[i]);
    if (write_file(full, data, (int)size[i]) < 0) { debugPrint("Could not write %s\n", full); return -1; }
    data += size[i];
  }
  debugPrint("Wrote %ld files.\n", files);
  return 0;
}

int main(void) {
  char dir[200], host[240], verf[240], *ver, *have, *pack; int n, hn = 0, try;
  XVideoSetMode(640, 480, 32, REFRESH_DEFAULT);
  nxMountDrive('E', "\\Device\\Harddisk0\\Partition1\\");
  if (xbox_image_dir(dir, sizeof dir, NULL, 0)) snprintf(DIR, sizeof DIR, "%s\\", dir);
  snprintf(host, sizeof host, "%sloader.txt", DIR); xh_read_host(host);
  snprintf(verf, sizeof verf, "%sworld.ver", DIR);
  debugPrint("VMX World network loader (%s)\n", DIR);
  if (nxNetInit(NULL) != 0) { debugPrint("No network.\n"); start_game(); }
  net_up = 1;
  debugPrint("Xbox %s, laptop %s:%d\n", ip4addr_ntoa(netif_ip4_addr(g_pnetif)), xh_host, xh_port);
  if (!(ver = xh_http("GET", "/version", NULL, 0, &n))) { debugPrint("The laptop does not answer.\n"); start_game(); }
  ver[strcspn(ver, "\r\n")] = 0;
  have = read_small(verf, &hn);
  if (have && !strcmp(have, ver)) { debugPrint("Build %s is up to date.\n", ver); start_game(); }
  for (try = 0; try < 2; try++) {
    DWORD t0 = GetTickCount();
    debugPrint("Downloading build %s ...\n", ver);
    if ((pack = xh_http("GET", "/pack", NULL, 0, &n)) != NULL) {
      debugPrint("Got %d bytes in %lu ms.\n", n, (unsigned long)(GetTickCount() - t0));
      if (unpack(pack, n) == 0 && write_file(verf, ver, (int)strlen(ver)) == 0) { free(pack); break; }
      free(pack);
    }
    debugPrint("Download failed%s.\n", try ? "; starting the copy on the disk" : ", once more");
  }
  start_game();
  return 0;
}
