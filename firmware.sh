#!/bin/sh
# Build the firmware with a modern Arm GNU toolchain.
#
# GCC 15 defaults to C++17, which rejects the `register` keyword in ST's HAL
# headers. Pinning gnu++11 restores the behaviour the code was written for.
#
# Usage:
#   ./firmware.sh          build build/beads/beads.elf and .hex
#   ./firmware.sh wav      build the audio-bootloader update file
#   ./firmware.sh clean
#
# Override the toolchain with TOOLCHAIN_PATH=/path/to/arm-none-eabi/ ./firmware.sh

set -e
cd "$(dirname "$0")"

TOOLCHAIN_PATH="${TOOLCHAIN_PATH:-/Applications/ArmGNUToolchain/15.3.rel2/arm-none-eabi/}"

make -f beads/makefile \
  TOOLCHAIN_PATH="$TOOLCHAIN_PATH" \
  CPPFLAGS="-fno-exceptions -fno-rtti -std=gnu++11" \
  -j8 "$@"

if [ "$#" -eq 0 ] && [ -f build/beads/beads.elf ]; then
  # Report how far the image reaches into flash. Settings (calibration) live
  # in sector 15 at 0x081E0000 and must never be overwritten.
  END=0
  for SEG in $("${TOOLCHAIN_PATH}bin/arm-none-eabi-readelf" -lW build/beads/beads.elf |
      awk '$1 == "LOAD" && $4 ~ /^0x08/ { print $4 ":" $5 }'); do
    SEG_END=$((${SEG%%:*} + ${SEG##*:}))
    [ "$SEG_END" -gt "$END" ] && END=$SEG_END
  done
  LIMIT=$((0x081E0000))
  printf 'Flash image ends at 0x%08X, %d KB below the settings sector.\n' "$END" $(((LIMIT - END) / 1024))
  if [ "$END" -gt "$LIMIT" ]; then
    echo "ERROR: firmware overlaps the settings sector. Do not flash." >&2
    exit 1
  fi
fi
