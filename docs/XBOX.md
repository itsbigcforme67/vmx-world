# VMX World on the original Xbox

The World disc's TV app, for real, built with nxdk (`xbox/`). Started 2026-10-22 (owner: "let's get world onto the
Xbox for real"). The look follows `mockup/world.html`; everything in it is real except the network service.

## What it does

- **NEWS**: dated items (on the disc for now).
- **GET GAMES**: the disc's catalogue (`catalog/*.VXP`: every game vmx-firmware builds, 27 now). A "downloads" it:
  the VXP is copied to the hard disk as a save of VMX World (below). Title, publisher, blurb and the 24x24 card all
  come out of the VXP itself.
- **MY VMX**: what's on the VMX (or the hard disk, below); A: play it or delete it.
- **The VMX in a controller** (xbox/vmxlink.c): a memory unit is USB mass storage on the controller's hub, so World
  registers nxdk's mass-storage driver before SDL starts USB (`app_usb_init`) and reads the VMX's FATX volume with
  vmx-firmware/core/fatx.c. A memory unit with `56580001\VMX` on it is a VMX: while it's plugged in, MY VMX, PLAY,
  GET GAMES' downloads and SCORES (records, casino chips) all use the VMX; unplugged, World's own copy on the hard disk.
  Plugging and unplugging is noticed while World runs ("VMX PLUGGED IN"). Another memory unit: "isn't a VMX".
- **PLAY**: the real VMX on the TV, as a turning skeleton hologram (xbox/holo.c; DESIGN.md "The look on the Xbox
side"): the VMX firmware's hardware-free core (vmx-firmware/core) with the .NET runtime
  (vmx-firmware/runtime/vxrt.c) running every game on the hard disk, inside the owner's VMX render. The pad is the
  VMX's: D-pad, A, B; Back is its side button; Y goes back to World. Its saves, records and casino chips are files.
- **SCORES**: the VMX's records per game and the VMX CASINO stash. Sending online needs the score service.
- **APPLETS**: FOLD BUILDER (starts E:\Games\FOLD\default.xbe if it's there) and the CASINO CASHIER (detects which
  of its Xbox games have saves on this console; editing them isn't built yet: the PC save tool comes first).
- Back + Start in World's menus: back to the dashboard.

## Where things are on the Xbox (docs/DESIGN.md "Where things live")

```
E:\UDATA\56580001\TitleMeta.xbx        "TitleName=VMX World" (UTF-16): the dashboard's Memory screen shows the title
E:\UDATA\56580001\<ID>0000\GAME.VXP     one installed VMX game, with SaveMeta.xbx "Name=<title>"
E:\UDATA\56580001\SETTINGS.BIN, <ID>0000\SAVE.BIN + RECORD.BIN, 56581F000000\CHIPS.BIN   World's own VMX's saves
E:\TDATA\56580001\$c\5658000156581010\    FOLD BUILDER as World content (else E:\Games\FOLD)
E:\Games\VMXWORLD\                      World itself (the network loader, world.xbe, assets\, catalog\)
```

World's XBE has title ID 56580001 (`tools/xbe_title_id.py` patches the certificate; cxbe can't). With no VMX plugged
in, World's VMX lives on the hard disk. On a VMX the same folders sit at the root of its volume (a memory unit's root
holds title folders, as UDATA does): `56580001\<ID>0000\GAME.VXP`, `56580001\VMX\*.BIN` (vmx-firmware/core/vmxvol.h).

## Build and test

    tools/build_xbox.sh                       # -> xbox/build/xbox/VMXWORLD/ and vmxworld.iso
    tools/build_xbox.sh DEMO=tests/xemu_play.txt   # with a pad script as D:\demo.txt (tests only: it plays itself)
    tools/run_xemu.sh --fresh --shot x.png --after "30 40"   # hidden xemu, formatted scratch E:, no real pads
    tools/xemu_loader_test.sh                 # the network loader end to end (7 checks)
    cd xbox && make -f Makefile.pc && build/world_headless tests/tour.txt   # the same app on the PC, screenshots

The build goes through `~/xboxdev/links/vmxworld` (make can't take the space in the path); the Makefile reaches
microgames/fold (renderer, platform layer, GPU, pad scripts) and vmx-firmware (core, runtime) by relative paths.
Never `make clean` in xbox/ (nxdk's clean deletes its shared libraries). The VMX games come from
vmx-firmware/build/games (build them there first).

    tools/run_xemu.sh --mu vmx.img ...        # a memory unit image in controller 1 (xemu's keyboard pad)
    tools/build_xbox.sh DEMO=tests/xemu_vmx.txt   # then: World lists the VMX's games and downloads one onto it
    tools/kernel_fatx_test.sh                 # the Xbox kernel's FATX driver against our FATX code (PASS)

xemu runs use `unshare` with an empty /dev/input so they never see the owner's controllers. A VMX image for xemu:
`../vmx-firmware/build/vmxmu vmx.img format`, then `... install GAME.VXP`, and `... tree` to see what World did to it.
(xemu's memory unit reports 16383 sectors through nxdk's driver for an 8 MB image; FATX's geometry comes out the same.)

## Install on the real Xbox (network loader, as FOLD)

    tools/xbox_upload.sh      # once; the owner types the FTP login. Puts the loader in E:\Games\VMXWORLD
    tools/xbox_serve.py       # on the laptop, port 8771, while starting VMX World on the Xbox

Each start of VMX World asks the laptop for the build's version and downloads it (world.xbe + assets + catalog,
~1.2 MB) when it changed. Without the laptop it starts the copy on the disk.

## Found on the way

- nxdk's `lroundf` never returns (the VMX firmware's menus hung in s_line). `xbox/nxdk_compat.h` replaces it for the
  World build; the other maths the firmware and runtime use (atan2, pow, fmod, hypot, sqrt, floor, ceil, cos) were
  checked in xemu and are fine.
- With pbkit (the GPU), FOLD's `gfx_gradient` goes under the picture, not into it: World draws its background into
  the picture itself, or the VMX render's soft edges blend with black.
- vmx-firmware/core/surf.c's stand-in `gfx_glyph` is weak, so builds that link FOLD's gfx.c use FOLD's.
- xemu's frame rate (20-25 fps here) isn't the console's: real numbers need the hardware.

## The hologram's cost

Every picture of the model (the turning VMX's 40 steps, the big one; glow, edges, glass and bloom baked in) is made
ahead of time on the PC (owner: "why not just do that ahead of time"): `cd xbox && make -f Makefile.pc holo` renders
them with the same code (holo.c, holo_bake.c) into assets/vmx_holo.pak (1.4 MB, zero runs packed), which goes on the
disc; the same run also writes FOLD's head-on VMX pictures (microgames/fold/assets/vmx_small, vmx_large: FOLD
BUILDER's VMX window and World's fallback when there's no .pak). World only loads them; a frame adds the picture, the beam and the live screen through a tint table, and the big
view works out once which VMX pixel each TV pixel shows. Re-bake after changing the model (tools/hologram_export.py
-> assets/vmx_holo.bin), the look or the views' layout (a .pak made for another layout is refused and World renders
from the .bin instead). In xemu: 15-17 fps in PLAY, no slow start; the console's own numbers are still to come.

## Next

1. On the real Xbox (owner: upload once, run the server): looks, speed, the pad.
2. Sound: the VMX's beeper through the Xbox's audio (vmx_audio is ready; World doesn't play it yet).
3. GET GAMES over the network from a laptop "service" (the loader's HTTP client), not only from the disc.
4. The real VMX in a real controller: World's side works in xemu (memory unit image) and the CoreS3 rig presents its
   volume over USB (vmx-firmware docs/STATUS.md), but the two haven't met: needs the rig flashed and an Xbox
   controller-port-to-USB adapter, or the VMX's own board. Then SCORES online and the cashier's save editing.
