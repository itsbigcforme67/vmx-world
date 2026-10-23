# VMX World: design

VMX World is the Xbox side of the VMX. It is a disc you put in the Xbox to get new VMX games, put them on your VMX,
play them on the TV, read the news and see the score tables. This file is the plan for how it all fits together: the
"how it would have worked in 2002" story first, then the real files and protocols we build.

The owner's brief (2026-10-20) is the source. Anything marked **(proposed)** is my default and is open to change.

## The story (2002)

The VMX is a Microsoft "microconsole" that plugs into the Duke's memory-unit slot, like the Dreamcast VMU. To the
Xbox it looks like a memory unit. That one fact gives us most of the platform for free:

- **A VMX game is a save.** All VMX games and their data live under one title on the Xbox, "VMX World". In the
  normal Xbox dashboard's Memory screen, VMX World shows up as a title, and each VMX game is one save inside it. Copying
  that save to a VMX in the dashboard is the same as installing the game. Friends can swap VMX games by copying saves
  with a memory unit, like any other save.
- **Games come from three places:**
  1. **Retail discs.** An Xbox game with VMX features carries its VMX games on the disc and a small built-in "World
     Kit" screen to install them and show their scores (like PocketStation and VMU minigames).
  2. **The VMX World disc**, which downloads games over broadband (Xbox Live arrived in November 2002). There was no
     web browser on the Xbox, so this disc is where you shop, as the Dreamcast's browser was for the VMU.
  3. **Magazine demo discs and store kiosks** (proposed): the monthly demo disc carries a few VMX games, and a
     store kiosk can write games straight to a VMX. Both use the same package file.
- **Indie games.** Anyone can make a VMX game with the free kit and submit it to the World service. They show up
  in a "Homebrew" shelf, the way Dreamcast fans shared VMU games on web pages.
- **Xbox-side code.** A downloaded game can bring an "applet": a program that runs on the Xbox from the World disc.
  The FOLD level builder is the model case: you build rooms in 3D on the TV, then send the level to the VMX.
  A retail game can also get an applet in World (its VMX unlocks, its score tables, its minigames) so you don't need
  its disc in the tray.
- **Scores.** Every VMX game keeps its high scores in one standard record. At the Xbox, World sends them over the
  network (it reads them from the hard disk and the VMX in the controller). The phone call is the VMX's own
  feature, for when it is away from the Xbox (owner, 2026-10-20: the call belongs only on the VMX). On the VMX's main menu,
  **CALL** sends the scores from all your recent games at once over a phone call to a free 1-800 number: the VMX
  dials by tone and sends the data as modem sounds through its beeper, held to the phone's mouthpiece. You don't
  need broadband or an Xbox for this; the VMX does it on its own. The World disc then shows the national tables.
- **Simulator.** World runs VMX games on the TV, so you can play them big, try them before installing, and build
  levels for them.

## Where things live

### Title IDs (proposed)

An Xbox title ID is 32 bits: two letters for the publisher and a 16-bit number. We use the letters **VX** (0x5658),
which no known publisher used, so nothing we write can clash with a real game's saves.

| ID | What |
|---|---|
| `56580001` | VMX World (the disc, and the title that owns every VMX save) |
| `5658 1xxx` | first-party VMX games (BLOX = `56581001`, ...) |
| `5658 8xxx` | homebrew VMX games (given out by the World service, first come first served) |
| other publishers | their own letters + a number from their own range |

A VMX game's ID is used for its save folder, its scores and its applet.

### On the Xbox hard disk and on the VMX

The same layout on both, because the VMX is a memory unit (FATX):

```
UDATA\56580001\                      VMX World
    TitleMeta.xbx                    "TitleName=VMX World"
    TitleImage.xbx                   the World icon
    565810010000\                    one save folder per VMX game: its ID + 0000
        SaveMeta.xbx                 "Name=BLOX" (what the dashboard shows)
        SaveImage.xbx                the game's icon
        GAME.VXP                     the game (the package, below)
        SCORES.DAT                   its score records (below)
        DATA\                        anything else the game saves (progress, levels, ghosts)
    565880010000\                    a homebrew game, same shape
```

- **Saves of a game** are in its own folder (`DATA\`, `SCORES.DAT`).
- **Reading other games' saves**: any VMX game may READ another game's folder (a sequel checks that you beat the
  first game; a dungeon game gives a bonus if you own a shmup by the same studio). Only the owning game writes.
- **A game installed on the VMX** = its folder is on the VMX. Its saves travel with it. When the VMX is in the
  controller, World copies newer saves both ways (newest file wins, by date).
- **Applets** (Xbox code) are not saves: they live in `TDATA\56580001\APPS\<game ID>\default.xbe` with their
  own files beside them. The World disc launches them and they return to World when you quit. (2002 story:
  Microsoft signs them, like Xbox Live title updates.)
- **VMX capacity** (proposed): the VMX has 8 MB of flash; about 4 MB is its memory-unit area = 256 blocks of
  16 KB, shown in blocks as the Xbox dashboard does.
- **Retail games** write VMX games into the shared VMX World folder through a "VMX library" in the Xbox XDK
  (2002 story: the one library allowed to write outside its own title). Their own saves stay under their own
  title. Anything the retail game and its VMX minigame trade (unlocks, items) goes in the VMX game's `DATA\`.

## The package: GAME.VXP (proposed)

One file holds a VMX game. Little-endian. Everything is in "chunks" so new parts can be added later.

```
header (32 bytes)
  0  char[4]  "VXP1"
  4  u32      game ID
  8  u16      version (bumped each release)
 10  u16      flags: bit 0 has an applet, bit 1 has a TV-only mode, bit 2 reads other games' saves
 12  u32      total length
 16  u32      CRC-32 of everything after the header
 20  char[12] short name, upper case, padded with 0 ("BLOX", "FOLD ROOMS")
chunks, each: char[4] tag, u32 length, data (padded to 4 bytes)
  NAME  full title, publisher, one-line blurb (UTF-8, lines split by \n)
  ICON  32 x 32 1-bit icon (128 bytes), the VMX SAVES screen's picture (World draws it big)
  CARD  24 x 24 1-bit card picture for the GAMES roulette (72 bytes)
  CODE  the program (see "The program" below)
  BRDS  score boards: for each, a name, which way is better, and the labels of fields 1-3
  APPL  the applet's download ID and size (the applet itself is a separate download into TDATA)
  FILE  extra read-only files (levels, text), each with a name
```

## High scores: SCORES.DAT (proposed)

The owner's brief: title ID, high score, field 1, field 2, field 3. One fixed record, 20 bytes, so a VMX can
send a pile of them over the phone quickly.

```
record (20 bytes)
  0  u32  game ID
  4  u8   board (a game can have several: per mode, per level set)
  5  u8   flags: bit 0 sent by phone, bit 1 sent online, bit 2 made in the simulator
  6  u16  date: days since 15 Nov 2001 (Xbox launch day)
  8  i32  score
 12  u16  field 1 \
 14  u16  field 2  | what they mean is in the game's BRDS chunk (level, time, lines, wins...)
 16  u16  field 3 /
 18  u16  CRC-16 (CCITT) of bytes 0-17
SCORES.DAT = "VXSC", u16 count, u16 0, then the records (best few per board)
```

Which way is "better" (higher score, or lower time) is in the board definition, not the record. Each VMX also has
an owner's name (8 letters, set in SETTINGS) and a serial number, sent once per call.

## Calling in (the VMX's CALL menu)

The VMX's main menu has four pills: GAMES, SAVES, SETTINGS and a "???" placeholder. The placeholder becomes **CALL** (built in the
VMX simulator).

1. CALL lists the scores not sent yet (from every game on the VMX). A sends them all.
2. The VMX says: hold its top to the phone's mouthpiece, press A. It dials **1-800** by tone (DTMF), like the pocket
   tone diallers of the time.
3. The service answers with a voice: "Welcome to VMX World. Send now." The player presses A again (the VMX can't
   hear the line: its mic and beeper are only 4 cm apart, and a handset's ear and mouth ends are 15 cm apart).
4. The VMX sends the data **three times** as Bell 202 modem tones (1200 baud: 1200 Hz = 1, 2200 Hz = 0), each with
   a sync tone first. 30 records are 618 bytes, about 5 seconds per copy; the whole call is about 20 seconds.
5. The voice reads back a 4-digit check code. The VMX shows the same code (made from the data's CRC). If they match,
   the player presses A and the records are marked sent. A wrong code: try again.
6. **(maybe)** The voice also gives one ranking ("Your BLOX score is number 12 in the country").

The message on the line:

```
sync     about 0.3 s of 1200 Hz (marks), then 0x7E 0x7E
header   "VX", u8 version, u8 record count, u32 VMX serial, char[8] owner name
records  20 bytes each
trailer  u16 CRC-16 of header + records
```

Each byte goes as a start bit, 8 data bits (lowest first) and a stop bit.

**Building it today**, as the phone network doesn't take modem tones to a toll-free number of ours for free:
- **Phone page** (first): a web page on a phone listens with the microphone, decodes the tones and posts them to
  the score service. The VMX doesn't change: you hold it to the phone's mic instead of a handset.
- **A real number** (later, maybe): a VoIP number whose call audio is streamed to our server, which runs the same
  decoder and plays the voice back.
- **The Xbox** doesn't call: World reads SCORES.DAT from the VMX or the hard disk and sends it over the network.
  Same records.

The decoder is shared (one C version for tests and the ESP32, one JavaScript version for the phone page and the
mockup) and is tested through a fake phone line: band-pass 300-3400 Hz, 8 kHz mu-law, noise, and the beeper's own
square-wave sound.

## The World disc

Tabs along the top (the VMX main menu's pill arc, grown up for the TV):

- **NEWS**: dated items from the service (new games, score contests, phone line hours). Some news ships on the disc
  so it works offline.
- **GET GAMES**: the catalogue: first party, partners, homebrew. Each entry: name, icon, size in blocks, a
  description, whether it brings an applet. A downloads it to the hard disk.
- **MY VMX**: the hard disk's VMX games on one side, the VMX in the controller on the other (free blocks shown).
  Install, remove, copy saves, delete.
- **PLAY**: the simulator. Any VMX game on the hard disk runs on the TV with the pad (the left stick or D-pad,
  A and B; BACK is the VMX's side button). Its saves are the same files.
- **SCORES**: your records, the national tables (downloaded), and SEND ONLINE. No phone call here: that is only
  on the VMX.
- **APPLETS**: Xbox-side programs from downloaded games and for retail games (FOLD BUILDER first).

A retail game's **World Kit** is a cut-down World inside that game: install its VMX games, show its scores, and a
"get VMX World" advert.

## The program: VMX managed code (decided 2026-10-20)

Owner's decision: VMX games are managed code on a small Microsoft .NET runtime, not native code and not Java.

**The 2002 story.** Microsoft lost its Java licence in the Sun settlement (January 2001) and bet on .NET (betas
November 2000 and June 2001, 1.0 in February 2002). Microsoft Research's SPOT project ran a tiny .NET runtime on
watch-sized hardware (unveiled at COMDEX, November 2002). The VMX team took that runtime in late 2001 and had about a
year to launch in November 2002, the same month as Xbox Live. Developers get a "VMX SDK" for Visual Studio .NET 2002
with an emulator, as Microsoft's toolkit for small devices worked then. Games are written in **C# 1.0**: classes, structs, arrays,
strings, no generics (those came in 2005). That limit is the era's own sandbox.

- **Downloaded and homebrew games are managed code only.** They can't touch the hardware: only the VMX calls
  (screen, buttons, beeper, their own saves, reading other games' saves, reporting scores), inside a fixed memory
  budget. The firmware (menus, CALL) is native, like a phone's built-in Snake. Retail-licensed games are managed too.
- **Limits:** heap 128 KB (owner, 2026-10-20), game code + data up to 16 blocks (256 KB), 30 ticks a second.

**How we build it.** Not nanoFramework (SPOT's real descendant, MIT, runs on the ESP32-S3): it is big, built on
ESP-IDF, and hard to bring to the Xbox and the browser. Instead:
1. Games compile with the normal C# compiler set to C# 1.0 (`LangVersion ISO-1`) against **our own small core
   library** (System.Object, String, arrays, Math...) and the **VMX library**, so nothing else exists to call.
2. A PC tool (the "linker") reads the compiled assembly, refuses anything outside the subset (generics, 64-bit maths,
   threads, reflection), resolves every reference, and writes a compact image into GAME.VXP's CODE chunk.
3. A small interpreter in plain C runs that image: the same file on the VMX (ESP32-S3), the CoreS3 rig, the Xbox
   simulator, PC and (compiled to WebAssembly) the browser.

The JS cores are ported to C# one by one and checked frame for frame against the JS versions. FOLD ROOMS keeps its
C core as built-in software until it is ported.

## Building it (plan)

1. **This design + a TV mockup** (`mockup/world.html`): every tab, the simulator running the real game cores, the
   install flow with made-up downloads, scores sent online. CALL itself is built into the VMX simulator
   (Pocket Arcade `vmx/`, branch `vmx-call`), using `mockup/modem.js`.
2. **Formats in code**: a package tool (`tools/vxp.py` pack/unpack/list) and the score record in Python, C and JS,
   with shared test files so all three agree.
3. **The modem**: C encoder (for the VMX), JS + C decoder, the fake phone line test, then the phone page.
4. **World on the Xbox** (nxdk, like FOLD: same software renderer, xemu headless tests, padscript): the tabs,
   the UDATA layout, the simulator running C cores, FOLD BUILDER launched as the first applet.
5. **The real VMX link**: the VMX as a memory unit (FATX on its flash), with `XMU GADGET`'s board.
6. **The score service and the phone page**: needs a server; the owner decides where it runs.

## The look on the Xbox side: skeleton holograms (owner, 2026-10-23)

Owner: "I think maybe we lean into this skeleton hologram look for all VMX stuff on the Xbox side. It's very cool."
The VMX on the TV is a green x-ray hologram of its real CAD model (shells, side button and its own D-pad caps and
A/B; never the sleeve's parts): glowing see-through faces, brighter where they turn away; the parts' sharp edges;
the glass hides the insides so the VMX's live picture reads; bloom, scanlines, a rolling bright band, a faint
flicker, and a projector disc with a beam under it. Preferred over the outline-only version.
- Built: World's PLAY (xbox/holo.c; data tools/hologram_export.py -> xbox/assets/vmx_holo.bin; preview and
  comparison images tools/hologram_preview.py -> docs/hologram/).
- World's own screens follow it too (2026-10-23, xbox/world.c's hologram kit): glass panels (the background seen
  darkened and tinted green through them) with glowing edges and bright corner brackets, glowing titles with a fading
  rule, lit selections (a bright bar and a sweep of light), projected slanted tabs, game cards glowing on a projector
  line, an amber version for VMX CASINO, and one pass of scanlines and a slow rolling band over the whole screen.
- To follow (same look): other VMX things on the Xbox: MY VMX / GET GAMES details, applets' VMX windows (FOLD
  BUILDER's VMX popup), retail World Kits, loading and boot screens.
