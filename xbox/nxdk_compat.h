// Force-included into VMX World's Xbox build (Makefile): fixes for nxdk's C library found on the way.
// lroundf never returns on nxdk (seen in xemu: the VMX firmware's menus hung in s_line); this is the C standard's
// rounding (halves away from zero) for the floats the firmware passes, which are small.
#ifndef NXDK_COMPAT_H
#define NXDK_COMPAT_H
#include <math.h>
static inline long world_lroundf(float x) { return (long)(x < 0 ? x - 0.5f : x + 0.5f); }
#define lroundf world_lroundf
#endif
