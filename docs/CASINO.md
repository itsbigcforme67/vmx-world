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
| VIDEO POKER | 5658100A | bet 1-5; Jacks or Better, full-pay 9/6 (royal 4000 at 5 chips) | 99.54% return with perfect play at max bet (98.4% under 5); the careful builder |
| ROULETTE | 5658100B | chips in 1s, up to 50 a bet; red/black, odd/even, 1-18/19-36 (1:1), dozens (2:1), one number (35:1) | single zero: 97.3% return on every bet; bets stay on between spins |
| SLOTS | 5658100C | bet 1-3; 3 reels x 24 stops, 1 line; 3 VMX chips = jackpot (3000 at 3 chips) | 93.4% return (94.85% at 3 chips), a win on 18.45% of spins; the fast, risky table |
| HI-LO | 5658100D | stake 1-10; call higher/lower; pot x floor(97 x left / wins)/100 per right call; B collects | every call returns 97% of the pot (card-aware odds); 10 calls or 9999 auto-collect |
| CASINO WAR | 5658100E | bet 1-20 + tie 0-10 (10:1); tie: war (raise, tie at war wins) or surrender (half back) | 2.88% going to war; tie bet 18.65%; 6-deck shoe |
| BACCARAT | 5658100F | Player 1:1, Banker 1:1 less 5% (whole-chip commission with credit), Tie 8:1; 0-20 each | 1.24% / 1.06% / 14.36%; 8 decks, full tableau |
| 3 CARD POKER | 56581011 | ante 1-10, pair plus 0-10; play or fold; ante bonus 5/4/1; pair plus 40/30/6/4/1 | ante 3.37% (Q-6-4), pair plus 2.32% |
| RED DOG | 56581012 | bet 1-20; spread pays 5/4/2/1, trips 11:1, raise once | 2.80% raising on spread 7+; 6 decks |
| LET IT RIDE | 56581013 | three bets 1-10; pull back two; pays 1000/200/50/11/8/5/3/2/1 | 3.51% of one bet with best play |
| CRAPS | 56581014 | pass/don't 1-20, free odds to 3x (whole-chip steps), field, any 7, any craps | 1.41% / 1.36% / 0% / 2.78% / 16.7% / 11.1%; shared dice art lives here |
| SIC BO | 56581015 | small/big/odd/even, totals 60/30/17/12/8/6/6, any triple 30, one triple 150, single die 1/2/3 | 2.78% even bets; totals 9.7-19%; Macau table |
| CHUCK-A-LUCK | 56581016 | faces 1-6 (1/2/10 for 1/2/3 dice), lo/hi 1:1 | 4.63% faces, 2.78% lo/hi; flipping birdcage |
| KENO | 56581017 | 1-10 spots, bet 1-10, 20 of 80 drawn | 92-95% for 2-10 spots (1 spot 75%); kinder than a lounge on purpose |
| BIG SIX | 56581018 | money wheel, 54 segments: 1/2/5/10/20, Joker and X 40:1 | 11.1% ($1) to 24.1% (40:1) |
| SCRATCH | 56581019 | tickets 1/2/5: MATCH 3 and LUCKY 7, prizes to 500x | 92.32% / 93.84%; outcome fixed at purchase, scratched with the coin |
| FROG RACE | 5658101A | BOG HOP frogs, 4-6 a race, WIN bets 0-20 at posted odds | 9.5-11.1% per race (shown on the card), whole finishing order drawn first |

Every new table's board is its biggest single payout (Video Poker's field 1 = hands played). The outcome always
comes from the RNG first; animations only land on it (slot stops and the roulette ball can't change the odds).
Return figures are exact (slots enumerated over all 13824 stop combinations; checked again by the coordinator).

All 18 tables are in (2026-10-22: owner "get em all in here"). They were built from `docs/CASINO_TABLE_BRIEF.md` by
helpers and checked by the coordinator (frames, parity, odds). `tools/add_casino_table.py` wires a finished table into
the simulator, World, `make test` and the README. Not every table has to be on a VMX at once (owner): World picks
which go on it; the firmware holds up to 32 managed games.

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
