#!/bin/bash
# VMX World's network loader in xemu, hidden, on a SCRATCH hard disk (xbox/build/loader_xemu/hdd.img), as on the real
# Xbox after tools/xbox_upload.sh: E:\Games\VMXWORLD\default.xbe (the loader) + loader.txt (10.0.2.2:PORT, the laptop
# as xemu's NAT network sees it). FOLD's test DVD (microgames/fold xbox/testlaunch) starts the loader from the disk
# as the dashboard would; tools/xbox_serve.py serves the current build (tools/build_xbox.sh first).
#   1. nothing on the disk: the loader fetches /version and /pack, writes the build, starts World
#   2. again: the build is up to date, no /pack, World starts
# Uses the mh1-xbox repo's tools/xbox_fatx.py and the owner's BIOS files, read only.
set -e
cd "$(dirname "$0")/.."
T=$PWD/xbox/build/loader_xemu
MH="$HOME/claude projects/MH XBOX/mh1-xbox"
FOLD="$PWD/../microgames/fold"
fatx() { python3 "$MH/tools/xbox_fatx.py" "$@"; }
PORT=8771
. "$FOLD/tools/emu_lib.sh"
tools/build_xbox.sh --loader >/dev/null
[ -f xbox/build/xbox/VMXWORLD/default.xbe ] || { echo "no build: tools/build_xbox.sh"; exit 1; }
[ -f "$FOLD/build/xbox/testlaunch/default.xbe" ] || (cd "$FOLD" && tools/build_xbox.sh --testlaunch >/dev/null)
rm -rf "$T"; mkdir -p "$T"
truncate -s 8G "$T/hdd.img"; fatx format "$T/hdd.img" >/dev/null
printf '10.0.2.2:%s\n' $PORT > "$T/loader.txt"
fatx put "$T/hdd.img" xbox/build/xbox/loader/default.xbe Games/VMXWORLD/default.xbe >/dev/null
fatx put "$T/hdd.img" "$T/loader.txt" Games/VMXWORLD/loader.txt >/dev/null
SPID=
unserve() { [ -z "$SPID" ] || { kill "$SPID" 2>/dev/null || :; wait "$SPID" 2>/dev/null || :; SPID=; }; }
emu_traps unserve
python3 tools/xbox_serve.py $PORT > "$T/serve.log" 2>&1 & SPID=$!; sleep 1
boot() {                                    # boot RUN SECONDS
    printf 'E:\\Games\\VMXWORLD\\default.xbe\n' > "$T/launch.txt"; fatx put "$T/hdd.img" "$T/launch.txt" launch.txt >/dev/null
    tools/run_xemu.sh --xbe "$FOLD/build/xbox/testlaunch/default.xbe" --net --hdd "$T/hdd.img" --shot "$T/run$1.png" --after "$2" >/dev/null 2>&1 & wait $! || :
    cp xbox/build/xemu/serial.txt "$T/serial_$1.txt"
}
FAILS=0
check() { if eval "$2"; then echo "PASS: $1"; else echo "FAIL: $1"; FAILS=$((FAILS + 1)); fi; }
boot 1 "30 50"
check "1: loader asked for the version" "grep -q 'GET /version' '$T/serve.log'"
check "1: loader downloaded the build" "grep -q 'GET /pack' '$T/serve.log'"
check "1: World started (its serial line)" "grep -q 'WORLD: ready' '$T/serial_1.txt'"
check "1: the catalogue came along" "fatx ls '$T/hdd.img' Games/VMXWORLD/catalog | grep -q 'VIDEOPOK.VXP'"
check "1: World made its UDATA title" "fatx ls '$T/hdd.img' UDATA/56580001 | grep -q TitleMeta.xbx"
: > "$T/serve.log.2"; cp "$T/serve.log" "$T/serve.log.1"
boot 2 "30 40"
check "2: no second download" "[ \$(grep -c 'GET /pack' '$T/serve.log') = 1 ]"
check "2: World started again" "grep -q 'WORLD: ready' '$T/serial_2.txt'"
echo "$FAILS failed"; exit $FAILS
