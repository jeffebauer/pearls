# Next session: after 8 November 2026 (ticket 01 done, wet gain decided)

## Where things stand

On `master` at `f785350`, pushed to origin (level, nothing ahead). This handoff is a checkpoint commit on top. **Ticket 01 is done** (`08d345f`): `beads/test/measure.py`, `beads/test/checks.py`, and the `TestProcessorWetDry` harness test. `f785350` records Jesse's decision to set the wet gain from measurement (spec decision 2, ADR 0004, `CLAUDE.md` row 1, tickets 04 and 05). **No DSP changes yet. Nothing flashed. No settings backup exists.**

The upstream push block is done: `git remote -v` shows `upstream DISABLED (push)`.

## Start here

**Ticket 04**, `.scratch/soft-character/issues/04-wet-gain-matches-dry.md`, done test-first:

1. Run `python3 beads/test/measure.py --check`. It fails at +4.1 dB (wet − dry, gated RMS, dense patch).
2. Tune `post_gain_wet` (1.5) in `beads/dsp/processor.cc:212` until the check passes. Expect about 0.94, but confirm by rendering, because the limiters sit after the mix.
3. Record the chosen gain and the measured wet − dry in the ticket's Comments.
4. Render into `renders/04-wet-gain/`, compare against `renders/stock/metrics.txt`, and ask Jesse for the A/B verdict.

Read the ticket and spec decision 2 first.

## Waiting on Jesse

- **Settings backup.** Read sector 15 (0x081E0000, 128 KB) with STM32CubeProgrammer into `backups/` (git-ignored), and copy it off this machine. This blocks any flash, and so the CPU measurements in tickets 05, 09 and 10.
- **Ticket 03, the reference captures**, once ticket 02 lands.
- **A/B verdicts** on each DSP ticket (04 onwards).
- **The 15-ticket breakdown** is still unreviewed (07 and 08 could merge, for example).
- **Optional:** `uv` and a Perplexity key for `/research` (memory `research-script-broken`).

## In flight

Nothing. Tree clean, no branches, worktrees or delegates.

## Will bite

- **`/implement` and `/retro` aren't in this session's skill list.** Ticket 01 was done by hand, and `/retro` never ran. Use `/mattpocock-skills:tdd`, or work from the ticket directly.
- **The wet level depends on the patch.** The check uses Density 0.9 (about 25 grains a second) and gated RMS, as Jesse chose. A sparse patch measures mostly the gaps between grains: wet − dry went from −16 to +4 dB across the patches tried. See the table in ticket 01's Comments.
- **The click detector flags `kettel.wav`'s own note onsets** (85 in the dry render). Compare wet click counts with dry and stock, not as absolutes.
- **Stock fully wet clips 8 samples** on the check patch (peak 0.0 dBFS). Ticket 04's acceptance now asks for no clipping.
- **The harness writes renders into the repo root.** `measure.py --check` moves them into `renders/check/`. `/beads_*.wav` is git-ignored, and `*.csv` was already ignored.
- **The attack probe** is a test-only hook in `grain_envelope.h` (`#ifdef TEST`). Ticket 09 rewrites that file, so keep the two probe calls in `Start()` and `Render()`.
- **The reference source is clip 09 of the Sublustris demos** (picked D chords, CC0 Emily guitar). It's built by `~/Documents/Sites/sublustris-versio/docs/minisite/tools/demos/build_sources.py` from `~/Documents/Sites/resilio-versio/test_audio/demo_sources/cc0/guitar_emily/`. The script writes float WAV, and `AudioSource` in the harness reads raw 16-bit after a 48-byte header, so convert. Don't use the lossy `*-dry.mp3`.
- **`kInstantCrossfadeTime` is tied to the scheduler thresholds** (`granular_player.h:41`). Ticket 08 must check that ordering still holds.
- **Density** is periodic left of noon and random right of noon (`trigger_generator.cc:110, 132-137`).
- **Always build with `./firmware.sh`, never bare `make`.** The linker script lets the image run over the settings sector (ADR 0002).
- `renders/` and `audio_samples/` are git-ignored, so a fresh clone must regenerate both, including `renders/stock/metrics.txt` (`python3 beads/test/measure.py renders/stock/*.wav`, after rendering stock with `TestProcessorWetDry`).

## Gates

At `08d345f`; docs only since.

- Harness: builds with 0 errors or warnings.
- Firmware: image ends at 0x0805ABB0, 1557 KB below the settings sector (unchanged, so the probe is compiled out).
- `measure.py --check`: 1 of 1 checks fails (expected): wet − dry gated RMS +4.1 dB.
- Stock baseline in `renders/stock/metrics.txt`. Highlights:
  - `beads_processor`: peak −3.2 dBFS, RMS −29.3 dBFS, 0 clipped
  - wet/dry check patch: wet clips 8 samples; attacks are 0.52 ms on all 748 grains

## Where to look

- `.scratch/soft-character/issues/04-wet-gain-matches-dry.md`: the next ticket
- `.scratch/soft-character/spec.md`: decision 2
- `.scratch/soft-character/issues/01-render-measurement-tool.md`: Comments, for what the tool does and the density table
- `beads/test/checks.py`: where ticket 04's assertion already lives
- `.claude/session.md`: gates, including the check command
