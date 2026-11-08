# Soft Beads firmware mod

Personal firmware mod for a Mutable Instruments Beads (granular texture synth, STM32H743 at 400 MHz). Goal: Instruo Arbhar's soft, smooth grain character while keeping Beads' always-on live processing. Dry signal stays clear; changes target the wet grain path.

Full spec: https://claude.ai/artifact/Nzy7HQb927z7DKiCY4a2Gs

## Repo

- Fork of `pichenettes/eurorack`. Work lives in `beads/`. `stmlib` and `stm_audio_bootloader` are submodules.
- Code is MIT. Keep license headers intact. Don't use "Mutable Instruments" or "Beads" as the name of anything we publish.
- Upstream README: builds on desktop, not verified on a module. Treat every first flash as a test.

## Workflow

1. Make a DSP change.
2. Build the desktop harness (`make -f beads/test/makefile`) and render WAVs: `./beads_test TestProcessor` (or `./beads_test list`). Source audio is `audio_samples/kettel.wav`; regenerate with `python3 beads/test/make_test_source.py` or drop in a 48 kHz stereo 16-bit WAV. Stock reference renders live in `renders/stock/` (git-ignored).
3. A/B against stock renders by ear before touching hardware.
4. Firmware build: `./firmware.sh` (wraps `make -f beads/makefile`). Uses Arm GNU Toolchain 15.3 from `/Applications/ArmGNUToolchain/15.3.rel2/` and pins `-std=gnu++11`, since GCC 15's C++17 default rejects `register` in ST's HAL headers. It prints how far the image reaches into flash and fails if it overlaps the settings sector. Stock image ends at 0x0805ABB0.
5. Flash over SWD with Jesse's STM32 programmer. `make -f beads/makefile wav` builds an audio-bootloader update file as an alternative.
6. Never erase the settings sector (calibration): sector 15, 0x081E0000 to 0x081FFFFF, 128 KB (verified in `stmlib/system/sector_storage_h7xx.h` and `beads/settings.h`). If it's blank or corrupt the firmware silently rewrites defaults, so back it up with STM32CubeProgrammer before the first custom flash.

Commit one change per commit so each can be reverted and A/B tested in isolation.

## Planned changes, in order

| # | Change | Where |
| --- | --- | --- |
| 1 | Fix wet gain: wet is 1.5, dry 0.75, so wet sits about 6 dB hot | `beads/dsp/processor.cc`, dry/wet balance |
| 2 | Raise minimum grain attack (currently 0.5 ms) | `beads/dsp/grain_envelope.h`, `min_attack_` |
| 3 | Lengthen voice-stealing fade (currently 5 ms) | `beads/dsp/granular_player.h`, `kInstantCrossfadeTime` |
| 4 | Pitch quantiser with scale masks: off, chromatic, major, minor, pentatonic, fifths and octaves, octaves. Relative to source pitch. Quantise knob plus CV plus random, once per grain, with hysteresis. Select by holding Quality and turning Pitch | `beads/cv_reader.h` (`LAW_DETUNE` table), `beads/dsp/randomized_parameter.h` (`pitch_value()`), speed calc in `beads/dsp/granular_processor.h`, `beads/ui.cc` |
| 5 | Port Clouds' diffuser onto the wet grain path, before the mix; fold into the soft end of the Shape knob. Scale delay lengths for 48 kHz | `clouds/dsp/fx/diffuser.h` into `beads/dsp/fx/` |
| 6 | Envelope-sum wet normalisation instead of per-grain 1/√n at birth | `beads/dsp/granular_player.h` |
| 7 | Optional: grain-only reverb (reverb currently runs after the dry/wet mix) | `beads/dsp/processor.cc` |

Measure CPU (debug pin in `beads/drivers/debug_pin.h`) before adding anything per-grain.

## Existing button combos (don't collide)

Seed + Freeze, Assign + Seed, Assign + Freeze, Assign + Quality (calibration), hold Quality + Feedback pot (manual gain), hold Assign + mix pots. Quality + Seed and Quality + Freeze exist but are compiled out (`ENABLE_ADVANCED_FEATURES` in `ui.h`).

## Working with Jesse

- Australian English. Direct, explain trade-offs, recommend one option.
- Senior designer and front-end developer, newer to embedded C++ and DSP: explain hardware and DSP specifics, skip general programming basics.

## Agent skills

### Issue tracker

Issues live as local markdown files under `.scratch/<feature>/`, committed with the repo (the GitHub repo's Issues hold inherited upstream issues, so don't use them). See `docs/agents/issue-tracker.md`.

### Triage labels

Default five roles (`needs-triage`, `needs-info`, `ready-for-agent`, `ready-for-human`, `wontfix`), recorded as a `Status:` line in each issue file. See `docs/agents/triage-labels.md`.

### Domain docs

Single-context: `GLOSSARY.md` and `docs/adr/` at the repo root, created lazily. See `docs/agents/domain.md`.
