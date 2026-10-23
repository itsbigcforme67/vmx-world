# Cashier: editing retail Xbox saves (research)

Research date: 2026-10-22. Read-only web research; nothing downloaded except
source files read for reference. Marks used below:

- **[V]** verified: read in a primary source (source code or the doc itself), URL given.
- **[P]** partly verified: a secondary source says it, or it is inferred from verified facts.
- **[U]** unverified: could not confirm. Must be tested on hardware or in xemu.

No key values are copied into this file. All key bytes stay out of the repo (see 1.4).

---

## 1. How original-Xbox save signing works

### 1.1 Folder layout [V]
- Saves live in `E:\UDATA\<titleid-hex>\<savefolder>\`. Each game folder holds
  `TitleMeta.xbx` (`TitleName=...`) and `TitleImage.xbx`. Each save folder holds
  `SaveMeta.xbx` (`Name=...`), an optional `SaveImage.xbx`, and the game's own files.
  `E:\TDATA\<titleid>\` holds title data such as DLC and settings.
  Source: https://xboxdevwiki.net/Xbox_Savegame_System (raw: `?title=Xbox_Savegame_System&action=raw`)
- Save folders made with XAPI `XCreateSaveGame` get a 12-hex-character name: a hash of
  the save name. The algorithm is in `XboxHelper.EncodeGameSaveName`. It keeps
  `hash = (c + (hash << 16)) % 0xFFFFFFFFFFC5` for each character, then prints the
  low 48 bits as uppercase hex.
  Source: https://github.com/whampson/gta-save-data/blob/master/GTASaveData.Core/Helpers/XboxHelper.cs
  The cashier only edits saves that already exist, so it never needs to create a folder name.

### 1.2 Where a title's key comes from [V]
- The XBE header holds the base address at file offset 0x104 and the certificate
  address at 0x118. The certificate's file offset is `cert_addr - base_addr`.
- Fields in the certificate:
  - +0x0C: title name (UTF-16)
  - +0xB0: LAN key (16 bytes)
  - +0xC0: **signature key** (16 bytes)
  - +0xD0: alternate signature keys (16 × 16 bytes)
- Sources:
  - xboxsig.py reads offsets 260/280 and `loc+192`: https://github.com/PMStanley/xbox-save-sig (MIT)
  - JayFoxRox dump-keys.py reads the LAN key at cert+0xB0: https://github.com/JayFoxRox/xbox-tools/blob/master/python-scripts/dump-keys.py
  - Certificate field list: https://xboxdevwiki.net/Xbe
- The cashier can read `default.xbe` from the game's HDD install, or straight from the
  disc in the DVD drive, using nxdk file I/O. [P] Disc read access from nxdk is not tested here.

### 1.3 The signature itself [V]
These are the steps for the common case: XAPI `XCalculateSignature*` with the save-game flag.

1. `auth_key = HMAC-SHA1(key = XboxCERTKey, msg = cert.SignatureKey)[0:16]`
   - XboxCERTKey is a 16-byte constant in the kernel.
   - At XBE load the kernel stores this value as the running title's `XboxSignatureKey`.
2. **Roamable:** `sig = HMAC-SHA1(key = auth_key, msg = data)` gives 20 bytes.
3. **Non-roamable:** `sig = HMAC-SHA1(key = XboxHDKey, msg = roamable_sig)`. This is a
   second HMAC over the roamable signature, using the console's own 16-byte HDD key
   from the EEPROM.
4. Some titles use `XCalculateSignatureBeginEx(flags, dwAltTitleId)`. Those pick one of
   the certificate's alternate signature keys instead.

Each game decides which bytes go into the HMAC and where it stores the 20 bytes.
That is a per-game detail. It is often an appended trailer or a separate `.sig`
file. Some games, such as Mafia, use the content flag: plain SHA-1 with no key.

Sources:
- Step 1, as code: `XboxHelper.CalculateGameSaveSignature`, https://github.com/whampson/gta-save-data/blob/master/GTASaveData.Core/Helpers/XboxHelper.cs (no licence file, so read-only reference).
- Steps 2-3, as cleaned-up XAPI decomp: `XCalcSigLib.cpp` (`XapipCalculateSaveGameSignatureEnd`, `XapiConvertSignatureToNonRoamable`), https://github.com/feudalnate/Original-Xbox-Gamesave-Resigners/blob/master/Mafia/source/XCalcSigLib/XCalcSigLib.cpp (**Unlicense**).
- Key derivation as described by the xboxsig author: https://www.ps2savetools.com/2015/03/25/python-script-to-generate-xbox-save-signing-keys

### 1.4 What an nxdk app needs to re-sign a save for another title
- **HMAC-SHA1** [V]: the kernel exports `XcHMAC`, `XcSHAInit`, `XcSHAUpdate` and
  `XcSHAFinal`, and nxdk declares them in `lib/xboxkrnl/xboxkrnl.h`.
  https://github.com/XboxDev/nxdk/blob/master/lib/xboxkrnl/xboxkrnl.h
- **XboxHDKey** [V]: exported as a kernel variable. nxdk declares
  `XBAPI XBOX_KEY_DATA XboxHDKey;`. Only needed for non-roamable games.
- **XboxCERTKey** [V, method]: the kernel does not export it. JayFoxRox found that it
  sits **16 bytes before `XboxHDKey`** in kernel memory. His script checks this by
  deriving the running XBE's LAN key and comparing it with `XboxLANKey`.
  - nxdk apps run in ring 0, so VMX World can read `((u8*)&XboxHDKey) - 16` at run time.
    It then checks the value by doing `HMAC(certkey, own_cert.LANKey)[0:16] == XboxLANKey`.
  - **This means we never ship a Microsoft key in our source or binary.** Keep it that way.
  - [U] We have not checked that this layout holds on every retail kernel and on
    modded or custom BIOSes. The self-check catches a mismatch, so on failure we refuse to sign.
- **The title's signature key**: read 16 bytes at cert+0xC0 from that game's
  `default.xbe` (1.2). Then follow 1.3.
- **Recommended code path:** write our own ~40 lines of C on top of `XcHMAC`.
  Use feudalnate's Unlicense code as the readable reference. Avoid GPL code
  (see the licence notes in 1.5) unless VMX World is GPL anyway.

### 1.5 Existing resigners and tools

| Tool | What it is | Licence | Status |
|---|---|---|---|
| **feudalnate / Original-Xbox-Gamesave-Resigners**: https://github.com/feudalnate/Original-Xbox-Gamesave-Resigners | Per-game C/C++ resigners (Black, Burnout 3, FlatOut 1/2, Half-Life 2, Mafia, NFS Most Wanted, Ninja Gaiden, PGR2, Splinter Cell PT/CT/DA, etc.), the shared `XCalcSigLib`, a list of XSavSig `resign.ini` profiles (`XSavSig.md`), and `XboxHDKey.md` | **Unlicense** | [V] |
| **xbox-save-sig** (`xboxsig.py`): https://github.com/PMStanley/xbox-save-sig | Derives the 16-byte save key from `default.xbe` or from the cert key as hex. Does not sign files. Can output in XBTF (trainer) format. | **MIT** | [V] |
| **GTASaveData** (whampson / thehambone): https://github.com/whampson/gta-save-data | .NET library. GTA III supports the **Xbox** format, including the signature. VC Xbox support is marked "TODO". | **No licence file**: all rights reserved, read-only reference | [V] |
| **gta3-save-editor** (whampson): https://github.com/whampson/gta3-save-editor | GUI built on GTASaveData | GPL-3.0 | [V] licence; [P] Xbox support comes through the library |
| **XSavSig005** | Closed Windows GUI resigner driven by `resign.ini` (`ID`, `SigKey`, `AuthKey`, `DataFile`, `SigType=NoRoam`) plus `hddkey.ini`. Has built-in profiles, e.g. Midnight Club 2 and NFS Hot Pursuit 2. | Freeware, no source | [P]: https://consolemods.org/wiki/Xbox:Games_with_Non-Roamable_(EEPROM-Locked)_Saves |
| ForzaSign | Forza-only resigner | unknown | [P] same consolemods page |
| Xport tools, "Xbox Save Resigner", XSATool, Le Fluffie | Named in the brief. Found no source or licence. Le Fluffie is an Xbox 360 tool as far as I know. | — | [U] |
| XBMC4Xbox trainer loader (see 3) | GPL-2.0 | GPL-2.0 | [V] |

**Non-roamable games** (consolemods list) [P]: Black, Burnout 3, DOA Ultimate/DOA2U/DOAXBV,
Far Cry Instincts Evolution, FlatOut 1/2, Forza, Half-Life 2, **NFS Most Wanted**,
Ninja Gaiden (+Black), PSO, PGR2, Richard Burns Rally, Tron 2.0.
**None of our candidate games is on that list**, except NFS:MW, which is not a candidate.
Source: https://consolemods.org/wiki/Xbox:Games_with_Non-Roamable_(EEPROM-Locked)_Saves
(behind Cloudflare; read through the archive.org snapshot of 2026-02-06).

---

## 2. Per game

Title IDs come from the xemu-project title database (`titles/<pub>/<n>/info.json`),
https://github.com/xemu-project/xdb [V]. Note that the repo states no licence.

### 2a. Grand Theft Auto III (Xbox): `5454000E` [V]
Released only in the Double Pack / Trilogy (EU, US, DE, JP).

- **Format** [V]:
  - The body is the PC block format: 20 size-prefixed blocks, then padding, then a
    4-byte checksum. Data size is 0x31400.
  - The Xbox build is detected by a player-ped size of 0x61E (PC uses 0x61A).
  - **Xbox appends a 20-byte HMAC-SHA1 signature after the checksum.** The PC file is
    0x3145C bytes, so the Xbox file is 0x3145C + 0x14.
  - Sources:
    - GTASaveData `SaveFileGTA3.cs` (`if (FileFormat.IsXbox) size += XboxHelper.SignatureLength;`, write path L449-456): https://github.com/whampson/gta-save-data/blob/master/GTASaveData.GTA3/SaveFileGTA3.cs
    - PC format: https://gtamods.com/wiki/Saves_(GTA_3) (site was returning 503; read through archive.org 2024-12-16)
- **Signature scope** [P]: the library's code takes `file.GetBytes()` after writing the
  checksum and HMACs `data[0 .. len-4]`. As written, that is everything except the
  trailing checksum dword. Signature type is **roamable**: the code uses the title key
  only, with no HDD key. **Test on hardware.**
- **Money** [V]: in Block 16 (Player Info), after two size dwords:
  - +0x00: `dword player money`
  - +0x0F: `dword money amount on screen`
  - Set both. The game rolls the HUD counter toward the real value.
  - Block offsets move around, so walk the block chain; do not use a fixed file offset.
- **Game checksum** [V]: the sum of all preceding bytes, stored as u32. It covers
  0x3145C bytes on PC. On Xbox it covers everything before the checksum. Recompute
  the checksum first, then the HMAC.
- **Save file name inside the folder** [U]: PC uses `GTA3sfN.b`. The Xbox name is not
  documented anywhere I could reach. List an existing save folder on the HDD to find it.
- **Editors** [V]: GTASaveData / gta3-save-editor (above). GTASum checksum fixer:
  https://github.com/whampson/GTASum (no licence).

### 2b. Grand Theft Auto: Vice City (Xbox): `5454000F` [V]
- **Format** [P]: the PC format is the GTA3-style block chain. Size is 0x31464, with the
  same sum-of-bytes checksum. Money is in Block 18 (Player Info) at +0x00
  (`dword current money`); the block size constant is 0x170.
  Source: https://gtamods.com/wiki/Saves_(GTA_VC) (read through archive.org).
- **Xbox specifics** [U]: GTASaveData lists an Xbox `FileFormat` for VC, but the
  load/size code says `// TODO: PS2, Xbox`.
  https://github.com/whampson/gta-save-data/blob/master/GTASaveData.VC/SaveFileVC.cs
  The best guess is the same scheme as GTA3 (same engine, same port team, same
  release): PC body, checksum, then a 20-byte roamable HMAC. **Test it.**
- **Money HUD field** [U]: VC also has an on-screen money field in Player Info.
  Find its offset by comparing two saves.

### 2c. Shenmue II (Xbox): `4D530032` [V] (US and EU only)
- **Currency is Hong Kong dollars ($), not yen** [V]. Yen was the currency in Shenmue I.
  The Dreamcast version converted carried-over yen into HK$.
  Source: Shenmue Wiki "Currency" and "Carrying Crates" pages (read through the fandom API), https://shenmue.fandom.com/wiki/Money
- **Save format / editors** [U]: I found **no documentation**, editor or money offset
  for the Xbox save.
  - The GitHub "Shenmue trainer unlimited money" repo
    (stackermanshield27/Shenmue-trainer-unlimited-money) looks like keyword-spam bait.
    Do not use it.
- **Memory addresses** [P]: there are Cheat Engine tables for the **PC HD remaster**,
  e.g. https://fearlessrevolution.com/viewtopic.php?t=30869 and
  https://fearlessrevolution.com/viewtopic.php?t=7723. Those addresses do not apply to the Xbox XBE.
- **Device codes** [U]: Cheat Code Central lists Codebreaker codes, PAL-only, for items.
  The page returned 403, so I could not see whether a money code exists.
  https://www.cheatcc.com/articles/shenmue-2-cheats-codes-unlockables-and-secrets-for-xbox/
- **Practical path**: find the money field by comparing saves (earn $10 at the crate
  job, save, compare). Then find the game's own checksum, if it has one. The Xbox
  signature is roamable unless proven otherwise (not on the consolemods list).
- **Economy anchors** [V]: crate carrying pays **$10 per crate**
  (https://shenmue.fandom.com/wiki/Carrying_Crates). Lucky Hit and gambling are the
  other income sources.

### 2d. Other candidates

| Game | Title ID [V] | Currency | What is documented | Rating |
|---|---|---|---|---|
| **GTA San Andreas** | `54540082` | $ | PC: 0x31800 bytes, 28 `BLOCK`-tagged blocks, sum-of-bytes checksum. Money in Block 15 (Player Info) at +0x04, on-screen money at +0x10 [V]: https://gtamods.com/wiki/Saves_(GTA_SA). Xbox wrapper/signature [U]; probably the same pattern as GTA3. | Good once the Xbox trailer is confirmed |
| **Midnight Club II** | `54540008` | $ (in-game cash) | XSavSig005 has a **built-in** profile, i.e. a known roamable key and data-file layout [P] (consolemods). Money offset [U]. | Easy to sign; money offset unknown |
| **NFS Underground 2** | `4541005A` | $ | PC profile: money u32 LE at **0xA16A** [P]: https://github.com/GComputeNerd/NFSU2-Save-Modify/blob/master/main.lua (tiny Lua script, no licence, does not touch any checksum). Xbox layout and game checksum [U]. Not on the non-roamable list. | Medium |
| **The Simpsons: Hit & Run** | `56550015` | coins | Rust PC save library: https://github.com/occanowey/share (`libhar`, chunk-based). Coin field and Xbox wrapper [U]. | Medium |
| **KOTOR** | `4C410003` (KOTOR II `4C410014`) | credits | KSE (Perl/Tk, open source, GPL-3.0 forks https://github.com/Fair-Strides/KotOR-Save-Editor, https://github.com/nadrino/kotor-savegame-editor) **supports Xbox saves partly**. Editing feats, powers, class or inventory gives "Damaged Savegame"; credits carry no such warning [P]: https://deadlystream.com/files/file/503-kotor-savegame-editor/. Saves are GFF/ERF. KSE does not sign. | Good: credits are a plain GFF field |
| **Morrowind** | `42530001` (GOTY `42530005`) | gold | No Xbox save editor found; the .ess format is mostly unknown [P]: https://forum.openmw.org/viewtopic.php?p=30397 | Poor |
| **Fable / TLC** | `4D53000D` / `4D5300D1` | gold | Nothing found [U] | Poor |
| **Jade Empire** | `4D53006D` | silver | Nothing found [U] | Poor |
| **The Sims: Bustin' Out** | `45410043` | simoleons | Nothing found for Xbox [U] (PS2/GC decomp projects exist) | Poor |

---

## 3. Trainer route (runtime memory patching)

### 3.1 How Xbox trainers worked [V]
- **Formats**:
  - `.etm` (Evox Trainer Module) is a header plus flat x86 "COM-style" code. Header
    offsets: 0x0A option states, 0x0E option labels, 0x12 list of target title IDs,
    0x16 entry point.
  - `.xbtf` is the same idea but scrambled. It adds a section pointer at 0x16 (support
    functions: SMBus, PatchIt, HookIt, in-game keys, LCD) and the entry point at 0x1A.
  - Sources:
    - XBMC4Xbox `Trainer.cpp` ("trainer loading generously donated by team xored"), GPL-2.0: https://github.com/antonic901/xbmc4xbox-redux/blob/HEAD/xbmc/platform/xbox/utils/Trainer.cpp (also in Rocky5/XBMC4Xbox-3.6)
    - Parser and samples: https://github.com/MrMilenko/Xbox-Trainer-Parser (no licence; Go)
- **Loader technique (XBMC/Evox)**:
  1. Allocate contiguous memory **below the kernel** at 0x7FFD2200 with
     `MmAllocateContiguousMemoryEx`, and mark it `MmPersistContiguousMemory` so it
     survives the reboot into the game.
  2. Copy in a small "loaderdata" stub plus the trainer.
  3. Store the pointer at kernel address 0x8000000C.
  4. With CR0 write-protect off, patch the kernel's `call [0x00010128]`, the call to the
     XBE entry point, so it goes through the stub instead.
  5. When the next XBE starts, the stub checks the title ID against the trainer's list.
     On a match it runs the trainer, which pokes or hooks game memory. Then it jumps to
     the real entry point.
  6. `RemoveTrainer` undoes the patch.

### 3.2 Can an nxdk app do this? [P]
Probably yes:
- nxdk code runs in kernel mode (ring 0).
- nxdk declares `MmAllocateContiguousMemoryEx`, `MmPersistContiguousMemory` and `LaunchDataPage`.
- nxdk has `XLaunchXBE` in `lib/hal/xbox.c`.

So VMX World could install a tiny stub, then `XLaunchXBE("D:\\default.xbe")` or the
HDD path. The stub would do one thing: once the game has started, write the purchased
amount to the money variable.

Risks:
- Kernel byte patterns differ between kernels. XBMC searches for them.
- The game's money address must be known per XBE version (NTSC/PAL/disc revision).
  Existing trainers encode those addresses. MrMilenko's parser is aimed at extracting them.
- Writing the value at entry point time is too early, because the save has not loaded
  yet. The stub needs a hook, or a timer/thread that waits for the player struct.
  That is real per-game reverse-engineering work.

**Conclusion:** save editing is simpler and safer for a v1 cashier.

---

## 4. Recommendation

**First three targets:**
1. **GTA III**: the only target where an open, working Xbox reader/writer *with the
   signature* exists. Format, money field and checksum are fully documented, and the save is roamable.
2. **GTA Vice City**: the same engine and Double Pack disc, so the same code path
   (block walk, sum checksum, HMAC trailer). It needs one hardware test of the Xbox trailer.
3. **KOTOR** (or **GTA SA** as the third GTA): KSE shows credits are editable on Xbox
   saves without breaking them. SA has a documented PC format but an untested Xbox
   wrapper. Hold **Shenmue II** until someone compares saves: nothing about its format
   is public, though it is a fine fourth target.

**Implementation order:**
1. Build the signer in C on `XcHMAC`, with the CERT key read at run time and checked (1.4).
2. Read the cert sig key from `default.xbe`.
3. Back up the save folder to `E:\UDATA\<tid>\` → our own backup dir.
4. Patch money and the HUD money field.
5. Recompute the game checksum.
6. Recompute the HMAC.
7. Verify by re-reading.
8. Keep a one-button restore.

**Conversion-rate ideas** (1 VMX chip = X):

| Game | Economy anchors | Suggested rate |
|---|---|---|
| GTA III | Missions pay $1,000–$30,000 (e.g. Luigi's Girls $1,500, Van Heist $20,000) [V]: https://gta.fandom.com/wiki/Missions_in_GTA_III | 1 chip = **$100**; 100 chips ≈ one mid mission |
| GTA VC | Properties cost $20,000–$120,000 (Malibu Club $120k) and earn $2k–$10k/day; Ammu-Nation heavy weapons $8k–$10k [V]: https://gta.fandom.com/wiki/Properties_in_GTA_Vice_City | 1 chip = **$250**; a 480-chip haul ≈ the Malibu |
| GTA SA | Similar scale to VC (properties tens of thousands) [U] | 1 chip = **$250** |
| Shenmue II | HK$10 per crate [V]; prices are small (capsule toys, inn) [U] | 1 chip = **HK$2** (5 chips ≈ one crate) |
| KOTOR | Credits; store items roughly 100s–1000s [U] | 1 chip = **10 credits** |

Cap each payout (for example, at one mission's worth per visit) so the cashier
supplements play rather than replacing it.

## Open questions / to test
- [U] GTA3 Xbox: confirm the save file name, and that the HMAC covers the body excluding the checksum.
- [U] VC/SA Xbox: confirm the trailer layout (expected: checksum + 20-byte HMAC).
- [U] XboxCERTKey-at-HDKey−16: confirm on stock 1.0–1.6 kernels and on the owner's BIOS.
- [U] Shenmue II save structure; KOTOR Xbox credits field and any `.sig`.
- [U] XSavSig005 built-in Midnight Club II profile: which data file, and where the money is.
