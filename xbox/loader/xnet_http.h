/* Copied from the mh1-xbox repo (src/pc/xbox/xnet_http.h, same owner) for FOLD's network loader. */
/* Tiny HTTP/1.0 client for the Xbox bench programs (network loader, dumper):
 * talks to tools/xbox_serve.py on the laptop (docs/xbox.md "Network loader").
 * Needs nxNetInit() first. */
#ifndef XNET_HTTP_H
#define XNET_HTTP_H
extern char xh_host[64];    /* laptop address, default 192.168.1.166 */
extern int xh_port;         /* 8765 */
/* "ip:port" from the first line of FILE (E:\Games\MH1\loader.txt), when it exists */
void xh_read_host(const char *file);
/* one request; returns the body (malloc'd, NUL-terminated, length in *nout) or
 * NULL on any error or a status other than 200. Connects with a 3 s limit. */
char *xh_http(const char *method, const char *path, const void *body, int nbody, int *nout);
/* GET PATH from byte FROM on (a Range request, for resuming a big file): the body goes to FN in
 * pieces as it arrives. Returns the number of body bytes handed over when the whole answer
 * (Content-Length) arrived, -1 - that number when it did not (no answer, more than 10 s without
 * data, a status other than 200/206, a dropped connection, or FN returned nonzero). */
long xh_get_range(const char *path, unsigned long from, int (*fn)(void *ctx, const char *p, int n), void *ctx);
#endif
