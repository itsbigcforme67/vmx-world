// VMX World's files (docs/DESIGN.md "Where things live"): the World disc's catalogue of GAME.VXP files, the VMX games
// installed on the hard disk under UDATA\56580001 (one save folder per game, as the Xbox dashboard expects), and the
// VMX's own storage (its settings, records and casino chips: the VMX firmware's plat_load / plat_save keys).
// On the Xbox: the disc is D:\ (the XBE's folder), the hard disk E:\. On the PC: build/disc/ and build/hdd/.
#ifndef HDD_H
#define HDD_H
#include <stdint.h>

typedef struct {
  uint32_t id; int bytes, casino;
  char title[24], pub[32], l1[8], l2[8], blurb[200], path[220];
  uint8_t card[72];                                   // 24 x 24, 1 bit, rows top first, MSB left
} Vxp;

#define MAXVXP 48
extern Vxp CATALOG[MAXVXP]; extern int NCATALOG;     // on the disc (GET GAMES)
extern Vxp INSTALLED[MAXVXP]; extern int NINSTALLED; // on the VMX if one is plugged in, else the hard disk (MY VMX, PLAY)

void hdd_init(void);                                 // mounts E: (Xbox), makes UDATA\56580001 with its TitleMeta
void hdd_scan(void);                                 // re-reads CATALOG and INSTALLED
int  hdd_installed(uint32_t id);                     // index in INSTALLED or -1
int  hdd_on_vmx(void);                                // 1 = a VMX is plugged in: INSTALLED, installs and saves are on it
int  hdd_install(const Vxp *v);                      // copies a catalogue game to the VMX, or the hard disk; 1 = ok
int  hdd_has_game(int dev, uint32_t id);              // is that game on VMX dev (vmxlink.h; -1 = the hard disk)
int  hdd_room(int dev, int bytes);                    // room on it for a game that size (and its folder)
int  hdd_games_on(int dev);
int  hdd_remove(uint32_t id);                        // deletes its save folder; 1 = ok
uint8_t *hdd_read(const char *path, int *len);       // a whole file (malloc), or NULL
int  hdd_exists(const char *path);
int  hdd_title_saves(uint32_t titleId);              // how many save folders a title has on the disk (0 = none)
int  hdd_day(void);                                  // days since 15 Nov 2001
#endif
