#!/bin/sh
# The Xbox kernel's FATX driver against vmx-firmware/core/fatx.c (the VMX's memory-unit volume): makes a 16-bit FATX
# volume with build/vmxmu on a scratch hard disk's partition 3 (X:, 750 MB), boots xbox/tests/kernel_fatx in xemu
# (hidden), which lists and reads it all and writes, replaces and deletes files through the kernel, then reads the
# result back with build/vmxmu. Passes if the kernel saw exactly what we wrote and we see what the kernel wrote.
set -e
cd "$(dirname "$0")/.."
R=$PWD; FW=$R/../vmx-firmware; MU=$FW/build/vmxmu
(cd "$FW" && make -s build/vmxmu)
. "$HOME/xboxdev/env.sh"; mkdir -p "$HOME/xboxdev/links"; ln -sfn "$R" "$HOME/xboxdev/links/vmxworld"
(cd "$HOME/xboxdev/links/vmxworld/xbox/tests/kernel_fatx" && make >/dev/null)
W=$R/xbox/build/kfatx; rm -rf "$W"; mkdir -p "$W"
P3=0x80000; P3LEN=0x2EE00000
truncate -s $((P3LEN)) "$W/x.img"
"$MU" "$W/x.img" format 750 >/dev/null
printf 'hello from the VMX\n' > "$W/a.txt"; head -c 70000 "$FW/build/games/BLOX.VXP" > "$W/b.bin"
"$MU" "$W/x.img" put "$W/a.txt" a.txt; "$MU" "$W/x.img" put "$W/b.bin" deep/er/b.bin; "$MU" "$W/x.img" install "$FW/build/games/9BALL.VXP" >/dev/null
"$MU" "$W/x.img" put "$W/a.txt" replace.bin; "$MU" "$W/x.img" put "$W/a.txt" delete.me
truncate -s 8G "$W/hdd.img"; dd if="$W/x.img" of="$W/hdd.img" bs=64K seek=$((P3 / 65536)) conv=notrunc,sparse status=none
tools/run_xemu.sh --xbe xbox/build/xbox/kernel_fatx/default.xbe --hdd "$W/hdd.img" --shot "$W/shot.png" --after 25 >/dev/null
cp xbox/build/xemu/serial.txt "$W/serial.txt"; cat "$W/serial.txt"
dd if="$W/hdd.img" of="$W/x.img" bs=64K skip=$((P3 / 65536)) count=$((P3LEN / 65536)) status=none
echo "-- read back:"; "$MU" "$W/x.img" tree
ok=1
want() { grep -q "$1" "$W/serial.txt" || { echo "MISSING in the kernel's view: $1"; ok=0; }; }
sumf() { python3 -c "import sys;s=0
for c in open(sys.argv[1],'rb').read(): s=(s*31+c)&0xffffffff
print('%08x'%s)" "$1"; }
want "F X:\\\\a.txt 19 $(sumf "$W/a.txt")"
want "F X:\\\\deep\\\\er\\\\b.bin 18664 $(sumf "$W/b.bin")"
want "F X:\\\\56580001\\\\565810090000\\\\GAME.VXP $(stat -c %s "$FW/build/games/9BALL.VXP") $(sumf "$FW/build/games/9BALL.VXP")"
want "KFATX: done"
[ "$("$MU" "$W/x.img" get kernel.txt)" = "$(printf 'written by the Xbox kernel\r')" ] || { echo "kernel.txt wrong"; ok=0; }
"$MU" "$W/x.img" get kdir/inner.bin > "$W/inner.bin"; python3 -c "import sys;d=open(sys.argv[1],'rb').read();sys.exit(not(len(d)==40000 and all(d[i]==(i*7)&255 for i in range(40000))))" "$W/inner.bin" || { echo "kdir/inner.bin wrong"; ok=0; }
[ "$("$MU" "$W/x.img" get replace.bin)" = "replaced by the kernel" ] || { echo "replace.bin wrong"; ok=0; }
"$MU" "$W/x.img" get delete.me >/dev/null 2>&1 && { echo "delete.me still there"; ok=0; }
[ $ok = 1 ] && echo "kernel FATX check: PASS" || { echo "kernel FATX check: FAIL"; exit 1; }
