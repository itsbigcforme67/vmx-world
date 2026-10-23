// The VMX in a controller (or any USB port): an Xbox memory unit is USB mass storage on the controller's hub, so
// nxdk's USB host stack finds it (usbh_umas_*), and its FATX volume (vmx-firmware/core/fatx.c, laid out as
// core/vmxvol.h) is read and written here. A memory unit counts as a VMX when it has 56580001\VMX (the VMX makes it).
// On the PC build the "VMX" is an image file, build/vmx.mu, if it's there (tests).
#ifndef VMXLINK_H
#define VMXLINK_H
#include "../../vmx-firmware/core/fatx.h"
void  vmxlink_init(void);       // before SDL_Init on the Xbox (world.c's app_usb_init)
int   vmxlink_poll(void);       // every tick: 1 when a VMX came or went (World re-reads its lists)
Fatx *vmxlink_vol(void);        // the VMX's volume, or NULL
int   vmxlink_other(void);      // a memory unit that isn't a VMX is plugged in
#endif
