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

int holo_load(const char *path) {
  int n = 0; uint8_t *d = hdd_read(path, &n);
  if (!d || n < 12 || memcmp(d, "VHOL", 4)) { free(d); return 0; }
  nE = d[4] | d[5] << 8 | d[6] << 16 | d[7] << 24; nP = d[8] | d[9] << 8 | d[10] << 16 | d[11] << 24;
  if (12 + nE * 12 + nP * 10 > n) { free(d); return 0; }
  E = malloc((size_t)nE * 12); P = malloc((size_t)nP * sizeof(HPoint));
  memcpy(E, d + 12, (size_t)nE * 12); memcpy(P, d + 12 + nE * 12, (size_t)nP * sizeof(HPoint));
  free(d);
  for (int i = 0; i < 512; i++) {                    // v = i / 85: green, with a white-hot core past 1.6
    float v = i / 85.0f, hot = v > 1.6f ? (v - 1.6f) * 120 : 0;
    float c[3] = { v * 0.35f * 204 + hot, v * 1.0f * 204 + hot, v * 0.45f * 204 + hot };
    for (int k = 0; k < 3; k++) TINT[i][k] = (uint8_t)(c[k] > 255 ? 255 : c[k]);
  }
  return loaded = 1;
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
    float d = (Y - band) / 9.0f, row = ((Y % 3) ? 1.0f : 0.72f) * (1 + 0.5f * expf(-d * d)) * flick;
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
