# VMX World: status

Read `docs/DESIGN.md` first: it is the plan (the 2002 story, the save layout, GAME.VXP, the score record, the
phone call, the World disc's tabs, the open question about the program format).

## 2026-10-20 later: the call is VMX-only (owner)

Owner: "The call in features need to be accessible from just the VMX. Having them on the TV is superfluous since we
can just use the network there." So:
- World's SCORES tab now has SEND ONLINE (made-up network, real records packed by modem.js), no call screens.
- CALL is on the VMX simulator's main menu (was "???"): Pocket Arcade branch `vmx-call`, committed locally, NOT
  pushed (the simulator is live on Pages; needs the owner's OK to merge/push). Screens: list of new scores ->
  HOLD MY TOP TO THE PHONE -> dialling 1-800-VMX-WRLD (DTMF) -> "when it says SEND NOW press A" -> sending (Bell 202
  x3) -> the VMX's 4-digit code, A = matches (marks them sent), B = try again. The call's sound plays even with SOUND
  off (it is the data). Records sent are remembered per game by value (`sent` in vmx-saves), so a new best is new again.
  `tools/vmx-sync.sh` copies modem.js from here.

## 2026-10-20: design + TV mockup

Owner's brief (chat, 2026-10-20): an Xbox-side downloader / installer / launcher for VMX games; games from retail
discs (PocketStation/VMU style) or downloads; downloads can bring Xbox-side code (the FOLD builder); game data and
saves under one "VMX World" title; games can read other games' saves; a simulator, leaderboards and news; scores in
a standard record (title ID, score, fields 1-3) sent in bulk over a dial-up call to an 1-800 number from the VMX's
main menu; retail games carry a small World; World gets applets for retail games.

Done:
- `docs/DESIGN.md`. My choices there are marked (proposed). The main ones: the VMX is a memory unit, so a VMX
  game is a save of title `56580001` ("VX" publisher letters, unused by real games); the "???" main-menu pill
  becomes CALL; the call is one-way (DTMF dial, then Bell 202 sent 3 times, then a voice reads a check code back),
  because the VMX's mic and beeper are both at its top and can't reach both ends of a handset; for today a phone
  web page decodes the tones instead of a real 1-800 line; programs recommended as WebAssembly ("VMX byte code").
- `mockup/modem.js`: score records (20 bytes, CRC-16), the call message, the call's sound (square-wave beeper or
  sine), a fake phone line (300-3400 Hz, 8 kHz mu-law, noise) and a decoder.
  `node tools/modem_test.mjs` passes: 30 records decode through clean, normal, noisy and quiet lines.
- `mockup/world.html` + `world.js`: the World disc's TV screen, 640 x 480, styled after FOLD BUILDER (green grid,
  slanted tabs, brushed metal from fold/assets/metal.rgb, the VMX head render from fold/assets/vmx_large.rgba).
  Tabs: NEWS, GET GAMES (made-up downloads to the "hard disk"), MY VMX (hard disk vs the VMX in controller 1, blocks,
  install / remove / copy), PLAY (the real game cores on the VMX head, pad or keyboard), SCORES (your records, a
  made-up national top 5, CALL IN: plays the real call sound, decodes it through the fake line, shows the voice's
  code against the VMX's), APPLETS (FOLD BUILDER: a picture of the real builder; a retail-applet placeholder).
  Keys: arrows, Z/Enter = A, X/Esc = B, Q/E = triggers (tabs), Backspace = BACK (quits a game), V = plug/unplug the
  VMX. Pads: standard Gamepad mapping. `?debug` exposes `window.WORLD`. State is in localStorage `vmx-world`.
- `tools/sync.sh` copies the game adapters/cores from Pocket Arcade's `vmx/` and FOLD ROOMS from microgames into
  `mockup/vmx/` (never edit those copies) and converts the two FOLD assets. Preview config: "vmx-world" (port 8799).

Not done / next (DESIGN.md "Building it"):
1. `tools/vxp.py` (pack/unpack GAME.VXP) and the score record in C, with shared test files.
2. The modem in C (VMX encoder) and the phone page (mic decoder in JS).
3. World on the Xbox (nxdk), reusing FOLD's renderer and test tools; FOLD BUILDER as the first applet.
4. The VMX as a memory unit (XMU GADGET).
Open for the owner: the program format (WebAssembly or not), the CALL name for the "???" pill, the title ID letters,
where the score service runs, whether to publish the mockup.

## 2026-10-22: VMX CASINO (shared chips, cashier, chip leaderboard)

Owner's brief and calls are in `docs/CASINO.md` (read it first). Built:
- Chip bank in the firmware (`Microsoft.Vmx.Chips`, casino flag in GAME.VXP), the web simulator and this mockup.
  BLACKJACK AND HOOKERS (bait = chips) and 9-BALL (stakes per rack) both play from it; C# parity holds.
- Daily stipend: under 25 chips, a casino game started on a new day tops the stash up to 25.
- Chip stash = score record 56581F00 (chips, peak, cashed out): CALL on the VMX, SCORES here.
- Mockup: APPLETS > VMX CASINO CASHIER (casino-cage look): Xbox games with rates, amount, confirm, the steps
  (read save, add money, fix checksum, sign, take chips), 500 chips per game per day. GTA III is the one marked
  ready; the saves are pretend. 9-BALL added to GET GAMES; Blackjack moved to a "casino" shelf.
- Research: `docs/research/CASHIER_SAVES.md` (save signing, per-game formats, trainer route). GTA III first.
Next: a PC save tool for GTA III (edit + checksum + re-sign, tested in xemu with the owner's own disc), then KOTOR /
Vice City; Shenmue II needs a save diff. Real online boards need the score service (owner decides where).

## 2026-10-22 later: three more casino tables
VIDEO POKER, ROULETTE and SLOTS (CASINO.md "The games") are in GET GAMES (casino shelf, 2 blocks each) and play from
the shared bank; the cashier opens with any casino game installed.

## 2026-10-22 end: 18 casino tables
All in GET GAMES (casino shelf) and the cashier's list: see CASINO.md. `tools/add_casino_table.py` wires a new table in.
`docs/CASINO_TABLE_BRIEF.md` is the brief the helpers built them from.

## 2026-10-22: World on the Xbox, for real (docs/XBOX.md)
Owner: "Let's get world onto the Xbox for real". `xbox/`: the TV app in C on nxdk (FOLD's renderer and platform
layer), with the real VMX firmware + .NET runtime + all 27 games inside PLAY; installs are real saves under
E:\UDATA\56580001 (TitleMeta/SaveMeta for the dashboard); XBE title ID 56580001. Checked in hidden xemu (formatted
scratch disk, no real pads): every tab, a download, a 3 CARD POKER hand on the VMX (same screen as the PC build), and
the network loader end to end (tools/xemu_loader_test.sh: 7/7). NOT on the real Xbox yet: it was off; the owner runs
tools/xbox_upload.sh once (FTP login) and keeps tools/xbox_serve.py (port 8771) running.
