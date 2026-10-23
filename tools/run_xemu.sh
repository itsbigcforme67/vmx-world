#!/bin/sh
# Boots xbox/build/xbox/vmxworld.iso (tools/build_xbox.sh) in xemu, hidden on its own Xvfb display, with an EMPTY
# /dev/input so it can't see the owner's real controllers (a pad script, D:\demo.txt, drives World instead). Uses the
# owner's MCPX ROM and BIOS dumps from the mh1-xbox repo's xbox_files/ (never copied here) and a scratch hard disk,
# xbox/build/xemu/hdd.img, formatted with the mh1-xbox repo's tools/xbox_fatx.py so World has an E: drive.
#   tools/run_xemu.sh --shot out.png --after "20 40"     screenshots at those seconds, then quit
#   --fresh   a new, empty hard disk first;  --record out.mp4  also record
#   --xbe FILE  boot a DVD holding only FILE;  --net  xemu's NAT network (the laptop is 10.0.2.2);  --hdd FILE  another disk
#   --mu FILE  a memory unit (a raw 8 MB image, e.g. a VMX's: vmx-firmware/build/vmxmu) in controller 1's first slot
#              (controller 1 is then xemu's keyboard pad; the pad script still drives World)
set -e
cd "$(dirname "$0")/.."
XF=${XBOX_FILES:-"$HOME/claude projects/MH XBOX/mh1-xbox/xbox_files"}
MH="$HOME/claude projects/MH XBOX/mh1-xbox"
FOLD="$PWD/../microgames/fold"
XEMU=${XEMU:-$HOME/xboxdev/xemu/xemu-0.8.136-x86_64.AppImage}
SHOT=""; AFTER=30; REC=; FRESH=; XBE=; NET=; HDDARG=; MU=
while [ $# -gt 0 ]; do
    case "$1" in
    --shot) SHOT=$2; shift 2 ;; --after) AFTER=$2; shift 2 ;; --record) REC=$2; shift 2 ;; --fresh) FRESH=1; shift ;;
    --xbe) XBE=$2; shift 2 ;; --net) NET=1; shift ;; --hdd) HDDARG=$2; shift 2 ;; --mu) MU=$(realpath "$2"); shift 2 ;;
    *) echo "unknown option $1"; exit 2 ;;
    esac
done
for f in mcpx.bin bios.bin; do [ -f "$XF/$f" ] || { echo "missing $XF/$f (the owner's own dumps)"; exit 1; }; done
W=$PWD/xbox/build/xemu; mkdir -p "$W/home/xemu/xemu"
ISO=${ISO:-$PWD/xbox/build/xbox/vmxworld.iso}; [ -f "$ISO" ] || { echo "no $ISO: run tools/build_xbox.sh"; exit 1; }
HDD=${HDDARG:-$W/hdd.img}
if [ -n "$XBE" ]; then                      # a DVD holding only that XBE
    rm -rf "$W/iso_other"; mkdir -p "$W/iso_other"; cp "$XBE" "$W/iso_other/default.xbe"
    ISO=$W/other.iso; rm -f "$ISO"; ~/xboxdev/nxdk/tools/extract-xiso/build/extract-xiso -c "$W/iso_other" "$ISO" >/dev/null
fi
if [ -n "$FRESH" ] || [ ! -f "$HDD" ]; then rm -f "$HDD"; truncate -s 8G "$HDD"; python3 "$MH/tools/xbox_fatx.py" format "$HDD" >/dev/null; fi
cat > "$W/home/xemu/xemu/xemu.toml" <<TOML
[general]
show_welcome = false
[sys.files]
bootrom_path = '$XF/mcpx.bin'
flashrom_path = '$XF/bios.bin'
eeprom_path = '$W/eeprom.bin'
hdd_path = '$HDD'
dvd_path = '$ISO'
TOML
[ -z "$MU" ] || printf "[input.bindings]\nport1_driver = 'usb-xbox-gamepad'\nport1 = 'keyboard'\n[input.peripherals.port1]\nperipheral_type_0 = 1\nperipheral_param_0 = '%s'\n" "$MU" >> "$W/home/xemu/xemu/xemu.toml"
[ -z "$NET" ] || printf '[net]\nenable = true\nbackend = '"'"'nat'"'"'\n' >> "$W/home/xemu/xemu/xemu.toml"
export XDG_DATA_HOME="$W/home"
DBG="$W/serial.txt"; : > "$DBG"
XVFB=${XVFB:-$HOME/xboxdev/xvfb/root/usr/bin/Xvfb}
. "$FOLD/tools/emu_lib.sh"
PID=; XPID=; RPID=
stop() { [ -n "$RPID" ] && kill -INT $RPID 2>/dev/null && emu_sleep 2; emu_stop $PID $RPID $XPID; }
emu_traps stop
DNUM=$(( 70 + $$ % 20 ))
"$XVFB" :$DNUM -screen 0 1280x960x24 -nolisten tcp >/dev/null 2>&1 &
XPID=$!; export DISPLAY=:$DNUM; emu_sleep 2
# no real pads: xemu gets a private, empty /dev/input
unshare -rm sh -c 'mount -t tmpfs none /dev/input && exec "$0" "$@"' "$XEMU" -device lpc47m157 -serial "file:$DBG" -audio none >"$W/xemu.log" 2>&1 &
PID=$!
if [ -n "$REC" ]; then
    ffmpeg -v error -y -f x11grab -framerate 15 -video_size 1280x960 -i "$DISPLAY" -vf scale=640:480 \
        -c:v libx264 -preset veryfast -crf 24 -pix_fmt yuv420p "$REC" </dev/null & RPID=$!
fi
T=0
for A in $AFTER; do
    emu_sleep $(( A - T )); T=$A
    OUT=$SHOT; [ "$A" = "$AFTER" ] || OUT="${SHOT%.png}_${A}s.png"
    ffmpeg -v error -y -f x11grab -i "$DISPLAY" -frames:v 1 "$OUT" </dev/null; echo "screenshot: $OUT"
done
