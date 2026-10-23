#!/bin/sh
# Builds VMX World for the original Xbox: build/xbox/VMXWORLD/ (XBE + assets + catalog) and build/xbox/vmxworld.iso,
# in vmx-world/xbox/. Needs nxdk (~/xboxdev, ~/xboxdev/env.sh, as for FOLD and the mh1-xbox port) and the VMX games
# built in vmx-firmware (tools/build_game.sh). make can't take the space in the repo's path, so this builds through a
# symlink, ~/xboxdev/links/vmxworld -> this repo (the Makefile's paths to microgames/ and vmx-firmware/ are relative).
#   tools/build_xbox.sh [DEMO=tests/xemu_demo.txt]     (the path is relative to xbox/)
#   tools/build_xbox.sh --loader                      the network loader (xbox/build/xbox/loader/default.xbe)
set -e
R=$(cd "$(dirname "$0")/.." && pwd)
mkdir -p "$HOME/xboxdev/links"; ln -sfn "$R" "$HOME/xboxdev/links/vmxworld"
. "$HOME/xboxdev/env.sh"
if [ "$1" = --loader ]; then shift; cd "$HOME/xboxdev/links/vmxworld/xbox/loader" && make "$@"; exit; fi
cd "$HOME/xboxdev/links/vmxworld/xbox" && make package "$@"
