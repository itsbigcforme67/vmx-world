// The VMXs in the controllers (or any USB port): an Xbox memory unit is USB mass storage on the controller's hub, so
// nxdk's USB host stack finds them (usbh_umas_*), and each one's FATX volume (vmx-firmware/core/fatx.c, laid out as
// core/vmxvol.h) is read and written here. Up to 8 at once (4 controllers x 2 slots). A memory unit is a VMX when it
// has a valid 56580001\VMX.SYS (its serial and owner's name); other memory units are counted, not used.
// One is "selected": World's MY VMX, PLAY, SCORES and the cashier use it; none selected = World's own VMX on the hard
// disk. On the PC build the VMXs are image files, build/vmx.mu and build/vmx2.mu .. build/vmx8.mu (tests).
#ifndef VMXLINK_H
#define VMXLINK_H
#include "../../vmx-firmware/core/fatx.h"
#include "../../vmx-firmware/core/vmxvol.h"
#define MAXVMX 8
typedef struct {
  VmxSys sys;                   // serial, owner's name
  int port, slot;               // controller 1-4 and slot 0 (A) / 1 (B); 0 / -1 if not in a controller
  int games, freeBlocks;        // what's on it (refreshed by vmxlink_refresh)
} VmxInfo;
void  vmxlink_init(void);       // before SDL_Init on the Xbox (world.c's app_usb_init)
int   vmxlink_poll(void);       // every tick: 1 when a VMX came or went (World re-reads its lists)
int   vmxlink_count(void);      // VMXs plugged in
const VmxInfo *vmxlink_info(int i);
int   vmxlink_selected(void);   // which is selected, -1 = the hard disk
void  vmxlink_select(int i);    // -1 = the hard disk
Fatx *vmxlink_vol(void);        // the selected VMX's volume, or NULL (the hard disk)
Fatx *vmxlink_vol_of(int i);    // VMX i's volume (NULL for -1 or none)
void  vmxlink_refresh(void);    // re-count the selected one's games and free blocks (after writing)
int   vmxlink_others(void);     // memory units plugged in that aren't VMXs
const char *vmxlink_event(void);   // what the last poll change was, for a toast ("JAM'S VMX PLUGGED IN")
void  vmxlink_label(int i, char *out, int n);   // "JAM'S VMX", "VMX 1A2B-3C4D", or "WORLD'S VMX" for -1
void  vmxlink_where(int i, char *out, int n);   // "CONTROLLER 2 SLOT A", "HARD DISK"
#endif
