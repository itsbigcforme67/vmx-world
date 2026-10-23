// Makes assets/vmx_holo.pak: every picture of the VMX hologram (holo.c) rendered ahead of time on the PC from
// assets/vmx_holo.bin (tools/hologram_export.py), so World on the Xbox only loads them,
// and FOLD's head-on VMX pictures.
//   make -f Makefile.pc holo     (run again after the model, the look or the views' layout change)
#include "holo.h"
#include <stdio.h>
int main(void) {
  if (!holo_load("assets/vmx_holo.bin")) { printf("no assets/vmx_holo.bin (tools/hologram_export.py)\n"); return 1; }
  if (!holo_bake("assets/vmx_holo.pak")) return 1;
  // FOLD's VMX window pictures (World's fallback too): as tools/make_assets.py cut the render (small: round the screen)
  return holo_bake_fold("../../microgames/fold/assets/vmx_small.rgba", 2, 26, 16) &&
         holo_bake_fold("../../microgames/fold/assets/vmx_large.rgba", 3, -1, 26) ? 0 : 1;
}
