# Brief: build one VMX CASINO table (shared by three helpers)

You are building ONE new casino game for the VMX, a 64x64 1-bit handheld (30 ticks a second, D-pad + A + B).
It joins VMX CASINO: casino games that share one chip stash. Read first:
- `/home/james/claude projects/vmx-world/docs/CASINO.md` (the design and the owner's calls)
- The reference table, 9-BALL: `/home/james/claude projects/microgames/ui/games/pool_core.js` (JS core) and
  `/home/james/claude projects/vmx-firmware/games/pool/Pool.cs` (its C# twin), plus `pool.html`, `tools/pool_film.mjs`,
  `vmx-firmware/games/pool/game.ini`, `card.txt`, `vmx-firmware/tools/parity/adapters/pool.mjs`.
- Also look at `microgames/ui/games/blackjack_core.js` for how the bank/bonus/broke flow reads on a card table.

## What to make (all of it)
1. `microgames/ui/games/<g>_core.js`: pure logic + drawing. Exports `W, H, newGame(seed, bank = { chips: 100, bonus: 0 }),
   step(g, held)`, `draw(g)` (returns a Uint8Array(4096), 1 = lit), `demo(g)` (returns `{ auto: true }` or a held-buttons
   object for the demo player). The core takes HELD buttons and finds edges itself (as pool does). Title screen first
   (A starts; in demo it starts by itself after ~40 ticks). Seeded, deterministic.
2. `microgames/ui/games/<g>.html`: copy pool.html's shape (canvas, Restart, Demo, keyboard arrows/Z/X + gamepad, a short
   rules/controls paragraph).
3. `microgames/tools/<g>_film.mjs` (copy pool_film.mjs) and render `microgames/anim/<g>/<g>.bin` (~60-90 s) + a GIF
   (see how anim/pool/pool.gif was made: numpy+PIL from `microgames/.venv/bin/python`, frames x4096 bytes, every 2nd frame).
4. `vmx-firmware/games/<g>/<Name>.cs`: a line-for-line C# 1.0 port (class extends `Game`; `Start(seed, demo)`, `Tick(held,
   pressed)`, `Draw(Screen)`, `UsesB`). Chips via `Microsoft.Vmx.Chips` (`Balance()`, `Bet(n)` -> bool, `Pay(n)`, `Bonus()`)
   where the JS core reads/changes `g.bank.chips` / `g.bank.bonus`. `game.ini` with `casino=1` and your id/title/lines/
   publisher=VMX Studio/blurb; a 24x24 `card.txt` picture.
5. `vmx-firmware/tools/parity/adapters/<g>.mjs` (copy pool.mjs's shape: `start: (seed, demo, bank) => m.newGame(seed, bank)`).
6. Build and check: `cd vmx-firmware && tools/build_game.sh <g>` then `tools/parity/check.sh <g> 30000` and
   `CHIPS=15 tools/parity/check.sh <g> 8000` (low stash: must reach and handle "broke"). All must say "same".
   Measure the interpreter cost: `build/vxrun build/<g>.vxi 1 1 30000 5 2>&1 >/dev/null | tail -1` (and demo 0). Keep the
   worst tick well under ~300k instructions (the device runs ~3.5-7M a second); average ideally under ~30k.

## Rules that keep the C# port exact (learned the hard way)
- Integer maths only in the core: no floats, no Math.sin/sqrt/random/round on floats, no Array.sort (V8's sort differs).
  Use `const div = (a, b) => Math.trunc(a / b)` for division and keep every product under 2^31. RNG: copy pool's
  `rnd(g)` (LCG on a uint32, returns `g.rs >>> 16`, 0..65535); in C# `rs = rs * 1664525u + 1013904223u; return (int)(rs >> 16);`.
  If you need a shuffle, Fisher-Yates with `rnd(g) % (i + 1)`.
- Draw text with the core's own 3x5 font table (copy pool's FONTCH/FONT and add glyphs you need) and port the same table
  to C# (as Pool.cs does). Don't use Screen.Text in C#.
- Same order of rnd() calls in both. Same strings. Phases as strings in JS, int constants in C#.
- Demo mode must not care about the real bank: the runtime gives demos a scratch bank; just use the bank you're given.
- Bonus: at newGame, if `bank.bonus > 0`, show a message like "DAILY CHIPS +25" (C#: `Chips.Bonus()` in Start).
- Broke: if the stash can't cover the minimum bet, show a clear "OUT OF CHIPS / MORE TOMORROW" state (the daily stipend
  tops up to 25 when the game is next started on a new day). Never refill chips yourself.
- Show the stash on screen while betting. Bets must never exceed the stash; take the stake with `bank.chips -= n` (C#
  `Chips.Bet(n)`) when the round starts, pay winnings including the stake back with `bank.chips += n` (C# `Chips.Pay(n)`).
- Real, honest odds: the outcome comes from the RNG with the stated probabilities; animations never change the result.
  State the house edge / return in the core's header comment.

## Look and feel
- 1-bit, 64x64, readable at a glance. Our own art, generic casino imagery (no real casino brands, no trademarked slot
  symbols or layouts). Match the family: pool/blackjack use a 3x5 font, boxed messages, a blinking "A: ..." hint.
- Check your screens by rendering frames to PNG offline (numpy+PIL from microgames/.venv, scale x4-x6) and LOOKING at them
  with the Read tool; fix anything cramped or unclear. Never open windows on the owner's screen (no GUI browsers, no
  preview servers). The machine has ~7.8 GB shared with the owner's apps: keep jobs light, one at a time.

## Don't
- Don't edit shared files: Makefile, POCKET ARCADE, vmx-world, microgames README/GAMES_LOG, the SDK/runtime, other games.
  The coordinator integrates those. Don't commit or push anything (the coordinator commits).
- Don't download anything.

## Report back (final message, concise)
Files made; rules and pay table with the house edge; controls; the demo player's strategy; parity results (paste the
lines); IL avg/worst; anything you were unsure about. Also the path of a PNG contact sheet of 6 representative frames you
made (put it at `microgames/anim/<g>/<g>_frames.png`).
