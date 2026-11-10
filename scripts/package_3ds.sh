#!/bin/sh
# Builds dist/StayingAlive-3DS.cia from the compiled .elf.  Usage: package_3ds.sh <dir with makerom + bannertool>
set -e
T=${1:-.}
mkdir -p dist build3ds
"$T/bannertool" makebanner -i assets/icons/banner.png -a assets/icons/banner.wav -o build3ds/banner.bnr
"$T/bannertool" makesmdh -s "Staying Alive" -l "Scavenge. Survive. Rebuild." -p "JustGoodinc" -i assets/icons/icon.png -o build3ds/icon.icn
"$T/makerom" -f cia -o dist/StayingAlive-3DS.cia -elf StayingAlive.elf -rsf platform/n3ds/cia.rsf \
  -target t -exefslogo -icon build3ds/icon.icn -banner build3ds/banner.bnr
cp StayingAlive.3dsx dist/StayingAlive-3DS.3dsx
echo "CIA packaged"
