# Next session: 8 November 2026 (setup done; Matt Pocock flow next)

## Where things stand

On `master`. Repo set up: a clone of `pichenettes/eurorack` with submodules, `origin` =
`jeffebauer/pearls` (public fork), `upstream` = Émilie's repo (pull only). Commits so far: the
macOS harness fixes (`b1dee73`), the firmware build script and notes (`da78018`), and the session
file plus render level check (`38bbcf8`). `origin/master` is at `da78018`. Whether `38bbcf8` and
this handoff's commit were pushed depends on Jesse's answer at this wrap; `/session-start`'s
ahead/behind line settles it. **No DSP changes yet. Nothing flashed. No settings backup exists.**
The stock firmware builds with Arm GNU Toolchain 15.3 via `./firmware.sh` but has never run on the
module.

## Start here

Run `/setup-matt-pocock-skills` in this fresh session, then `/grill-with-docs` on the brief
(https://claude.ai/artifact/Nzy7HQb927z7DKiCY4a2Gs), then `/to-spec` and `/to-tickets`, all in
**one unbroken session**: the grilling, spec and tickets build on the same thinking. Recommended
answers for setup: local markdown tracker (GitHub turns issues off on forks, and Jesse is the only
filer), default triage labels, single-context glossary at the repo root. Kick off two `/research`
runs in the background before grilling: (a) what's documented about Arbhar's grain envelopes and
soft character; (b) envelope-sum wet normalisation, and scaling Clouds' diffuser from 32 to 48 kHz.
Record two decisions as ADRs during grilling: the GCC 15 / `-std=gnu++11` build, and never writing
sector 15. Make the first ticket an objective render-measurement tool (wet vs dry level, a 1 ms-frame
click detector, grain attack time), so `/tdd` has test points to work against. Then one fresh session
per ticket with `/implement`, and `/retro` after the first.

## Waiting on Jesse

- **Settings backup**: read sector 15 (0x081E0000, 128 KB) out of the module with
  STM32CubeProgrammer into `backups/` (git-ignored), plus a copy off this machine. Blocks any flash.
- **Tracker choice** at `/setup-matt-pocock-skills`: local markdown (recommended) or GitHub Issues.
- **Optional**: block pushes to upstream with `git remote set-url --push upstream DISABLED`. Offered,
  not done.

## In flight

Nothing is half-built. The shared session skills are finished and committed in
`~/Documents/Sites/session-skills` (local only, not on GitHub). The other four projects haven't moved
over to them yet; that happens when each is next opened (see that repo's README).

## Will bite

- The firmware linker script allows the image to run over the settings sector. Only `./firmware.sh`'s
  end-address check stops an oversized build. Keep using the script, never bare `make`.
- If sector 15 is blank or corrupt, the firmware silently writes defaults over the calibration.
- `beads/test/makefile` needs `-Wno-vla-cxx-extension` for Apple Clang (already committed). GCC 15
  needs `-std=gnu++11` (in `firmware.sh`); without it, 196 `register` errors come from ST's HAL.
- `audio_samples/kettel.wav` is synthetic (from `beads/test/make_test_source.py`), not Émilie's
  original. `renders/` and `audio_samples/` are git-ignored, so a fresh clone must regenerate both,
  and render `renders/stock/` from the upstream DSP before any DSP change.
- `./firmware.sh wav` (audio bootloader update file) has never been tried.
- Shell quirks (`ls` is eza, zsh `=word`, sudo needs the Terminal panel) are in memory.

## Gates

At `38bbcf8`:
- harness: builds with no warnings
- firmware: image ends at 0x0805ABB0, 1557 KB below the settings sector
- stock renders, 30 s each, 0 clipped samples:
  - `beads_processor`: peak −3.2 dBFS, RMS −29.3 dBFS
  - `beads_granular_player`: peak −5.3, RMS −37.2
  - `beads_granular_processor`: peak −10.7, RMS −32.3

## Where to look

- `CLAUDE.md`: goals, workflow, planned-changes table
- `.claude/session.md`: checks, hazards, gates
- the brief: https://claude.ai/artifact/Nzy7HQb927z7DKiCY4a2Gs
- `beads/test/beads_test.cc`: harness tests (`./beads_test list`)
- `beads/dsp/processor.cc`: dry/wet mix, the target of change #1
