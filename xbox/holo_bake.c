// Makes assets/vmx_holo.pak: every picture of the VMX hologram (holo.c) rendered ahead of time on the PC from
// assets/vmx_holo.bin (tools/hologram_export.py), so World on the Xbox only loads them.
//   make -f Makefile.pc holo     (run again after the model, the look or the views' layout change)
#include "holo.h"
#include <stdio.h>
int main(void) {
  if (!holo_load("assets/vmx_holo.bin")) { printf("no assets/vmx_holo.bin (tools/hologram_export.py)\n"); return 1; }
  return holo_bake("assets/vmx_holo.pak") ? 0 : 1;
}
