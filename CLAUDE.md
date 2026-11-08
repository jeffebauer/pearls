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
3. A/B against stock renders by ear before touching hardware, on the listening page: `python3 beads/test/listen.py` (or the `listen` preview), then http://127.0.0.1:8765. Every option plays in sync, switching is click-free (1–9, ←/→), level matching defaults to LUFS (attenuate only), and there is a blind mode. Link a comparison with `#path,path`.
4. Firmware build: `./firmware.sh` (wraps `make -f beads/makefile`). Uses Arm GNU Toolchain 15.3 from `/Applications/ArmGNUToolchain/15.3.rel2/` and pins `-std=gnu++11`, since GCC 15's C++17 default rejects `register` in ST's HAL headers. It prints how far the image reaches into flash and fails if it overlaps the settings sector. Stock image ends at 0x0805ABB0.
5. Flash over SWD with Jesse's STM32 programmer. `make -f beads/makefile wav` builds an audio-bootloader update file as an alternative.
6. Never erase the settings sector (calibration): sector 15, 0x081E0000 to 0x081FFFFF, 128 KB (verified in `stmlib/system/sector_storage_h7xx.h` and `beads/settings.h`). If it's blank or corrupt the firmware silently rewrites defaults, so back it up with STM32CubeProgrammer before the first custom flash.

Commit one change per commit so each can be reverted and A/B tested in isolation.

## Planned changes, in order

Settled by grilling on 8 November 2026. The spec and one ticket per row are in `.scratch/soft-character/`. Decisions with lasting weight are in `docs/adr/`, and parked ideas are in `.scratch/backlog/parked.md`.

| # | Change | Where |
| --- | --- | --- |
| 0 | Measurement setup: a render tool (wet vs dry level, click detector on 1 ms frames, grain attack time), guitar clip 09 as the reference source, reference patches, and Arbhar and Multigrain reference captures | `beads/test/`, `docs/reference-patches.md`, `renders/reference/` |
| 1 | Done in `d4381c0`: wet gain from 1.5 to a measured 0.94, so a dense cloud at 12 o'clock is within 1 dB of the dry | `beads/dsp/processor.cc`, dry/wet balance |
| 2 | Done in `bcf5e36` (CPU on the debug pin still to measure): envelope-sum wet normalisation: bus gain 1/max(Σenv, 1), dropping at once and recovering slowly, replacing per-grain 1/√n at birth (ADR 0004, amended to linear) | `beads/dsp/granular_player.h` |
| 3 | Soft saturator on the wet bus (tanh-style, from about −6 dBFS); the stock pre-reverb limiter stays | `beads/dsp/processor.cc` |
| 4 | Raise minimum grain attack (currently 0.5 ms); value chosen from a render sweep | `beads/dsp/grain_envelope.h`, `min_attack_` |
| 5 | Lengthen voice-stealing fade (currently 5 ms); value chosen from a render sweep | `beads/dsp/granular_player.h`, `kInstantCrossfadeTime` |
| 6 | Shape sweep as one continuous morph: decay, square, Tukey, Hann, Gaussian. Reverse dropped (ADR 0003) | `beads/dsp/grain_envelope.h` |
| 7 | Clouds' diffuser on the wet path, before the mix, fading in from Hann to fully clockwise. Lines sized for 48 kHz, read at runtime offsets scaled by sample rate | `clouds/dsp/fx/diffuser.h` into `beads/dsp/fx/` |
| 8 | Random per-grain level, driven by Density's random half, downward only to −6 dB | `beads/dsp/granular_player.h` |
| 9 | Density sets overlap rather than grains per second when no clock is patched; clocked behaviour stays stock | `beads/dsp/trigger_generator.cc`, `beads/dsp/granular_processor.h` |
| 10 | Stereo width: a ±30% pan floor on the wet path plus opposite-polarity random level on L and R; mono stays mono | `beads/dsp/granular_player.h` |
| 11 | Freeze seam: measure first, and add a crossfade only if the click detector finds a click | `beads/dsp/delay_line.h` |
| 12 | Pitch quantiser with scale masks: off, chromatic, major, minor, pentatonic, fifths and octaves, octaves. Relative to source pitch. Quantise knob plus CV plus random, once per grain, with hysteresis. Select by holding Quality and turning Pitch. Gets its own short grilling first | `beads/cv_reader.h` (`LAW_DETUNE` table), `beads/dsp/randomized_parameter.h` (`pitch_value()`), speed calc in `beads/dsp/granular_processor.h`, `beads/ui.cc` |

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
