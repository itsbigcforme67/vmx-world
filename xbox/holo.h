// The VMX as a skeleton hologram (owner 2026-10-23: World's look for VMX things on the Xbox side; the preview is
// tools/hologram_preview.py, the data tools/hologram_export.py -> assets/vmx_holo.bin). Drawn additively over
// gfx_px: see-through glowing faces (brighter at grazing angles), the parts' edges, the glass hiding the insides,
// bloom, scanlines, a rolling band and a projector beam, with the VMX's live 64x64 picture on its screen.
#ifndef HOLO_H
#define HOLO_H
#include <stdint.h>
enum { HOLO_LIST, HOLO_PLAY };            // the PLAY tab's small turning VMX / the big one while you play
int  holo_load_baked(const char *path);   // the pictures made ahead of time (assets/vmx_holo.pak); 1 = loaded
int  holo_load(const char *path);         // or the model itself (assets/vmx_holo.bin), rendered here; 1 = loaded
int  holo_bake(const char *out);          // PC only: renders every picture and writes the .pak
void holo_draw(int view, int t, const uint8_t *screen, int screenOn);   // t: World's 30 Hz tick count
void holo_prepare(void);                  // call every tick: renders the next cached picture until all are ready
#endif
