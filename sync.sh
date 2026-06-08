#!/bin/bash
# Sync sources from Windows to WSL and rebuild
set -e

SRC=/mnt/d/\!code/screen_system
DST=~/screen_system

echo "Syncing $SRC -> $DST ..."
rsync -av --delete \
    --exclude build/ \
    --exclude .git/ \
    --exclude .vscode/ \
    "$SRC"/ "$DST"/

cd "$DST"
mkdir -p build && cd build

echo "Building..."
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
echo ""
echo "Done. Run: cd ~/screen_system/build && ./ScreenInferenceSystem"
