# VMX CASINO: design

VMX CASINO is a family of VMX casino games that share one stash of chips. Chips won at one table can be staked at
another, cashed out at the Xbox for money in your Xbox games, and sent to the leaderboards as one "chip stash"
score. Owner's brief (2026-10-22): "A collection of VMX casino games with shared resources. Money you win in
blackjack could be used to gamble in pool... a themed tool in VMX World to convert your chips into in-game currency
in various Xbox games (Shenmue 2, GTA3 etc), different games can have different conversion rates... an easy way to
have useful cross features in games that might not be fully decomped but are easy to cheat in. Also a universal
high score of your chip stash to upload to online leaderboards."

Chips are play money. They can't be bought, and nothing real can be won with them.

## The owner's calls (2026-10-22)

- **Blackjack and Hookers stays as it is.** Bait is that table's name for chips: 1 bait = 1 chip, one balance.
- **Going broke: a small daily stipend.** Once a day, starting a casino game with fewer than 25 chips tops the
  stash up to 25. The owner: "To make serious money you'll still need to be careful and not blow it daily. Maybe
  build up a bit with easier slower games."
- **Cashing out spends the chips.** Chips cashed out at the Xbox leave the stash for good. The stash leaderboard
  rewards keeping chips, the cashier rewards spending them. The most ever held (PEAK) and the total CASHED OUT are
  kept as bragging fields.
- **First Xbox games:** any that are easy to edit first ("if Shenmue or GTA are hard, I'm sure we can find other
  games to try first").

## The chip bank (built)

- **Where it lives:** the VMX firmware keeps it in a save of its own (`casino`), outside any game's folder, because
  every casino game writes it. The 2002 story: the firmware is the bank, and casino games can only ask it for chips.
  A new VMX starts with 100 chips.
- **Who can use it:** games whose GAME.VXP header has flags bit 3 set (`game.ini` `casino=1`, `tools/vxp.py
  --casino`). Other games' `Chips` calls answer 0 / false.
- **The game's side:** `Microsoft.Vmx.Chips` in the VMX library:
  `Balance()`, `Bet(n)` (takes n, or returns false and takes nothing), `Pay(n)`, `Bonus()` (the daily chips given
  when this game started, so it can say "DAILY BAIT +25").
- **Demos** (attract mode, DEMO on GAMES) play from a scratch bank of 100, so they never touch the player's chips.
- **Kept with it:** chips, peak, total staked, total paid, total cashed out, the day of the last stipend, and what
  CALL last sent.
- **No clock set** (`plat_day()` = -1): the stipend can come once per power-on instead of once a day.
- **The same bank everywhere:** firmware `core/game_vx.c`; PC test runner `runtime/vxrun.c --chips N`; web
  simulator `vmx.js` (localStorage `vmx-casino`); World mockup (`ST.bank`). JS cores take a bank object
  `{ chips, bonus }` in `newGame(seed, bank)` and change `bank.chips` themselves. The parity check runs the C# and
  JS games against the same starting bank (`CHIPS=15 tools/parity/check.sh blackjack` exercises going broke).

## The games

| Game | ID | Chips | Notes |
| --- | --- | --- | --- |
| BLACKJACK AND HOOKERS | 56581005 | bet 5-50 a hand; blackjack pays 3:2 | broke (under 5): "OUT OF BAIT / MORE TOMORROW" |
| 9-BALL | 56581009 | stake 0-50 a rack (0 = friendly); a won rack pays the stake back doubled | slow and skill-based: the "build up slowly" table |

Ideas for more tables (none built; owner to pick): video poker (low variance, a good slow builder), slots (fast,
risky), roulette, craps, keno or scratch cards, horse or frog racing, a casino version of BOG HOP.

## The leaderboard record (built)

The stash is one score record of its own (DESIGN.md "High scores"), under the bank's ID **56581F00** "VMX CASINO":

- score = chips now
- field 1 = the most chips ever held (capped at 65535)
- field 2 = chips cashed out (capped at 65535)

The VMX's CALL lists it as CHIPS once chips have been played, and sends it again whenever the stash has changed
since the last call. World's SCORES tab shows it as CASINO CHIPS and sends it with SEND ONLINE.

**Cheating.** A homebrew VMX (or an edited save) can claim any stash. For real online boards we would want each
casino game to log its results so the server can sanity-check how the stash grew. That isn't built.

**Real online boards:** the score service doesn't exist yet (DESIGN.md: "the owner decides where it runs"). Until
then the boards are made up.

## The cashier (World mockup built; Xbox side planned)

A World applet, **VMX CASINO CASHIER**. It appears once a casino game is on the hard disk, and needs the VMX in
controller 1 because that is where the chips are.

1. It lists the Xbox games found on the hard disk that it knows how to top up, each with its rate.
2. Pick a game and an amount (10 chips at a time, LT/RT for 100), then A, then confirm.
3. It reads the save, adds the money, fixes the game's own checksum, re-signs the save, and takes the chips from the
   VMX. The mockup shows these steps; the saves in it are pretend.
4. **Daily limit:** 500 chips per game per day, so a big stash can't flood a game's economy in one go.

**How the Xbox version works.** Full research with sources: `docs/research/CASHIER_SAVES.md`.

- Xbox saves are signed with HMAC-SHA1. The key comes from the game's signature key in its XBE certificate, run
  through the console's certificate key. A homebrew app can re-sign a save if it reads the game's XBE (on its disc
  or the hard disk), and nxdk has the HMAC functions.
- The certificate key doesn't need shipping: it can be read from the kernel at run time and checked against a known
  value. The cashier refuses to sign if that check fails.
- None of the candidate games uses console-locked saves.
- **Save editing, not trainers, for v1.** Trainers patch memory while the game runs. They need per-version money
  addresses and a hook after the save loads; the XBMC4Xbox trainer loader shows how, and it's the fallback.

**Rates and readiness.** 1 chip is worth:

| Game | Title ID | 1 chip = | State |
| --- | --- | --- | --- |
| Grand Theft Auto III | 5454000E | $100 | First target. PC block format plus a byte-sum checksum plus a 20-byte signature (from GTASaveData's code). Money in block 16 (+0x00, and +0x0F for the counter on screen). Still to check: the save file's name and exactly which bytes the signature covers. |
| GTA: Vice City | 5454000F | $250 | Probably the same wrapper as GTA III (unconfirmed). Money in block 18. |
| GTA: San Andreas | 54540082 | $250 | PC format documented; Xbox wrapper unconfirmed. |
| Star Wars: KOTOR | 4C410003 | 10 credits | KSE edits Xbox saves, so the format is known; the credits offset still needs reading out. |
| Shenmue II | 4D530032 | HK$2 | Hard: no public save format or Xbox money address. Crate work pays HK$10 a crate. Needs our own research (a save diff in xemu). |

**Next steps for the Xbox side:**
1. Get a GTA III Xbox save (ours, made in xemu) and confirm its file name and what the signature covers; round-trip
   it in a PC tool (Python) that reads, edits, checksums and re-signs it, and check that GTA III in xemu loads the
   edited save.
2. The same tool for KOTOR and Vice City.
3. Shenmue II: diff two saves in xemu (before and after earning money) to find the money and any checksum.
4. Port the tool into World on the Xbox (nxdk) when World itself is built (DESIGN.md "Building it", step 4).
   This needs the owner's game discs (their own copies), read on their own Xbox or in xemu. Nothing is downloaded.
