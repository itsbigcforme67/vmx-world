/* Copied from the mh1-xbox repo (src/pc/xbox/xnet_http.c, same owner) for FOLD's network loader. */
/* xnet_http.c - see xnet_http.h */
#include "xnet_http.h"
#include <lwip/sockets.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef XH_HOST                /* make -C src/pc/xbox/loader XH_HOST='"10.0.2.2"' XH_PORT=8799: a test loader for xemu */
#define XH_HOST "192.168.1.166"
#endif
#ifndef XH_PORT
#define XH_PORT 8770
#endif
char xh_host[64] = XH_HOST;
int xh_port = XH_PORT;

void xh_read_host(const char *file)
{
    char line[96], *colon;
    FILE *f = fopen(file, "r");
    if (!f)
        return;
    if (fgets(line, sizeof line, f)) {
        line[strcspn(line, "\r\n ")] = 0;
        if ((colon = strchr(line, ':')) != NULL) {
            *colon = 0;
            xh_port = atoi(colon + 1);
        }
        if (line[0])
            snprintf(xh_host, sizeof xh_host, "%s", line);
    }
    fclose(f);
}

/* connect with a 3 s limit; -1 when the laptop does not answer */
static int dial(void)
{
    struct sockaddr_in a;
    struct timeval tv = { 3, 0 };
    fd_set w;
    int s = lwip_socket(AF_INET, SOCK_STREAM, 0), err = 0;
    socklen_t el = sizeof err;

    if (s < 0)
        return -1;
    memset(&a, 0, sizeof a);
    a.sin_family = AF_INET;
    a.sin_port = htons(xh_port);
    a.sin_addr.s_addr = inet_addr(xh_host);
    lwip_fcntl(s, F_SETFL, O_NONBLOCK);
    if (lwip_connect(s, (struct sockaddr *)&a, sizeof a) < 0 && errno != EINPROGRESS) {
        lwip_close(s);
        return -1;
    }
    FD_ZERO(&w);
    FD_SET(s, &w);
    if (lwip_select(s + 1, NULL, &w, NULL, &tv) <= 0 ||
        lwip_getsockopt(s, SOL_SOCKET, SO_ERROR, &err, &el) < 0 || err) {
        lwip_close(s);
        return -1;
    }
    lwip_fcntl(s, F_SETFL, 0);
    return s;
}

static int send_all(int s, const void *p, int n)
{
    const char *c = p;
    while (n > 0) {
        int k = lwip_send(s, c, n, 0);
        if (k <= 0)
            return -1;
        c += k;
        n -= k;
    }
    return 0;
}

/* wait at most 10 s for data: nxdk's lwIP has no SO_RCVTIMEO, and a download stalled half way
 * on 12 Oct 2026 (the loader then hung for good) */
static int recv_wait(int s, char *p, int n)
{
    struct timeval tv = { 10, 0 };
    fd_set r;
    FD_ZERO(&r);
    FD_SET(s, &r);
    if (lwip_select(s + 1, &r, NULL, NULL, &tv) <= 0)
        return -1;
    return lwip_recv(s, p, n, 0);
}

/* one HTTP/1.0 request; returns the body (malloc'd, NUL-terminated) or NULL
 * on any error or a status other than 200 */
char *xh_http(const char *method, const char *path, const void *body, int nbody, int *nout)
{
    char req[256], *buf = NULL, *p;
    int s, cap = 0, n = 0, k;

    if ((s = dial()) < 0)
        return NULL;
    snprintf(req, sizeof req, "%s %s HTTP/1.0\r\nHost: %s\r\nContent-Length: %d\r\n\r\n",
             method, path, xh_host, nbody);
    if (send_all(s, req, strlen(req)) < 0 || (nbody && send_all(s, body, nbody) < 0))
        goto fail;
    for (;;) {
        if (cap - n < 65536) {
            cap = cap ? cap * 2 : 1 << 20;
            if (!(p = realloc(buf, cap + 1)))
                goto fail;
            buf = p;
        }
        if ((k = recv_wait(s, buf + n, cap - n)) < 0)
            goto fail;
        if (k == 0)
            break;
        n += k;
    }
    lwip_close(s);
    s = -1;
    if (!buf || n < 12 || strncmp(buf + 9, "200", 3))
        goto fail;
    buf[n] = 0;
    if (!(p = strstr(buf, "\r\n\r\n")))
        goto fail;
    p += 4;
    if ((k = strstr(buf, "Content-Length:") ? atoi(strstr(buf, "Content-Length:") + 15) : -1) >= 0 &&
        k != n - (p - buf))
        goto fail;   /* cut short */
    *nout = n - (p - buf);
    memmove(buf, p, *nout + 1);
    return buf;
fail:
    if (s >= 0)
        lwip_close(s);
    free(buf);
    return NULL;
}

long xh_get_range(const char *path, unsigned long from, int (*fn)(void *ctx, const char *p, int n), void *ctx)
{
    static char buf[65536];
    char req[320], *body, *cl;
    long want = -1, got = 0;
    int s, n = 0, k;

    if ((s = dial()) < 0)
        return -1;
    snprintf(req, sizeof req, "GET %s HTTP/1.0\r\nHost: %s\r\nRange: bytes=%lu-\r\n\r\n", path, xh_host, from);
    if (send_all(s, req, strlen(req)) < 0)
        goto out;
    for (;;) {                  /* the header: status line, Content-Length */
        if (n >= (int)sizeof buf - 1 || (k = recv_wait(s, buf + n, sizeof buf - 1 - n)) <= 0)
            goto out;
        n += k;
        buf[n] = 0;
        if ((body = strstr(buf, "\r\n\r\n")) != NULL)
            break;
    }
    if (n < 12 || !(!strncmp(buf + 9, "206", 3) || (!strncmp(buf + 9, "200", 3) && from == 0)))
        goto out;
    *body = 0;
    if (!(cl = strstr(buf, "Content-Length:")))
        goto out;
    want = atol(cl + 15);
    body += 4;
    n -= body - buf;
    while (got < want) {
        if (n > 0) {
            if (n > want - got)
                n = want - got;
            if (fn(ctx, body, n))
                break;
            got += n;
        }
        body = buf;
        if ((n = recv_wait(s, buf, sizeof buf)) <= 0)
            break;
    }
out:
    lwip_close(s);
    return got == want ? got : -1 - got;
}
