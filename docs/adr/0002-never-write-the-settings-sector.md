# Never write the settings sector

Sector 15 (0x081E0000 to 0x081FFFFF, 128 KB) holds the module's factory calibration. If it is blank or corrupt, the firmware silently writes defaults over it, and calibration is lost for good. So no build, flash or erase may ever touch it. The linker script would let an oversized image run into it, so `./firmware.sh` checks the image's end address and fails if it reaches 0x081E0000. Never work around that failure, and never flash with a bare `make` or a full-chip erase. Before the first custom flash, the sector is read out into `backups/` (git-ignored) and copied somewhere off this machine.

## Consequences

- Flash space is capped at 0x081E0000. A feature that doesn't fit gets cut. It never moves into sector 15.
- Recovery goes through the backup and the SWD programmer, not the audio bootloader.
