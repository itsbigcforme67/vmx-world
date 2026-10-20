#!/usr/bin/env bash
# Copy the VMX simulator's game adapters + cores (Pocket Arcade) and FOLD ROOMS (microgames) into mockup/vmx/.
# Never edit the copies; change them at home and run this again. Star Raiders' cart is never copied.
set -euo pipefail
cd "$(dirname "$0")/.."
PA="${POCKET_ARCADE:-../POCKET ARCADE/pocket-arcade}/vmx"
MG="${MICROGAMES:-../microgames}"
mkdir -p mockup/vmx/cores mockup/img
cp "$PA/games.js" "$PA/vmx-front.webp" mockup/vmx/
for f in "$PA"/cores/*; do case "$(basename "$f")" in starraiders*|cpu6502.js) ;; *) cp "$f" mockup/vmx/cores/ ;; esac; done
cp "$MG/ui/games/foldrooms_core.js" mockup/vmx/cores/
cp "$MG/fold/docs/builder-level-view.png" mockup/img/fold-builder.png
python3 tools/asset_png.py "$MG/fold/assets/metal.rgb" mockup/img/metal.png
python3 tools/asset_png.py "$MG/fold/assets/vmx_large.rgba" mockup/img/vmx-head.png
echo "synced into mockup/vmx/"
