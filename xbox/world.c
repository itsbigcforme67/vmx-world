// VMX World on the original Xbox (docs/DESIGN.md "The World disc"): the TV app. 640 x 480, drawn by FOLD's software
// renderer (microgames/fold/app/gfx.c) and run by FOLD's platform layers (app.h's interface: main_xbox.c on the Xbox,
// main_headless.c for scripted tests on the PC). Tabs: NEWS, GET GAMES (the disc's catalogue -> the hard disk),
// MY VMX (what's installed), PLAY (the real VMX firmware and its games, running on the TV), SCORES (the VMX's records
// and casino chips), APPLETS (FOLD BUILDER, the VMX CASINO cashier). The look follows mockup/world.html.
#include "../../microgames/fold/app/app.h"
#include "../../microgames/fold/app/gfx.h"
#include "../../vmx-firmware/core/vmxfw.h"
#include "hdd.h"
#include "vmxlink.h"
#include "holo.h"
// ---- the hologram look (DESIGN.md "The look on the Xbox side"): the kit lives in FOLD's gfx.c, shared with FOLD BUILDER
#define HG GFX_HG
#define HG_DIM GFX_HG_DIM
#define HG_EDGE GFX_HG_EDGE
#define mixc gfx_mix
#define add_rect gfx_add_rect
#define glass gfx_glass
#define brackets gfx_brackets
#define gtext gfx_gtext
#define hpanel gfx_hpanel
#define hsel gfx_hsel
#define post gfx_post
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef NXDK
#include <hal/xbox.h>
void fold_serial(const char *s);                           // main_xbox.c: a line out of COM1 (xemu's serial.txt)
void fold_launch(const char *xbe);                           // main_xbox.c: start another XBE, USB and GPU stopped first
#define LOG(s) fold_serial("WORLD: " s "\n")
#else
#define LOG(s) ((void)0)
#endif

#define LIT 0x7dff3a
#define INK 0xd9e3d2
#define MUTED 0x8c9787
#define GREEN 0x2f7a22
#define GOLD 0xe6c15a

enum { T_NEWS, T_SHOP, T_MINE, T_PLAY, T_SCORES, T_APPLETS, NTABS };
static const char *TABS[NTABS] = { "NEWS", "GET GAMES", "MY VMX", "PLAY", "SCORES", "APPLETS" };
static struct {
  int tab, zone, sel[NTABS], t;                 // zone 0: the tab row, 1: the tab's content
  unsigned prev;
  int playing;                                  // PLAY: the pad drives the VMX
  int dl, dlT; uint32_t dlId, dlTo;             // a download in progress (frames left), to which VMX (serial; 0 = the hard disk)
  int choose, chooseSel;                        // the VMX chooser open: 1 = which VMX World uses, 2 = where a download goes
  int applet, appSel;                           // an applet open (1 FOLD BUILDER, 2 CASHIER)
  int menu, menuSel;                            // MY VMX's menu for a game
  char toast[64]; int toastT;
  int gamesDirty;                               // the hard disk changed: the VMX re-reads its games before PLAY
  uint32_t vmxMs, playSerial;                   // playSerial: the VMX PLAY is running (0 = the hard disk's)
  int dlBytes;
} W;
static uint8_t *vxpData[MAXVXP]; static int nVxpData;
static void storage_changed(void); static int count(int tab);
static int haveHolo;                                    // the skeleton hologram's data loaded (assets/vmx_holo.bin)

static void toast(const char *m) { snprintf(W.toast, sizeof W.toast, "%s", m); W.toastT = 75; }

// ---- small drawing helpers
static void card(int x, int y, int s, const uint8_t *c, uint32_t on, uint32_t off) {
  if (off == 0xff000000u) for (int j = 0; j < 24; j++) for (int i = 0; i < 24; i++)   // a glow under the picture
    if ((c[j * 3 + i / 8] >> (7 - i % 8)) & 1) add_rect(x + i * s - s, y + j * s - s, 3 * s, 3 * s, 0x020a04);
  for (int j = 0; j < 24; j++) for (int i = 0; i < 24; i++) {
    int b = (c[j * 3 + i / 8] >> (7 - i % 8)) & 1;
    if (b || off != 0xff000000u) gfx_rect(x + i * s, y + j * s, s, s, b ? on : off);
  }
}
static int wrap(int x, int y, int w, int scale, uint32_t c, const char *s, int maxLines) {   // returns lines used
  int cw = 6 * scale, per = w / cw, lines = 0; const char *p = s;
  while (*p && lines < maxLines) {
    int n = (int)strlen(p); if (n > per) { n = per; while (n > 0 && p[n] != ' ') n--; if (!n) n = per; }
    char line[120]; snprintf(line, sizeof line, "%.*s", n, p); gfx_text5(x, y + lines * 9 * scale, scale, c, line);
    lines++; p += n; while (*p == ' ') p++;
  }
  return lines;
}
static void upper(char *s) { for (; *s; s++) if (*s >= 'a' && *s <= 'z') *s -= 32; }


// the background (a dark green gradient and a grid floor), drawn once into its own picture and copied in each frame.
// (Not gfx_gradient: with the Xbox's GPU that goes under the picture instead of into it, and the VMX render's soft
// edges would blend with nothing.)
static uint32_t bgPx[GW * GH]; static int bgMade;
static void background(void) {
  if (!bgMade) {
    for (int y = 0; y < GH; y++) {
      int k = y * 256 / GH, r = (0x0b * (256 - k) + 0x02 * k) >> 8, g = (0x1a * (256 - k) + 0x06 * k) >> 8, bl = (0x0d * (256 - k) + 0x03 * k) >> 8;
      for (int x = 0; x < GW; x++) gfx_px[y * GW + x] = (uint32_t)(r << 16 | g << 8 | bl);
    }
    for (int k = -10; k <= 10; k++) gfx_line(320 + k * 24, 200, 320 + k * 120, 480, 0x10301a);   // the grid floor
    for (int j = 0, y = 210; y < 480; j++, y += 6 + j * 4) gfx_line(0, y, 639, y, 0x10301a);
    memcpy(bgPx, gfx_px, sizeof bgPx); bgMade = 1;
  }
  memcpy(gfx_px, bgPx, sizeof bgPx);
}
static void header(void) {
  gtext(40, 24, 3, 0xe8ffe8, "VMX"); gtext(40 + gfx_text5_w(3, "VMX "), 24, 3, HG, "WORLD");
  add_rect(40, 48, gfx_text5_w(3, "VMX WORLD"), 1, 0x1a5a28);
  char b[96], l[40], w[40]; int sel = vmxlink_selected(); vmxlink_label(sel, l, sizeof l); vmxlink_where(sel, w, sizeof w);
  if (sel >= 0) snprintf(b, sizeof b, "%s  %s  %d BLOCKS FREE", l, w, vmxlink_info(sel)->freeBlocks);
  else snprintf(b, sizeof b, "%s  ON THE HARD DISK", l);
  gfx_disc(600 - gfx_text5_w(1, b) - 12, 33, 4, LIT); gfx_text5(600 - gfx_text5_w(1, b), 30, 1, INK, b);
}
static void tabs(void) {                                 // projected tabs: slanted outlines; the open one lit
  int x = 32, y = 62, w = 96;
  for (int i = 0; i < 640; i++) gfx_rect(i, y + 27, 1, 1, mixc(0x000000, HG_EDGE, 255 - (i > 320 ? i - 320 : 320 - i) * 255 / 340));
  for (int i = 0; i < NTABS; i++, x += w + 2) {
    int on = i == W.tab;
    for (int j = 0; j < 26; j++) {
      int sx = x + (26 - j) / 3, sw = w - 8;
      if (on) add_rect(sx, y + j, sw, 1, (uint32_t)((4 + j / 3) << 16 | (30 + j * 2) << 8 | (10 + j)));
      gfx_rect(sx, y + j, 1, 1, on ? HG : HG_DIM); gfx_rect(sx + sw - 1, y + j, 1, 1, on ? HG : HG_DIM);
    }
    gfx_rect(x + 8, y, w - 8, 1, on ? HG : HG_DIM);
    if (on && W.zone == 0) brackets(x - 2, y - 3, w + 2, 32, HG, 6);
    if (on) gtext(x + 4 + (w - 8 - gfx_text5_w(1, TABS[i])) / 2, y + 10, 1, 0xffffff, TABS[i]);
    else gfx_text5(x + 4 + (w - 8 - gfx_text5_w(1, TABS[i])) / 2, y + 10, 1, MUTED, TABS[i]);
  }
}
static void hints(const char *a, const char *b, const char *y) {
  int x = 40, Y = 446;
  if (a) x += gfx_hint(x, Y, "A", a);
  if (b) x += gfx_hint(x, Y, "B", b);
  if (y) x += gfx_hint(x, Y, "Y", y);
  if (!W.playing && vmxlink_count() && !W.choose) x += gfx_hint(x, Y, "X", "CHOOSE VMX");
  if (!W.playing) { x += gfx_hint(x, Y, "L R", "TABS"); gfx_hint(x, Y, "BACK START", "DASHBOARD"); }
}
// a list of rows (left panel); returns nothing; row(i, x, y, w, selected) draws one
static void list(int x, int y, int w, int h, int n, int sel, void (*row)(int, int, int, int, int)) {
  hpanel(x, y, w, h, NULL, HG);
  int rh = 30, vis = (h - 12) / rh, top = sel - vis / 2; if (top > n - vis) top = n - vis; if (top < 0) top = 0;
  for (int i = top; i < n && i < top + vis; i++) {
    int ry = y + 6 + (i - top) * rh, s = i == sel && W.zone == 1;
    if (i == sel) hsel(x + 6, ry, w - 12, rh - 2, s);
    else gfx_rect(x + 12, ry + rh - 2, w - 24, 1, 0x0e2a16);    // a faint rule between rows
    row(i, x + 12, ry, w - 24, i == sel);
  }
  if (!n) gfx_text5(x + 16, y + 20, 1, MUTED, "NOTHING HERE YET");
}

// ---- NEWS
static const char *NEWS[][3] = {
  { "02 DEC 2002", "VMX CASINO OPENS", "Eighteen tables share one stash of chips: blackjack on the pier, 9-ball, video poker, roulette, slots, craps, keno and more. Win chips at one table and stake them at another, then cash them in at the CASINO CASHIER for money in your Xbox games." },
  { "21 NOV 2002", "FOLD ROOMS IS OUT", "Push boxes round rooms that wrap a shape you can't see. FOLD ROOMS brings the first World applet, FOLD BUILDER: build levels in 3D on your TV, then send them to your VMX." },
  { "15 NOV 2002", "SCORE LINES ARE OPEN", "Every VMX game keeps its best scores. Send them from SCORES here, or call them in from the VMX's own CALL menu when you're away from the Xbox." },
  { "15 NOV 2002", "WELCOME TO VMX WORLD", "GET GAMES puts new VMX games on your hard disk. MY VMX shows what you have. PLAY runs your VMX on the TV, big. Your VMX games are saves of VMX World: the dashboard's Memory screen shows them too." },
};
#define NNEWS 4
static void news_row(int i, int x, int y, int w, int sel) { gfx_text5(x, y + 3, 1, MUTED, NEWS[i][0]); gfx_text5(x, y + 14, 1, INK, NEWS[i][1]); }
static void draw_news(void) {
  list(32, 100, 230, 330, NNEWS, W.sel[T_NEWS], news_row);
  int i = W.sel[T_NEWS]; hpanel(276, 100, 332, 330, NEWS[i][1], LIT);
  gfx_text5(292, 142, 1, MUTED, NEWS[i][0]);
  char b[400]; snprintf(b, sizeof b, "%s", NEWS[i][2]); upper(b); wrap(292, 162, 300, 1, INK, b, 26);
  hints(NULL, "BACK", NULL);
}

// ---- GET GAMES
static void shop_row(int i, int x, int y, int w, int sel) {
  const Vxp *v = &CATALOG[i]; card(x, y + 2, 1, v->card, LIT, 0xff000000u);
  gfx_text5(x + 32, y + 4, 1, INK, v->title);
  const char *st = hdd_installed(v->id) >= 0 ? (hdd_on_vmx() ? "ON YOUR VMX" : "ON YOUR HARD DISK") : v->casino ? "VMX CASINO" : (v->id >> 12 & 15) == 8 ? "HOMEBREW" : "VMX STUDIO";
  gfx_text5(x + 32, y + 15, 1, hdd_installed(v->id) >= 0 ? LIT : MUTED, st);
}
static void draw_detail(const Vxp *v, int y0) {
  hpanel(276, 100, 332, 330, v->title, LIT);
  for (int k = 8; k > 0; k--) add_rect(296 + 48 - k * 6, 248 - k, k * 12, 2, 0x020c05);    // its projector, under it
  gfx_rect(296, 247, 96, 1, HG_EDGE);
  card(296, 144, 4, v->card, HG, 0xff000000u);
  char b[220]; snprintf(b, sizeof b, "%s", v->pub); upper(b); gfx_text5(404, 146, 1, MUTED, b);
  snprintf(b, sizeof b, "%d BLOCK%s", (v->bytes + 16383) / 16384, (v->bytes + 16383) / 16384 == 1 ? "" : "S"); gfx_text5(404, 160, 1, MUTED, b);
  snprintf(b, sizeof b, "ID %08X", (unsigned)v->id); gfx_text5(404, 174, 1, MUTED, b);
  if (v->casino) gfx_text5(404, 188, 1, GOLD, "VMX CASINO: SHARED CHIPS");
  snprintf(b, sizeof b, "%s", v->blurb); upper(b); wrap(292, 252, 300, 1, INK, b, 12);
  (void)y0;
}
static void draw_shop(void) {
  list(32, 100, 230, 330, NCATALOG, W.sel[T_SHOP], shop_row);
  if (!NCATALOG) { hpanel(276, 100, 332, 330, "NO CATALOGUE", LIT); wrap(292, 146, 300, 1, INK, "THIS DISC HAS NO GAMES IN CATALOG. ON THE REAL SERVICE THEY CAME OVER XBOX LIVE.", 6); hints(NULL, "BACK", NULL); return; }
  const Vxp *v = &CATALOG[W.sel[T_SHOP]]; draw_detail(v, 0);
  int have = hdd_installed(v->id) >= 0;
  if (W.dl && W.dlId == v->id) {
    int p = 100 - W.dl * 100 / W.dlT; gfx_frame(292, 392, 300, 16, LIT); gfx_rect(294, 394, 296 * p / 100, 12, LIT);
    gfx_text5(292, 378, 1, INK, "DOWNLOADING...");
  } else if (vmxlink_count()) gfx_text5(292, 392, 1, 0xffffff, have ? "ON THIS VMX.  A: DOWNLOAD TO ANOTHER" : "A: DOWNLOAD (CHOOSE WHERE)");
  else gfx_text5(292, 392, 1, have ? LIT : 0xffffff, have ? "ON YOUR HARD DISK" : "A: DOWNLOAD TO THE HARD DISK");
  hints(have && !vmxlink_count() ? NULL : "DOWNLOAD", "BACK", NULL);
}

// ---- MY VMX
static void mine_row(int i, int x, int y, int w, int sel) {
  const Vxp *v = &INSTALLED[i]; card(x, y + 2, 1, v->card, LIT, 0xff000000u);
  gfx_text5(x + 32, y + 4, 1, INK, v->title);
  char b[40]; snprintf(b, sizeof b, "%d BLOCK%s", (v->bytes + 16383) / 16384, (v->bytes + 16383) / 16384 == 1 ? "" : "S"); gfx_text5(x + 32, y + 15, 1, MUTED, b);
}
static void draw_mine(void) {
  list(32, 100, 230, 330, NINSTALLED, W.sel[T_MINE], mine_row);
  if (!NINSTALLED) { hpanel(276, 100, 332, 330, "MY VMX", LIT); wrap(292, 146, 300, 1, INK, hdd_on_vmx() ? "NO GAMES ON YOUR VMX YET. GET SOME IN GET GAMES." : "NO VMX GAMES ON THE HARD DISK YET. GET SOME IN GET GAMES. OR PLUG YOUR VMX INTO A CONTROLLER.", 6); hints(NULL, "BACK", NULL); return; }
  draw_detail(&INSTALLED[W.sel[T_MINE]], 0);
  char b[80]; snprintf(b, sizeof b, hdd_on_vmx() ? "VMX: 56580001\\%08X0000" : "UDATA\\56580001\\%08X0000", (unsigned)INSTALLED[W.sel[T_MINE]].id); gfx_text5(292, 378, 1, MUTED, b);
  gfx_text5(292, 392, 1, 0xffffff, "A: PLAY OR DELETE");
  if (W.menu) {
    gfx_dim(0, 0, 640, 480, 0.95f); hpanel(200, 180, 240, 110, INSTALLED[W.sel[T_MINE]].title, HG);
    const char *items[2] = { "PLAY ON THE TV", hdd_on_vmx() ? "DELETE FROM YOUR VMX" : "DELETE FROM THE HARD DISK" };
    for (int i = 0; i < 2; i++) { int y = 222 + i * 26; if (i == W.menuSel) gfx_rect(212, y - 6, 216, 20, GREEN); gfx_text5(222, y, 1, INK, items[i]); }
    hints("SELECT", "CANCEL", NULL); return;
  }
  hints("PLAY / DELETE", "BACK", NULL);
}

// ---- PLAY: the VMX firmware on the TV
typedef struct { int w, h, sx, sy; uint8_t *px; } Frame;
static Frame heads[2]; static int headTried;              // the owner's VMX renders: small (screen 2x), large (3x)
static void load_heads(void) {
  headTried = 1;
#ifdef NXDK
  const char *names[2] = { "D:\\assets\\vmx_small.rgba", "D:\\assets\\vmx_large.rgba" };
#else
  const char *names[2] = { "build/disc/assets/vmx_small.rgba", "build/disc/assets/vmx_large.rgba" };
#endif
  for (int k = 0; k < 2; k++) {
    int n = 0; uint8_t *d = hdd_read(names[k], &n); Frame *F = &heads[k];
    if (d && n > 12 && !memcmp(d, "VMXF", 4)) {
      F->w = d[4] | d[5] << 8; F->h = d[6] | d[7] << 8; F->sx = d[8] | d[9] << 8; F->sy = d[10] | d[11] << 8;
      if (n >= 12 + F->w * F->h * 4) { F->px = malloc((size_t)F->w * F->h * 4); memcpy(F->px, d + 12, (size_t)F->w * F->h * 4); }
    }
    free(d);
  }
}
static void draw_vmx(int x, int y, int sc) {             // the VMX with its screen's top-left at (x, y), sc 2 or 3
  if (!headTried) load_heads();
  Frame head = heads[sc >= 3];
  if (head.px) {
    int ox = x - head.sx, oy = y - head.sy;
    for (int j = 0; j < head.h; j++) for (int i = 0; i < head.w; i++) {
      int X = ox + i, Y = oy + j; if (X < 0 || Y < 0 || X >= GW || Y >= GH) continue;
      const uint8_t *q = head.px + 4 * (j * head.w + i); unsigned a = q[3]; if (!a) continue;
      uint32_t d = gfx_px[Y * GW + X];
      gfx_px[Y * GW + X] = (q[0] * a + ((d >> 16) & 255) * (255 - a)) / 255 << 16 | (q[1] * a + ((d >> 8) & 255) * (255 - a)) / 255 << 8 | (q[2] * a + (d & 255) * (255 - a)) / 255;
    }
  } else { gfx_rect(x - 20, y - 20, 64 * sc + 40, 64 * sc + 60, 0x2a2d2a); }
  const uint8_t *s = vmx_screen(); int on = vmx_on();
  for (int j = 0; j < 64; j++) for (int i = 0; i < 64; i++) gfx_rect(x + i * sc, y + j * sc, sc, sc, on && s[j * 64 + i] ? 0x7dff3a : 0x060a05);
}
static void vmx_reload(void) {                         // the VMX gets the hard disk's games
  LOG("clear"); vmx_clear_games(); LOG("cleared");
  for (int i = 0; i < nVxpData; i++) free(vxpData[i]);
  nVxpData = 0;
  for (int i = 0; i < NINSTALLED; i++) {
    int n = 0; uint8_t *d = hdd_read(INSTALLED[i].path, &n);
    if (d && !vmx_add_game(d, (size_t)n)) vxpData[nVxpData++] = d; else free(d);
  }
  vmx_init(); W.gamesDirty = 0;
}
static void play_row(int i, int x, int y, int w, int sel) { mine_row(i, x, y, w, sel); }
static void draw_play(void) {
  if (!W.playing) list(32, 100, 230, 330, NINSTALLED, W.sel[T_PLAY], play_row);
  if (W.playing) {
    if (haveHolo) holo_draw(HOLO_PLAY, W.t, vmx_screen(), vmx_on()); else draw_vmx(224, 189, 3);
    const char *m = "D-PAD, A AND B ARE THE VMX'S. BACK IS ITS SIDE BUTTON.";
    gfx_text5(320 - gfx_text5_w(1, m) / 2, 418, 1, MUTED, m);
    hints(NULL, NULL, "BACK TO WORLD");
  } else {
    hpanel(276, 100, 332, 330, "YOUR VMX", LIT);
    if (haveHolo) holo_draw(HOLO_LIST, W.t, vmx_screen(), vmx_on()); else draw_vmx(378, 196, 2);
    wrap(292, 372, 300, 1, MUTED, "THE VMX RUNS ON THE TV WITH EVERY GAME ON THE HARD DISK. ITS SAVES, SCORES AND CHIPS STAY ON THE HARD DISK.", 4);
    hints("TURN ON THE VMX", "BACK", NULL);
  }
}

// ---- SCORES
static VmxRec recs[MAXVXP]; static int haveRec[MAXVXP]; static VmxBank bank; static int haveBank;
static void load_scores(void) {                        // each game's record from its save folder ("r<ID>"), the shared chips
  for (int i = 0; i < NINSTALLED; i++) { char k[12]; snprintf(k, sizeof k, "r%08x", (unsigned)INSTALLED[i].id); haveRec[i] = plat_load(k, &recs[i], sizeof recs[i]) == (int)sizeof recs[i]; }
  haveBank = plat_load("casino", &bank, sizeof bank) == (int)sizeof bank && bank.magic == 0x42435856u;
}
static const VmxRec *rec_of(uint32_t id) { int i = hdd_installed(id); return i >= 0 && haveRec[i] ? &recs[i] : NULL; }
static int nscores(void) { return NINSTALLED + 1; }
static void scores_row(int i, int x, int y, int w, int sel) {
  if (i == NINSTALLED) { gfx_text5(x, y + 4, 1, GOLD, "VMX CASINO CHIPS"); char b[32]; snprintf(b, sizeof b, "%d", haveBank ? (int)bank.chips : 100); gfx_text5(x, y + 15, 1, MUTED, b); return; }
  const Vxp *v = &INSTALLED[i]; card(x, y + 2, 1, v->card, LIT, 0xff000000u); gfx_text5(x + 32, y + 4, 1, INK, v->title);
  const VmxRec *r = rec_of(v->id); char b[40];
  if (r && (r->best || r->w || r->l)) snprintf(b, sizeof b, "BEST %d", (int)(r->w || r->l ? r->w : r->best)); else snprintf(b, sizeof b, "NO SCORE YET");
  gfx_text5(x + 32, y + 15, 1, MUTED, b);
}
static void draw_scores(void) {
  list(32, 100, 230, 330, nscores(), W.sel[T_SCORES], scores_row);
  int i = W.sel[T_SCORES]; char b[80];
  if (i == NINSTALLED) {
    hpanel(276, 100, 332, 330, "VMX CASINO CHIPS", GOLD);
    int chips = haveBank ? bank.chips : 100;
    snprintf(b, sizeof b, "%d", chips); gfx_text5(292, 150, 4, GOLD, b); gfx_text5(292 + gfx_text5_w(4, b) + 10, 168, 1, MUTED, "CHIPS");
    snprintf(b, sizeof b, "MOST EVER HELD  %d", haveBank ? (int)bank.peak : 100); gfx_text5(292, 210, 1, INK, b);
    snprintf(b, sizeof b, "CASHED OUT      %d", haveBank ? (int)bank.cashed : 0); gfx_text5(292, 226, 1, INK, b);
    snprintf(b, sizeof b, "STAKED / PAID   %d / %d", haveBank ? (int)bank.staked : 0, haveBank ? (int)bank.paid : 0); gfx_text5(292, 242, 1, INK, b);
    wrap(292, 280, 300, 1, MUTED, "SCORE RECORD 56581F00. THE NATIONAL TABLES NEED THE SCORE SERVICE (NOT RUNNING YET).", 5);
  } else if (NINSTALLED) {
    const Vxp *v = &INSTALLED[i]; hpanel(276, 100, 332, 330, v->title, LIT);
    const VmxRec *r = rec_of(v->id);
    if (!r || !(r->best || r->w || r->l || r->d)) gfx_text5(292, 150, 1, MUTED, "NO SCORE YET: PLAY IT IN PLAY");
    else if (r->w || r->l || r->d) { snprintf(b, sizeof b, "WON %d  LOST %d  DRAWN %d", (int)r->w, (int)r->l, (int)r->d); gfx_text5(292, 150, 2, INK, b); }
    else { snprintf(b, sizeof b, "%d", (int)r->best); gfx_text5(292, 146, 4, LIT, b); gfx_text5(292, 186, 1, MUTED, "YOUR BEST"); }
    if (r && r->day) { snprintf(b, sizeof b, "SET ON DAY %d", r->day); gfx_text5(292, 206, 1, MUTED, b); }
    wrap(292, 280, 300, 1, MUTED, "SEND ONLINE NEEDS THE SCORE SERVICE (NOT RUNNING YET). THE VMX CAN STILL CALL ITS SCORES IN FROM ITS CALL MENU.", 5);
  }
  hints(NULL, "BACK", NULL);
}

// ---- APPLETS
#ifdef NXDK
#define TDATA "E:\\TDATA\\56580001\\$c\\"
#define SEP_ "\\"
#define GAMES_FOLD "E:\\Games\\FOLD\\default.xbe"
#else
#define TDATA "build/hdd/TDATA/56580001/$c/"
#define SEP_ "/"
#define GAMES_FOLD "build/hdd/Games/FOLD/default.xbe"
#endif
// applets are World's downloadable content on the hard disk (docs/DESIGN.md): TDATA\56580001\$c\<offer>\, the offer ID
// being World's title ID + the VMX game's ID, with ContentMeta.xbx naming it for the dashboard. FOLD BUILDER also
// runs from its own install (E:\Games\FOLD, FOLD's network loader) until it's offered as content.
#define FOLD_CONTENT TDATA "5658000156581010" SEP_ "default.xbe"
static const char *fold_xbe(void) { return hdd_exists(FOLD_CONTENT) ? FOLD_CONTENT : hdd_exists(GAMES_FOLD) ? GAMES_FOLD : NULL; }
#define FOLD_XBE fold_xbe()
static const struct { const char *name, *tid; uint32_t id; int per; const char *sym, *unit; int ready; } CASH[] = {
  { "GRAND THEFT AUTO III", "5454000E", 0x5454000E, 100, "$", "", 0 },
  { "GTA: VICE CITY", "5454000F", 0x5454000F, 250, "$", "", 0 },
  { "GTA: SAN ANDREAS", "54540082", 0x54540082, 250, "$", "", 0 },
  { "STAR WARS: KOTOR", "4C410003", 0x4C410003, 10, "", " CREDITS", 0 },
  { "SHENMUE II", "4D530032", 0x4D530032, 2, "HK$", "", 0 } };
#define NCASH 5
static void applet_row(int i, int x, int y, int w, int sel) {
  gfx_text5(x, y + 4, 1, INK, i == 0 ? "FOLD BUILDER" : "VMX CASINO CASHIER");
  gfx_text5(x, y + 15, 1, MUTED, i == 0 ? (FOLD_XBE ? "ON THE HARD DISK" : "NOT INSTALLED") : "FOR CASINO CHIPS");
}
static void draw_cashier(void) {
  gfx_dim(0, 0, 640, 480, 0.97f);
  hpanel(40, 70, 560, 360, "VMX CASINO - CASHIER", GOLD);
  char b[96]; snprintf(b, sizeof b, "%d", haveBank ? (int)bank.chips : 100);
  gfx_text5(60, 116, 1, MUTED, "YOUR STASH"); gfx_text5(60, 128, 3, GOLD, b); gfx_text5(64 + gfx_text5_w(3, b), 142, 1, MUTED, "CHIPS");
  gfx_text5(60, 172, 1, MUTED, "XBOX GAMES THE CASHIER KNOWS");
  for (int i = 0; i < NCASH; i++) {
    int y = 190 + i * 30, n = hdd_title_saves(CASH[i].id);
    if (i == W.appSel) hsel(56, y - 6, 300, 26, 1);
    gfx_text5(64, y, 1, INK, CASH[i].name);
    snprintf(b, sizeof b, n ? "%d SAVE%s ON THIS XBOX" : "NOT ON THIS XBOX", n, n == 1 ? "" : "S"); gfx_text5(64, y + 10, 1, n ? LIT : MUTED, b);
  }
  int i = W.appSel;
  glass(372, 186, 212, 200); add_rect(372, 186, 212, 200, 0x0c0800); brackets(372, 186, 212, 200, GOLD, 10);   // an amber slip
  gtext(382, 196, 1, GOLD, CASH[i].name);
  snprintf(b, sizeof b, "1 CHIP = %s%d%s", CASH[i].sym, CASH[i].per, CASH[i].unit); gfx_text5(382, 214, 1, 0xf2dc9a, b);
  snprintf(b, sizeof b, "UDATA\\%s", CASH[i].tid); gfx_text5(382, 230, 1, 0x9a8650, b);
  wrap(382, 256, 196, 1, 0xc8b47a, "EDITING THIS GAME'S SAVE ISN'T READY YET: THE SAVE TOOL COMES FIRST ON THE PC (DOCS/CASINO.MD). YOUR CHIPS STAY ON YOUR VMX.", 10);
  hints(NULL, "CLOSE", NULL);
}
static void draw_applets(void) {
  list(32, 100, 230, 330, 2, W.sel[T_APPLETS], applet_row);
  if (W.sel[T_APPLETS] == 0) {
    hpanel(276, 100, 332, 330, "FOLD BUILDER", LIT);
    wrap(292, 146, 300, 1, INK, "BUILD FOLD ROOMS LEVELS IN 3D: FOLD THE WORKING PLANE ROUND THE SHAPE, PLACE WALLS, BOXES AND TARGETS, WIRE UP SWITCHES. COMES WITH FOLD ROOMS.", 10);
    gfx_text5(292, 392, 1, FOLD_XBE ? 0xffffff : MUTED, FOLD_XBE ? "A: START FOLD BUILDER" : "NOT ON THIS XBOX");
  } else {
    hpanel(276, 100, 332, 330, "VMX CASINO CASHIER", GOLD);
    wrap(292, 146, 300, 1, INK, "CASH YOUR VMX CASINO CHIPS IN FOR MONEY IN YOUR XBOX GAMES. EACH GAME HAS ITS OWN RATE. CHIPS CASHED OUT LEAVE YOUR STASH FOR GOOD.", 10);
    gfx_text5(292, 392, 1, 0xffffff, "A: OPEN THE CASHIER");
  }
  if (W.applet == 2) { draw_cashier(); return; }
  hints("START", "BACK", NULL);
}

// ---- which VMX: the chooser (X, and when a download could go to more than one place)
static int dev_of(uint32_t serial) { for (int i = 0; i < vmxlink_count(); i++) if (vmxlink_info(i)->sys.serial == serial) return i; return -1; }
static void storage_changed(void) {                    // another VMX (or the hard disk): its games, saves and chips
  hdd_scan(); vmx_reload(); load_scores();
  for (int t = 0; t < NTABS; t++) if (W.sel[t] >= count(t)) W.sel[t] = count(t) ? count(t) - 1 : 0;
}
static void start_play(void) { W.playing = 1; W.playSerial = vmxlink_selected() >= 0 ? vmxlink_info(vmxlink_selected())->sys.serial : 0; }
static void draw_chooser(void) {
  int rows = vmxlink_count() + 1, h = 70 + rows * 36, x = 130, y = 240 - h / 2, w = 380;
  gfx_dim(0, 0, 640, 480, 0.95f); hpanel(x, y, w, h, W.choose == 2 ? "DOWNLOAD TO" : "CHOOSE A VMX", HG);
  for (int r = 0; r < rows; r++) {
    int dev = r < vmxlink_count() ? r : -1, ry = y + 44 + r * 36, on = r == W.chooseSel;
    char l[40], wh[40], b[80], t[10]; vmxlink_label(dev, l, sizeof l); vmxlink_where(dev, wh, sizeof wh);
    const VmxInfo *v = vmxlink_info(dev);
    int bad = W.choose == 2 && (hdd_has_game(dev, W.dlId) || !hdd_room(dev, W.dlBytes));
    if (on) hsel(x + 10, ry - 4, w - 20, 32, 1);
    gfx_text5(x + 22, ry, 1, bad ? MUTED : on ? 0xffffff : INK, l);
    if (v) { vol_serial_text(v->sys.serial, t); gfx_text5(x + w - 22 - gfx_text5_w(1, t), ry, 1, MUTED, t);
             snprintf(b, sizeof b, "%s  %d GAME%s  %d BLOCKS FREE", wh, v->games, v->games == 1 ? "" : "S", v->freeBlocks); }
    else { int g = hdd_games_on(-1); snprintf(b, sizeof b, "%s  %d GAME%s", wh, g, g == 1 ? "" : "S"); }
    if (W.choose == 2 && hdd_has_game(dev, W.dlId)) snprintf(b, sizeof b, "%s  ALREADY HAS IT", wh);
    else if (W.choose == 2 && !hdd_room(dev, W.dlBytes)) snprintf(b, sizeof b, "%s  NOT ENOUGH ROOM", wh);
    gfx_text5(x + 22, ry + 12, 1, MUTED, b);
    if (dev == vmxlink_selected() && W.choose == 1) gfx_text5(x + w - 22 - gfx_text5_w(1, "IN USE"), ry + 12, 1, HG, "IN USE");
  }
  hints(W.choose == 2 ? "DOWNLOAD HERE" : "USE THIS ONE", "CANCEL", NULL);
}

// ---- app.h
void app_init(const char *levelsDir) {
  (void)levelsDir;
  memset(&W, 0, sizeof W);
#ifdef NXDK
  haveHolo = holo_load_baked("D:\\assets\\vmx_holo.pak") || holo_load("D:\\assets\\vmx_holo.bin");
#else
  haveHolo = holo_load_baked("build/disc/assets/vmx_holo.pak") || holo_load("build/disc/assets/vmx_holo.bin");
#endif
  LOG("init"); hdd_init(); LOG("hdd ok"); vmx_reload(); LOG("vmx ok"); load_scores(); LOG("ready");
  // the brushed metal FOLD's panels use (not needed for World's own look, but the font and panels come from gfx.c)
}
int app_quit(void) {                                   // Back + Start in World's menus: back to the dashboard
  return !W.playing && (W.prev & (PAD_BACK | PAD_START)) == (PAD_BACK | PAD_START);   // (BACK START hint)
}
void app_usb_init(void) { vmxlink_init(); }        // the VMX in a controller: USB mass storage
int app_load(const char *path) { (void)path; return 0; }
int app_save(const char *path) { (void)path; return 0; }
int app_state(char *b, int n) {
  return snprintf(b, (size_t)n, "tab %d zone %d sel %d playing %d applet %d installed %d", W.tab, W.zone, W.sel[W.tab], W.playing, W.applet, NINSTALLED);
}
static int count(int tab) {
  switch (tab) { case T_NEWS: return NNEWS; case T_SHOP: return NCATALOG; case T_MINE: case T_PLAY: return NINSTALLED;
    case T_SCORES: return nscores(); case T_APPLETS: return 2; }
  return 0;
}
void app_frame(const Pad *pad) {
  unsigned held = pad->held;
  if (pad->ly < -0.6f) held |= PAD_UP; if (pad->ly > 0.6f) held |= PAD_DOWN;
  if (pad->lx < -0.6f) held |= PAD_LEFT; if (pad->lx > 0.6f) held |= PAD_RIGHT;
  unsigned e = held & ~W.prev; W.prev = held; W.t++;
  holo_prepare();                                     // the hologram's pictures, one a tick until ready
  if (vmxlink_poll()) {                               // a VMX came or went
    int was = W.playing; uint32_t serial = W.playSerial;
    if (W.playing && (vmxlink_selected() < 0 ? 0 : vmxlink_info(vmxlink_selected())->sys.serial) != serial) W.playing = 0;   // its VMX went
    if (!W.playing) { W.menu = 0; W.choose = 0; storage_changed(); } else (void)was;
    toast(vmxlink_event());
  }
  if (W.toastT) W.toastT--;
  if (W.dl && --W.dl == 0) {                           // a download finished: onto the VMX it was meant for
    int to = dev_of(W.dlTo), was = vmxlink_selected(); char l[40]; vmxlink_label(to, l, sizeof l);
    if (W.dlTo && to < 0) toast("THAT VMX WAS UNPLUGGED");
    else for (int i = 0; i < NCATALOG; i++) if (CATALOG[i].id == W.dlId) {
      vmxlink_select(to); int ok = hdd_install(&CATALOG[i]); vmxlink_refresh(); vmxlink_select(was); hdd_scan();
      char b[64]; snprintf(b, sizeof b, ok ? "DOWNLOADED TO %s" : "COULDN'T WRITE %s", l); toast(b);
      if (to == was) W.gamesDirty = 1;
    }
  }
  if (W.playing) {                                     // the VMX has the pad; Y gives it back to World
    int b = 0;
    if (held & PAD_UP) b |= B_UP; if (held & PAD_DOWN) b |= B_DOWN; if (held & PAD_LEFT) b |= B_LEFT; if (held & PAD_RIGHT) b |= B_RIGHT;
    if (held & PAD_A) b |= B_A; if (held & PAD_B) b |= B_B; if (held & PAD_BACK) b |= B_SIDE;
    W.vmxMs += 33; vmx_frame(W.vmxMs, b);
    if (e & PAD_Y) { vmx_save_now(); W.playing = 0; load_scores(); }
    return;
  }
  if (W.choose) {                                      // the VMX chooser: the VMXs, then the hard disk
    int rows = vmxlink_count() + 1;
    if (e & PAD_UP) W.chooseSel = (W.chooseSel + rows - 1) % rows;
    if (e & PAD_DOWN) W.chooseSel = (W.chooseSel + 1) % rows;
    if (e & (PAD_B | PAD_X)) W.choose = 0;
    if (e & PAD_A) {
      int dev = W.chooseSel < vmxlink_count() ? W.chooseSel : -1;
      if (W.choose == 1) { W.choose = 0; if (dev != vmxlink_selected()) { vmx_save_now(); vmxlink_select(dev); storage_changed(); char l[40], b[64]; vmxlink_label(dev, l, sizeof l); snprintf(b, sizeof b, "USING %s", l); toast(b); } }
      else if (!hdd_has_game(dev, W.dlId) && hdd_room(dev, W.dlBytes)) {
        W.choose = 0; W.dlTo = dev >= 0 ? vmxlink_info(dev)->sys.serial : 0; W.dlT = W.dl = 30 + W.dlBytes / 1200;
      }
    }
    return;
  }
  if (W.applet == 2) {
    if (e & PAD_UP) W.appSel = (W.appSel + NCASH - 1) % NCASH;
    if (e & PAD_DOWN) W.appSel = (W.appSel + 1) % NCASH;
    if (e & (PAD_B | PAD_A)) W.applet = 0;
    return;
  }
  if (W.menu) {
    if (e & (PAD_UP | PAD_DOWN)) W.menuSel ^= 1;
    if (e & PAD_B) W.menu = 0;
    if (e & PAD_A) {
      W.menu = 0;
      if (W.menuSel == 0) { W.tab = T_PLAY; W.sel[T_PLAY] = W.sel[T_MINE]; if (W.gamesDirty) vmx_reload(); start_play(); }
      else if (hdd_remove(INSTALLED[W.sel[T_MINE]].id)) { vmxlink_refresh(); toast("DELETED"); W.gamesDirty = 1; if (W.sel[T_MINE] >= NINSTALLED && W.sel[T_MINE]) W.sel[T_MINE]--; }
    }
    return;
  }
  if ((e & PAD_X) && vmxlink_count()) { W.choose = 1; W.chooseSel = vmxlink_selected() >= 0 ? vmxlink_selected() : vmxlink_count(); return; }
  if (e & (PAD_L | PAD_WHITE)) { W.tab = (W.tab + NTABS - 1) % NTABS; W.zone = 0; }
  if (e & (PAD_R | PAD_BLACK)) { W.tab = (W.tab + 1) % NTABS; W.zone = 0; }
  if (W.zone == 0) {
    if (e & PAD_LEFT) W.tab = (W.tab + NTABS - 1) % NTABS;
    if (e & PAD_RIGHT) W.tab = (W.tab + 1) % NTABS;
    if (e & (PAD_A | PAD_DOWN)) { W.zone = 1; if (W.tab == T_SCORES) load_scores(); }
    return;
  }
  int n = count(W.tab), *s = &W.sel[W.tab];
  if (e & PAD_B) { W.zone = 0; return; }
  if (e & PAD_UP) { if (*s == 0) W.zone = 0; else (*s)--; }
  if ((e & PAD_DOWN) && *s < n - 1) (*s)++;
  if (!(e & PAD_A)) return;
  switch (W.tab) {
    case T_SHOP:
      if (n && !W.dl) {                                 // where to: asked when there's more than one place
        W.dlId = CATALOG[*s].id; W.dlBytes = CATALOG[*s].bytes;
        if (vmxlink_count()) { W.choose = 2; W.chooseSel = vmxlink_selected() >= 0 ? vmxlink_selected() : vmxlink_count(); }
        else if (hdd_installed(W.dlId) < 0) { W.dlTo = 0; W.dlT = W.dl = 30 + W.dlBytes / 1200; }
      }
      break;
    case T_MINE: if (n) { W.menu = 1; W.menuSel = 0; } break;
    case T_PLAY: if (W.gamesDirty) vmx_reload(); start_play(); break;
    case T_APPLETS:
      if (*s == 1) { W.applet = 2; W.appSel = 0; load_scores(); }
      else if (FOLD_XBE) {
#ifdef NXDK
        fold_launch(FOLD_XBE);                         // (stops USB and the GPU first: FOLD's main_xbox.c)
#else
        toast("WOULD START FOLD BUILDER");
#endif
      } else toast("FOLD BUILDER ISN'T ON THIS XBOX");
      break;
  }
}
void app_draw(void) {
  background(); header(); tabs();
  switch (W.tab) {
    case T_NEWS: draw_news(); break; case T_SHOP: draw_shop(); break; case T_MINE: draw_mine(); break;
    case T_PLAY: draw_play(); break; case T_SCORES: draw_scores(); break; case T_APPLETS: draw_applets(); break;
  }
  if (W.choose) draw_chooser();
  if (W.toastT) { int w = gfx_text5_w(1, W.toast) + 28, x = 330 - w / 2; glass(x, 22, w, 22); brackets(x, 22, w, 22, HG, 6); gtext(x + 14, 29, 1, HG, W.toast); }
  post(W.t);
}
