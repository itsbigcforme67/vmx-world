"""Wire a finished VMX CASINO table into the simulator, World, the firmware test list and the microgames README.
The table itself (JS core, page, film, C# twin, game.ini, card.txt, parity adapter) is made first, following
docs/CASINO_TABLE_BRIEF.md. Safe to run again: a table already wired in is skipped.
   python3 tools/add_casino_table.py GAME "CARD LINE 1|LINE 2" "TITLE" BLOCKS "World blurb" "README line"
then: POCKET ARCADE tools/vmx-sync.sh, vmx-world tools/sync.sh, vmx-firmware tools/build_game.sh GAME."""
import json, os, re, sys

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..")
PA, FW, MG, VW = (os.path.join(ROOT, d) for d in ("POCKET ARCADE/pocket-arcade", "vmx-firmware", "microgames", "vmx-world"))

def edit(path, fn):
    s = open(path).read(); t = fn(s)
    if t != s: open(path, "w").write(t); print("  updated", os.path.relpath(path, ROOT))

def main(g, lines, title, blocks, blurb, readme):
    ini = dict(l.strip().split("=", 1) for l in open(f"{FW}/games/{g}/game.ini") if "=" in l)
    tid = ini["id"]; assert ini.get("casino") == "1", f"{g}: game.ini needs casino=1"
    rows = [l.rstrip("\n") for l in open(f"{FW}/games/{g}/card.txt")][:24]
    name = lines.split("|")
    import glob
    cs = open(glob.glob(f"{FW}/games/{g}/*.cs")[0]).read()
    usesB = "false" if re.search(r"UsesB\s*\{\s*get\s*\{\s*return false", cs) else "true"   # as the C# twin says

    def games_js(s):                                   # the simulator's adapter, before TRICKY WICKS
        if f'id: "{g}"' in s: return s
        e = (f'  {{\n    id: "{g}", name: {json.dumps(name)}, title: "{title}", usesB: {usesB}, casino: true,\n'
             f'    load: () => import("./cores/{g}_core.js"),\n    start: (m, seed, demo, bank) => m.newGame(seed, bank),\n'
             f'    tick(g, {{ held: h, demo }}) {{ this.m.step(g, demo ? this.m.demo(g) : h); }},\n'
             f'    record: bigWin, score: rec => ["BIG WIN", String(rec.best || 0)],\n    art: rowsArt({json.dumps(rows)}),\n  }},\n')
        return s.replace('  {\n    id: "tricky",', e + '  {\n    id: "tricky",', 1)
    edit(f"{PA}/vmx/games.js", games_js)

    def sw(s):
        if f"'{g}_core.js'" in s: return s
        s = s.replace("'pool_core.js',", f"'pool_core.js', '{g}_core.js',", 1)
        return re.sub(r"vmx-v(\d+)", lambda m: f"vmx-v{int(m.group(1)) + 1}", s, 1)
    edit(f"{PA}/vmx/sw.js", sw)
    edit(f"{PA}/tools/vmx-sync.sh", lambda s: s if f" {g}_core.js" in s else s.replace("pool_core.js ", f"pool_core.js {g}_core.js ", 1))

    def world(s):                                      # GET GAMES (casino shelf) + the cashier's list of casino games
        if f'id: "{g}"' in s: return s
        e = (f'  {{ id: "{g}", tid: {tid}, pub: "VMX Studio", blocks: {blocks}, shelf: "casino", title: "{title}", '
             f'applet: "CASINO CASHIER", blurb: {json.dumps(blurb)} }},\n')
        s = s.replace('  { id: "checkers", tid:', e + '  { id: "checkers", tid:', 1)
        return s.replace('from: ["pool", "blackjack",', f'from: ["pool", "blackjack", "{g}",', 1)
    edit(f"{VW}/mockup/world.js", world)

    edit(f"{FW}/Makefile", lambda s: s if re.search(rf"\b{g}\b", s.split("test:")[1].split("\n")[1]) else
         s.replace("; do tools/parity/check.sh", f" {g}; do tools/parity/check.sh", 1))

    def readme_md(s):
        if f"`{g}_core.js`" in s: return s
        row = f"| {title} | {readme}; VMX CASINO | `{g}.html`, `{g}_core.js` | `anim/{g}/` |\n"
        s = s.replace("| INVASION | Space Invaders-style", row + "| INVASION | Space Invaders-style", 1)
        return s.replace("- Ours: BLOX,", f"- Ours: {title}, BLOX,", 1)
    edit(f"{MG}/README.md", readme_md)

if __name__ == "__main__":
    a = sys.argv[1:]
    if len(a) != 6: sys.exit(__doc__)
    main(a[0], a[1], a[2], int(a[3]), a[4], a[5])
