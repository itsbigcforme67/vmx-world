// The VMX skeleton hologram: see holo.h. The model's glow and edges for a view are rendered once into an 8-bit
// picture (with the bloom and the glass baked in) and kept; the turning VMX keeps one per step of its turn, so after
// the first turn it costs a copy a frame. Every frame then adds that picture, the projector beam and the live screen
// to gfx_px through a green tint table, with the scanlines, the rolling band and the flicker applied per row.
// Brightness is kept as v * 85 (v 0..3, as in tools/hologram_preview.py).
#include "holo.h"
#include "hdd.h"
#include "../../microgames/fold/app/gfx.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

typedef struct { int16_t x, y, z; int8_t nx, ny, nz; uint8_t pad; } HPoint;
static int16_t *E; static HPoint *P; static int nE, nP, loaded;
static const float SCR_X0 = -10.05f, SCR_X1 = 10.05f, SCR_Y0 = 32.97f, SCR_Y1 = 53.07f, SCR_Z = 4.05f;

#define TURN 40                                       // steps in the idle turn
typedef struct {
  int x0, y0, w, h;                                   // the region of the screen it draws in
  float cx, cy, scale, centreY;                       // where the model's centre lands, px per mm, the centre's y (mm)
  int frames; uint8_t *cache[TURN]; uint8_t *beam;    // the model per step, the projector beam
  float fadeY;                                        // below this the model fades into the beam (0: no fade)
  float beamX, beamY, beamK;
  int16_t *cell;                                      // a still view: the VMX pixel each pixel shows (-1 none; -2 - c: c's halo)
} View;
static View V[2] = {
  { 280, 134, 324, 220, 442, 236, 3.3f, 31, TURN, { 0 }, NULL, 0, 442, 342, 0.6f },
  { 60, 90, 520, 350, 320, 262, 9.4f, 43, 1, { 0 }, NULL, 440, 320, 470, 0.8f },
};
static uint8_t TINT[512][3];                          // brightness index -> the added colour
static void tint_table(void);
static void angles(int view, int k, float *yaw, float *pitch);

int holo_load(const char *path) {
  int n = 0; uint8_t *d = hdd_read(path, &n);
  if (!d || n < 12 || memcmp(d, "VHOL", 4)) { free(d); return 0; }
  nE = d[4] | d[5] << 8 | d[6] << 16 | d[7] << 24; nP = d[8] | d[9] << 8 | d[10] << 16 | d[11] << 24;
  if (12 + nE * 12 + nP * 10 > n) { free(d); return 0; }
  E = malloc((size_t)nE * 12); P = malloc((size_t)nP * sizeof(HPoint));
  memcpy(E, d + 12, (size_t)nE * 12); memcpy(P, d + 12 + nE * 12, (size_t)nP * sizeof(HPoint));
  free(d);
  tint_table();
  return loaded = 1;
}
static void tint_table(void) {
  for (int i = 0; i < 512; i++) {                    // v = i / 85: green, with a white-hot core past 1.6
    float v = i / 85.0f, hot = v > 1.6f ? (v - 1.6f) * 120 : 0;
    float c[3] = { v * 0.35f * 204 + hot, v * 1.0f * 204 + hot, v * 0.45f * 204 + hot };
    for (int k = 0; k < 3; k++) TINT[i][k] = (uint8_t)(c[k] > 255 ? 255 : c[k]);
  }
}

// ---- rendering a view into an 8-bit picture (once per view / turn step)
static float R[9];
static void rotation(float yaw, float pitch) {
  float cy = cosf(yaw), sy = sinf(yaw), cp = cosf(pitch), sp = sinf(pitch);
  // Rx(pitch) * Ry(yaw), as the preview
  R[0] = cy; R[1] = 0; R[2] = sy;
  R[3] = sp * sy; R[4] = cp; R[5] = -sp * cy;
  R[6] = -cp * sy; R[7] = sp; R[8] = cp * cy;
}
static void project(const View *v, float x, float y, float z, float *sx, float *sy, float *vz) {
  y -= v->centreY;
  float X = R[0] * x + R[1] * y + R[2] * z, Y = R[3] * x + R[4] * y + R[5] * z, Z = R[6] * x + R[7] * y + R[8] * z;
  float d = 160.0f / (160.0f - Z);
  *sx = v->cx + X * d * v->scale - v->x0; *sy = v->cy - Y * d * v->scale - v->y0; if (vz) *vz = Z;
}
static void line_add(float *b, int w, int h, float x0, float y0, float x1, float y1, float val) {
  int n = (int)(fabsf(x1 - x0) > fabsf(y1 - y0) ? fabsf(x1 - x0) : fabsf(y1 - y0)) + 1;
  float dx = (x1 - x0) / n, dy = (y1 - y0) / n, x = x0, y = y0;
  for (int i = 0; i <= n; i++, x += dx, y += dy) {
    int X = (int)x, Y = (int)y; if (X >= 0 && Y >= 0 && X < w && Y < h && b[Y * w + X] < val) b[Y * w + X] = val;
  }
}
static void box_blur(float *b, float *t, int w, int h, int r) {   // one box pass across, one down
  for (int y = 0; y < h; y++) {
    float s = 0; float *row = b + y * w, *o = t + y * w;
    for (int x = -r; x <= r; x++) s += row[x < 0 ? 0 : x >= w ? w - 1 : x];
    for (int x = 0; x < w; x++) { o[x] = s / (2 * r + 1); int a = x - r, c = x + r + 1; s += row[c >= w ? w - 1 : c] - row[a < 0 ? 0 : a]; }
  }
  for (int x = 0; x < w; x++) {
    float s = 0;
    for (int y = -r; y <= r; y++) s += t[(y < 0 ? 0 : y >= h ? h - 1 : y) * w + x];
    for (int y = 0; y < h; y++) { b[y * w + x] = s / (2 * r + 1); int a = y - r, c = y + r + 1; s += t[(c >= h ? h - 1 : c) * w + x] - t[(a < 0 ? 0 : a) * w + x]; }
  }
}
static int inside_quad(const float q[4][2], float x, float y) {   // a convex quad, either winding
  int pos = 0, neg = 0;
  for (int k = 0; k < 4; k++) {
    const float *a = q[k], *b = q[(k + 1) & 3];
    float c = (b[0] - a[0]) * (y - a[1]) - (b[1] - a[1]) * (x - a[0]);
    if (c > 0) pos++; else if (c < 0) neg++;
  }
  return !(pos && neg);
}
static uint8_t *render(View *v, float yaw, float pitch) {
  int w = v->w, h = v->h, n = w * h;
  float *acc = calloc((size_t)n, 4), *lin = calloc((size_t)n, 4), *tmp = malloc((size_t)n * 4);
  uint8_t *out = malloc((size_t)n);
  if (!acc || !lin || !tmp || !out) { free(acc); free(lin); free(tmp); free(out); return NULL; }
  rotation(yaw, pitch);
  // the faces: points brighter where the surface turns away (the preview's 260k points with blur, from 90k here)
  float k = 260000.0f / (float)nP, sz = v->scale / 4.8f;
  if (sz < 1) k *= sz * sz;                            // smaller: the same points on fewer pixels, so each counts less
  int stride = v->scale < 4 ? 2 : 1; k *= stride;     // small: half the points do
  for (int i = 0; i < nP; i += stride) {
    float sx, sy; project(v, P[i].x * 0.01f, P[i].y * 0.01f, P[i].z * 0.01f, &sx, &sy, NULL);
    int X = (int)sx, Y = (int)sy; if (X < 0 || Y < 0 || X >= w - 1 || Y >= h - 1) continue;
    float nz = (R[6] * P[i].nx + R[7] * P[i].ny + R[8] * P[i].nz) / 127.0f, f = 1 - fabsf(nz);
    float wgt = (0.025f + 0.22f * f * f) * k, fx = sx - X, fy = sy - Y;
    acc[Y * w + X] += wgt * (1 - fx) * (1 - fy); acc[Y * w + X + 1] += wgt * fx * (1 - fy);
    acc[(Y + 1) * w + X] += wgt * (1 - fx) * fy; acc[(Y + 1) * w + X + 1] += wgt * fx * fy;
  }
  { float *t2 = malloc((size_t)n * 4); if (t2) { box_blur(acc, t2, w, h, v->scale > 6 ? 2 : 1); free(t2); } }   // soften the points
  // the edges: brighter nearer the camera
  float zmin = 1e9f, zmax = -1e9f;
  for (int i = 0; i < nE; i++) { float sx, sy, z; project(v, E[i * 6] * 0.01f, E[i * 6 + 1] * 0.01f, E[i * 6 + 2] * 0.01f, &sx, &sy, &z); if (z < zmin) zmin = z; if (z > zmax) zmax = z; }
  for (int i = 0; i < nE; i++) {
    float ax, ay, az, bx, by, bz; const int16_t *e = E + i * 6;
    project(v, e[0] * 0.01f, e[1] * 0.01f, e[2] * 0.01f, &ax, &ay, &az); project(v, e[3] * 0.01f, e[4] * 0.01f, e[5] * 0.01f, &bx, &by, &bz);
    float near = ((az + bz) * 0.5f - zmin) / (zmax - zmin + 1e-6f);
    line_add(lin, w, h, ax, ay, bx, by, 0.35f + 0.65f * near);
  }
  // the glass hides what's behind it
  float q[4][2]; const float G[4][2] = { { SCR_X0 - .6f, SCR_Y1 + .6f }, { SCR_X1 + .6f, SCR_Y1 + .6f }, { SCR_X1 + .6f, SCR_Y0 - .6f }, { SCR_X0 - .6f, SCR_Y0 - .6f } };
  for (int c = 0; c < 4; c++) project(v, G[c][0], G[c][1], SCR_Z, &q[c][0], &q[c][1], NULL);
  float qx0 = 1e9f, qx1 = -1e9f, qy0 = 1e9f, qy1 = -1e9f;
  for (int c = 0; c < 4; c++) { if (q[c][0] < qx0) qx0 = q[c][0]; if (q[c][0] > qx1) qx1 = q[c][0]; if (q[c][1] < qy0) qy0 = q[c][1]; if (q[c][1] > qy1) qy1 = q[c][1]; }
  for (int y = (int)qy0; y <= (int)qy1; y++) for (int x = (int)qx0; x <= (int)qx1; x++) {
    if (x < 0 || y < 0 || x >= w || y >= h || !inside_quad(q, x + .5f, y + .5f)) continue;
    acc[y * w + x] = acc[y * w + x] * 0.15f + 0.05f; lin[y * w + x] *= 0.1f;
  }
  // core, then the bloom (three box passes ~ a gaussian of 6 px)
  for (int i = 0; i < n; i++) { float c = acc[i] + lin[i] * 0.9f; acc[i] = c > 3 ? 3 : c; lin[i] = acc[i]; }
  for (int p = 0; p < 3; p++) box_blur(lin, tmp, w, h, 5);
  for (int i = 0; i < n; i++) {
    float val = acc[i] + lin[i] * 1.4f;
    if (v->fadeY > 0) { float y = (float)(i / w + v->y0), f = (v->fadeY - y) / 120.0f; val *= f < 0.15f ? 0.15f : f > 1 ? 1 : f; }
    int b = (int)(val * 85); out[i] = (uint8_t)(b > 255 ? 255 : b);
  }
  free(acc); free(lin); free(tmp);
  return out;
}
static uint8_t *beam(View *v) {                       // the emitter's glowing disc and a faint cone up into the model
  int w = v->w, h = v->h; uint8_t *b = calloc((size_t)w * h, 1); if (!b) return NULL;
  float bx = v->beamX - v->x0, by = v->beamY - v->y0;
  for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) {
    float dx = (x - bx) / 9.0f, dy = (y - by) / 2.2f, r = sqrtf(dx * dx + dy * dy);
    float disc = r < 15 ? (15 - r) * 0.06f * (15 - r) / 15.0f : 0;
    float up = by - y, cone = 0;
    if (up > 0 && up < 260) { float half = 40 + up * 0.55f, a = 1 - fabsf(x - bx) / half; if (a > 0) cone = a * (1 - up / 260.0f) * 0.09f; }
    int k = (int)((disc + cone) * v->beamK * 85); b[y * w + x] = (uint8_t)(k > 255 ? 255 : k);
  }
  return b;
}

// ---- the pictures made ahead of time (tools: xbox/Makefile.pc's holo_bake), so the Xbox only loads them
// "VHPK", u32 views, then per view: u32 frames, w, h, and per frame u32 length + the bytes, zeros packed as 0, count
static int pack_frame(const uint8_t *src, int n, uint8_t *dst) {
  int o = 0;
  for (int i = 0; i < n; ) {
    if (src[i]) { dst[o++] = src[i++]; continue; }
    int run = 0; while (i < n && !src[i] && run < 255) { i++; run++; }
    dst[o++] = 0; dst[o++] = (uint8_t)run;
  }
  return o;
}
#ifndef NXDK
#include <stdio.h>
int holo_bake(const char *out) {
  if (!loaded) return 0;
  FILE *f = fopen(out, "wb"); if (!f) return 0;
  uint32_t nv = 2; fwrite("VHPK", 1, 4, f); fwrite(&nv, 4, 1, f);
  long total = 0;
  for (int view = 0; view < 2; view++) {
    View *v = &V[view]; uint32_t hd[3] = { (uint32_t)v->frames, (uint32_t)v->w, (uint32_t)v->h }; fwrite(hd, 4, 3, f);
    uint8_t *buf = malloc((size_t)v->w * v->h * 2 + 16);
    for (int k = 0; k < v->frames; k++) {
      float y, p; angles(view, k, &y, &p);
      if (!v->cache[k]) v->cache[k] = render(v, y, p);
      uint32_t len = (uint32_t)pack_frame(v->cache[k], v->w * v->h, buf); fwrite(&len, 4, 1, f); fwrite(buf, 1, len, f); total += len;
    }
    free(buf);
  }
  fclose(f); printf("%s: %ld bytes of pictures\n", out, total);
  return 1;
}
// FOLD's VMX pictures (microgames/fold/assets/vmx_small, vmx_large: its VMX window, World's fallback): the model
// head-on, so the live 64x64 screen is a plain rectangle, sized so a VMX pixel is sc screen pixels, cropped to the
// top part round the screen with a fade at the cut. "VMXF", w, h, screen x, screen y (u16), then w*h RGBA: the glow
// as green light (colour and alpha from the tint table, so drawn over the dark panels it adds up like World's)
int holo_bake_fold(const char *out, int sc, int side, int extra) {
  if (!loaded) return 0;
  float d = 160.0f / (160.0f - SCR_Z), scale = 64.0f * sc / ((SCR_X1 - SCR_X0) * d);
  float xmax = 0, ymax = -1e9f, zmax = -1e9f;
  for (int i = 0; i < nP; i++) { float x = fabsf(P[i].x * 0.01f), y = P[i].y * 0.01f, z = P[i].z * 0.01f; if (x > xmax) xmax = x; if (y > ymax) ymax = y; if (z > zmax) zmax = z; }
  float dz = 160.0f / (160.0f - zmax), cy0 = (SCR_Y0 + SCR_Y1) * 0.5f;
  int m = 18, full = (int)(2 * xmax * dz * scale) + 2 * m;
  View v = { 0 }; v.centreY = cy0; v.scale = scale; v.cx = full * 0.5f; v.cy = (ymax - cy0) * dz * scale + m;
  int sx = (int)lroundf(v.cx + SCR_X0 * d * scale), sy = (int)lroundf(v.cy - (SCR_Y1 - cy0) * d * scale);
  v.cx += sx - (v.cx + SCR_X0 * d * scale); v.cy += sy - (v.cy - (SCR_Y1 - cy0) * d * scale);   // the screen on whole pixels
  v.w = full; v.h = sy + 64 * sc + extra;
  rotation(0, 0);
  uint8_t *pic = render(&v, 0, 0); if (!pic) return 0;
  int left = side < 0 ? 0 : sx - side, right = side < 0 ? v.w : sx + 64 * sc + side;
  if (left < 0) left = 0; if (right > v.w) right = v.w;
  int w = right - left, h = v.h, fade = extra < 24 ? extra : 24;
  FILE *f = fopen(out, "wb"); if (!f) { free(pic); return 0; }
  uint16_t hd[4] = { (uint16_t)w, (uint16_t)h, (uint16_t)(sx - left), (uint16_t)sy }; fwrite("VMXF", 1, 4, f); fwrite(hd, 2, 4, f);
  for (int y = 0; y < h; y++) for (int x = left; x < right; x++) {
    int i = pic[y * v.w + x]; if (y > h - fade) i = i * (h - y) / fade;
    const uint8_t *t = TINT[i]; int a = t[0] > t[1] ? t[0] : t[1]; if (t[2] > a) a = t[2];
    uint8_t px[4] = { 0, 0, 0, (uint8_t)a };
    if (a) for (int k = 0; k < 3; k++) px[k] = (uint8_t)(t[k] * 255 / a);
    fwrite(px, 1, 4, f);
  }
  fclose(f); free(pic); printf("%s: %dx%d, screen at %d,%d\n", out, w, h, sx - left, sy);
  return 1;
}
#endif
int holo_load_baked(const char *path) {
  int n = 0; uint8_t *d = hdd_read(path, &n), *p;
  if (!d || n < 8 || memcmp(d, "VHPK", 4)) { free(d); return 0; }
  p = d + 8; uint8_t *end = d + n;
  for (int view = 0; view < 2 && p + 12 <= end; view++) {
    View *v = &V[view]; uint32_t fr, w, h; memcpy(&fr, p, 4); memcpy(&w, p + 4, 4); memcpy(&h, p + 8, 4); p += 12;
    if ((int)fr != v->frames || (int)w != v->w || (int)h != v->h) { free(d); return 0; }   // made for another layout
    for (uint32_t k = 0; k < fr; k++) {
      uint32_t len; if (p + 4 > end) break; memcpy(&len, p, 4); p += 4; if (p + len > end) break;
      uint8_t *o = malloc((size_t)w * h); int at = 0;
      for (uint32_t i = 0; i < len && at < (int)(w * h); ) {
        if (p[i]) o[at++] = p[i++]; else { int run = p[i + 1]; memset(o + at, 0, (size_t)run); at += run; i += 2; }
      }
      v->cache[k] = o; p += len;
    }
  }
  free(d); tint_table();
  return loaded = 1;
}

// ---- every frame
static uint8_t scr[520 * 350];                         // the live screen for this frame (v * 85)
static void screen_layer(View *v, const uint8_t *bits) {
  memset(scr, 0, (size_t)v->w * v->h);
  float sx = (SCR_X1 - SCR_X0) / 64, sy = (SCR_Y1 - SCR_Y0) / 64;
  for (int j = 0; j < 64; j++) for (int i = 0; i < 64; i++) {
    if (!bits[j * 64 + i]) continue;
    float x = SCR_X0 + i * sx, y = SCR_Y1 - j * sy, q[4][2];
    project(v, x, y, SCR_Z, &q[0][0], &q[0][1], NULL); project(v, x + sx, y, SCR_Z, &q[1][0], &q[1][1], NULL);   // whole cells: no seams
    project(v, x + sx, y - sy, SCR_Z, &q[2][0], &q[2][1], NULL); project(v, x, y - sy, SCR_Z, &q[3][0], &q[3][1], NULL);
    int x0 = (int)fminf(fminf(q[0][0], q[1][0]), fminf(q[2][0], q[3][0])), x1 = (int)fmaxf(fmaxf(q[0][0], q[1][0]), fmaxf(q[2][0], q[3][0]));
    int y0 = (int)fminf(fminf(q[0][1], q[1][1]), fminf(q[2][1], q[3][1])), y1 = (int)fmaxf(fmaxf(q[0][1], q[1][1]), fmaxf(q[2][1], q[3][1]));
    for (int Y = y0; Y <= y1; Y++) for (int X = x0; X <= x1; X++)
      if (X >= 1 && Y >= 1 && X < v->w - 1 && Y < v->h - 1 && inside_quad(q, X + .5f, Y + .5f)) {
        scr[Y * v->w + X] = 115;                                                    // 1.35
      }
  }
}
static void screen_halo(View *v) {                      // a soft halo round the lit pixels
  int w = v->w;
  for (int y = 1; y < v->h - 1; y++) for (int x = 1; x < w - 1; x++) {
    uint8_t *p = &scr[y * w + x]; if (*p) continue;
    if (scr[y * w + x - 1] == 115 || scr[y * w + x + 1] == 115 || scr[(y - 1) * w + x] == 115 || scr[(y + 1) * w + x] == 115) *p = 30;
  }
}
static void angles(int view, int k, float *yaw, float *pitch) {
  float a = (float)k / V[view].frames * 6.2831853f;
  *yaw = (view == HOLO_LIST ? (-24 + 22 * sinf(a)) : -14) * 0.0174533f;
  *pitch = (view == HOLO_LIST ? (-12 + 3 * sinf(2 * a)) : -8) * 0.0174533f;
}
static void cell_map(View *v) {                        // a still view: which VMX pixel each pixel shows, and the halos
  int w = v->w, h = v->h; v->cell = malloc((size_t)w * h * 2); if (!v->cell) return;
  for (int i = 0; i < w * h; i++) v->cell[i] = -1;
  float sx = (SCR_X1 - SCR_X0) / 64, sy = (SCR_Y1 - SCR_Y0) / 64;
  for (int j = 0; j < 64; j++) for (int i = 0; i < 64; i++) {
    float x = SCR_X0 + i * sx, y = SCR_Y1 - j * sy, q[4][2];
    project(v, x, y, SCR_Z, &q[0][0], &q[0][1], NULL); project(v, x + sx, y, SCR_Z, &q[1][0], &q[1][1], NULL);
    project(v, x + sx, y - sy, SCR_Z, &q[2][0], &q[2][1], NULL); project(v, x, y - sy, SCR_Z, &q[3][0], &q[3][1], NULL);
    int x0 = (int)fminf(fminf(q[0][0], q[1][0]), fminf(q[2][0], q[3][0])), x1 = (int)fmaxf(fmaxf(q[0][0], q[1][0]), fmaxf(q[2][0], q[3][0]));
    int y0 = (int)fminf(fminf(q[0][1], q[1][1]), fminf(q[2][1], q[3][1])), y1 = (int)fmaxf(fmaxf(q[0][1], q[1][1]), fmaxf(q[2][1], q[3][1]));
    for (int Y = y0; Y <= y1; Y++) for (int X = x0; X <= x1; X++)
      if (X >= 0 && Y >= 0 && X < w && Y < h && inside_quad(q, X + .5f, Y + .5f)) v->cell[Y * w + X] = (int16_t)(j * 64 + i);
  }
  int16_t *c = v->cell;                                // pixels just outside a cell: its halo
  for (int y = 1; y < h - 1; y++) for (int x = 1; x < w - 1; x++) {
    int i = y * w + x; if (c[i] != -1) continue;
    int n = c[i - 1] >= 0 ? c[i - 1] : c[i + 1] >= 0 ? c[i + 1] : c[i - w] >= 0 ? c[i - w] : c[i + w] >= 0 ? c[i + w] : -1;
    if (n >= 0) c[i] = (int16_t)(-2 - n);
  }
}
// one step of work a tick, from World's start: the turning VMX's pictures, then the still one, so PLAY is ready
void holo_prepare(void) {
  if (!loaded) return;
  for (int view = 0; view < 2; view++) {
    View *v = &V[view];
    for (int k = 0; k < v->frames; k++) if (!v->cache[k]) { float y, p; angles(view, k, &y, &p); v->cache[k] = render(v, y, p); return; }
    if (!v->beam) { v->beam = beam(v); return; }
    if (v->frames == 1 && !v->cell) { float y, p; angles(view, 0, &y, &p); rotation(y, p); cell_map(v); return; }
  }
}
void holo_draw(int view, int t, const uint8_t *bits, int screenOn) {
  if (!loaded) return;
  View *v = &V[view];
  int k = v->frames > 1 ? (t / 2) % v->frames : 0;
  float yaw, pitch; angles(view, k, &yaw, &pitch);
  if (!v->cache[k] && !(v->cache[k] = render(v, yaw, pitch))) return;
  if (!v->beam && !(v->beam = beam(v))) return;
  rotation(yaw, pitch);                                // the screen is projected with this step's rotation
  if (v->frames == 1 && !v->cell) cell_map(v);
  if (!screenOn) memset(scr, 0, (size_t)v->w * v->h);
  else if (v->cell) {                                  // a still view: look the pixels up
    for (int i = 0; i < v->w * v->h; i++) { int c = v->cell[i]; scr[i] = c >= 0 ? (bits[c] ? 115 : 0) : c <= -2 ? (bits[-2 - c] ? 30 : 0) : 0; }
  } else { screen_layer(v, bits); screen_halo(v); }
  const uint8_t *c = v->cache[k], *b = v->beam;
  float band = (float)fmod(t / 30.0 * 0.6, 1.3) * GH, flick = 0.94f + 0.06f * sinf(t * 1.3f);
  for (int y = 0; y < v->h; y++) {
    int Y = y + v->y0; if (Y < 0 || Y >= GH) continue;
    float d = (Y - band) / 9.0f, row = (1 + 0.5f * expf(-d * d)) * flick;   // (the scanlines: World's post pass)
    int mul = (int)(row * 256);
    uint32_t *px = gfx_px + Y * GW + v->x0; const uint8_t *cr = c + y * v->w, *br = b + y * v->w, *sr = scr + y * v->w;
    for (int x = 0; x < v->w; x++) {
      int i = ((cr[x] + br[x] + sr[x]) * mul) >> 8; if (!i) continue; if (i > 511) i = 511;
      uint32_t p = px[x];
      int r = (int)((p >> 16) & 255) + TINT[i][0], g = (int)((p >> 8) & 255) + TINT[i][1], bl = (int)(p & 255) + TINT[i][2];
      px[x] = (uint32_t)((r > 255 ? 255 : r) << 16 | (g > 255 ? 255 : g) << 8 | (bl > 255 ? 255 : bl));
    }
  }
}
