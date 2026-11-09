# Next session: after 9 November 2026 (stereo width kept, seam clean)

## Where things stand

On `master` at `2d7a992`, with this handoff as a commit on top. **Committed and pushed:** everything, including this handoff. **Flashed:** nothing, ever. **No settings backup exists.** This session: ticket 12's verdict went in (`7ce91aa`). Ticket 13, stereo width, is done (`cee1b1c`, verdict in `30abc58`): a ±30% pan floor plus opposite L/R level of about ±1 dB per grain, on the delay player's stereo path only, kept by Jesse after the A/B. Ticket 14, the freeze seam, was measured and has no click, so no crossfade and no DSP change (`63038da`). Of the planned DSP changes only the pitch quantiser (ticket 15) is left. Tickets 05, 09, 10, 11, 13 and 16 still need CPU measured, and ticket 10 is the heaviest.

## Start here

**The settings backup, then CPU on the debug pin.** `CLAUDE.md` says to measure CPU before adding anything per-grain, and the quantiser is per-grain. Six DSP changes are stacked on the module with no CPU figure and no flash yet, so this is the biggest unknown left. Only Jesse can do it: read sector 15 (0x081E0000, 128 KB) with STM32CubeProgrammer into `backups/` (git-ignored), and copy it off this machine. Flashing and the debug-pin measurement follow.

While that waits, ticket 15's short grilling (`.scratch/soft-character/issues/15-pitch-quantiser.md`, `needs-triage`) can go ahead, since it needs only Jesse's decisions. Don't implement it before the CPU number.

## Waiting on Jesse

- **Settings backup.** See above. It blocks any flash, and so the CPU measurements.
- **Ticket 15 grilling:** LED display of the mask, and a modulation-only mode, per spec decision 12.
- **Ticket 03, the reference captures**, once ticket 02 (`ready-for-agent`, not started) lands.
- **Why ticket 12 was binned.** No reason is recorded. It goes in that ticket's comments, so nobody retries the same mapping.
- **The ticket breakdown** is still unreviewed (16 tickets).
- **Optional:** `uv` and a Perplexity key for `/research` (memory `research-script-broken`).

## In flight

Nothing is half-built. No branches other than `master`, no extra worktrees, no stashes. No listening server is running.

## Will bite

- **Stale binaries on macOS.** `make` 3.81 compares times to the second, so swapping source with `sed` or `git stash` straight after a build can skip the rebuild. A "before" column on ticket 13 was silently the ±2 dB build. Use `make -B -f beads/test/makefile` when building versions in a loop. Two columns that match exactly mean a stale binary.
- **A measurement patch is not a listening patch.** `beads_width_clocked` and `beads_wetdry_wet` have nothing randomised, so they sound like the source delayed (0.77 match to the dry guitar). Jesse said "they sound dry almost". Listen on `./beads_test TestProcessorGrains` (54 ms grains, Time randomised, random and regular timing) or `beads_cloud`.
- **The pan floor makes wide sources narrower.** The stock pan law feeds the opposite channel across as it pans. L/R correlation at ±1 dB on the short-grain patches: groove 0.987 to 0.939 and guitar 0.933 to 0.910 got wider, kettel 0.276 to 0.401 and synth 0.030 to 0.180 got narrower. Jesse kept it. A level-only floor with no crossfeed is the untried alternative.
- **The width check only means anything on a near-mono source.** `kettel.wav` is at 0.17 correlation, so `TestProcessorWidth` sums the source to mono for that render.
- **Width raises level on near-mono sources:** up to 2.1 dB gated on groove with regular timing, 1.1 dB on guitar. The level gates only use `kettel.wav`, which moved 0.1 dB, so they don't see this. The fully wet fixed patch peaks at −3.3 dBFS, up from −4.6.
- **Stereo renders aren't comparable sample for sample across `cee1b1c`.** Each stereo grain takes one more random draw, so the sequence shifts everywhere. Mono inputs skip both width stages and are byte-identical to before. In `TestProcessorWidth` the mono renders run first so the stereo ones can't shift them. `renders/13-stereo-width/` is the current "before".
- **`TestGrainLevel` uses a mono delay line** so pan doesn't move the level it measures.
- **The click detector misfires on a steady sine.** With 1 ms frames at 223 Hz it flags runs of frames at second-difference levels near −70 dBFS, in frozen and unfrozen renders alike. Look at the frame levels before believing a sine click (`frame_stats` in `beads/test/measure.py`).
- **Delay mode dips when Freeze engages.** Stock's `DelayController` resets every play head on purpose, so the output drops about 30 dB for a few milliseconds. It isn't the seam. It's untouched.
- **The freeze seam was measured on Digital media in the harness only.**
- **`preview_start` with `listen` fails when 8765 is taken,** and 8765 and 8766 are usually held by `sublustris-versio`. Use `listen-alt` and the port it reports. A 200 on `/` doesn't prove it's the listening page; check `/api/renders`, which only `listen.py` serves.
- **A new render named `beads_density_*` joins the Density sweep check.** `density_spread_narrower` picks up that whole prefix, and a stale file in `renders/check/` keeps counting until it's deleted.
- **`TriggerGenerator::Render` has two production callers,** `beads/dsp/processor.cc` and `beads/dsp/synth.cc`. The test harness builds both.
- **CPU is the unmeasured risk.** Past 3 o'clock on Shape the diffuser adds eight allpasses and two interpolated reads per sample on top of up to 32 grains. Below Hann it's skipped entirely. Tickets 11 and 13 went ahead without a CPU figure, which `CLAUDE.md` asks for, because they add only random draws and a little arithmetic per grain.
- **"Downward only" is gone.** Make-up gain means grains run from 0.667 to 1.333 of their level without the feature at full depth, so the loudest is +2.5 dB. Keeping the level out of the envelope sum is deliberate: inside it, the bus gain cancels the variation.
- **The random level also applies on the right half of Density with a clock patched**, where that half is probability. A clocked diff against stock will show this level, so compare against the commit before.
- **`quiet_wet_unchanged` is at −0.03 dB against a ±0.1 limit** (it was +0.08 before ticket 13, and that's random sequence). It compares against a fixed −37.35 dBFS. The constant is still called `STOCK_…`, though it isn't stock. If it fails, confirm the cause, then re-baseline it. Don't loosen the tolerance.
- **Treat click counts as approximate.** The granular player's count moved 307, 336, 325 between near-identical code.
- **Listening sweeps for settled tickets are deleted,** including ticket 13's `sweep/` (9 November) and ticket comments still mention them. Only each ticket folder's top level survives. The session prune step removes these as they settle; see `.claude/session.md`.
- **For a before and after,** either stash the DSP files and `make -B`, or build the previous commit in the scratchpad: `git archive <sha>`, `rmdir` its empty `stmlib` and `stm_audio_bootloader`, symlink those plus `audio_samples` and `renders/sources`, and `make -B -f beads/test/makefile`. Then `BEADS_TEST_SOURCE=renders/sources/<name>.wav ./beads_test TestProcessorGrains`.
- **Clicks fall to almost nothing once the diffuser is on** (340 to 8 on the cloud). That's partly the detector: the diffuser spreads the sudden jumps it looks for. Don't compare click counts across the diffuser.
- **Nothing measures "metallic".** The ring and movement checks in `checks.py` only guard the choice Jesse made by ear.
- **Knob positions in test patches changed meaning at ticket 09.** Patches that compare with stock moved to the position with the same window (0.33 to 0.0025, 0.3 to 0.025, within 1 LSB of before). The cloud moved from 0.66 to 0.75. `TestGranularProcessor` stayed at 0.5, now Tukey, so its figures don't compare with stock.
- **A/B on more than `kettel.wav`.** `renders/sources/` (git-ignored) has `guitar.wav`, `synth.wav` and `groove.wav`, built from Sublustris clips 09, 08 and 03, looped to 30 s. Sustained synth shows ring best. Regenerate with `.venv/bin/python docs/minisite/tools/demos/build_sources.py <dir> 09 08 03` in `sublustris-versio`, then `ffmpeg -stream_loop -1 -t 30 ... -ar 48000 -ac 2 -c:a pcm_s16le -bitexact -map_metadata -1`.
- **Listening options that share a folder need their own titles.** Use `{label, about}` as the render's value in `listen_notes.json`. File names are global keys, so reuse only with care.
- **`TestProcessorShapeSweep` has six positions now** (0.875 was added before 1.0), so its random sequence for 1.0 differs from ticket 09's five-position run.
- **Clouds' linear wet/dry blend dips about 2.3 dB halfway on the decorrelated signal.** Equal-power fixed it. Any new blend of a signal with an allpassed copy will do the same.
- **`beads/resources.cc` and `.h` are hand-spliced for `lut_gaussian`,** because the resource compiler needs Python 2. `lookup_tables.py` has the matching source, so a regeneration would agree.
- **Size above 0.99 is delay mode** (`beads/dsp/processor.cc`, in `Process`). Any "long grains" patch has to stop at 0.98.
- **In delay mode Shape does something else,** so anything keyed to Shape has to be off there, as the diffuser is.
- **Don't randomise Pitch in listening demos.** Jesse found it seasick and hard to judge.
- **Nothing fills the grain pool except `TestProcessorVoiceSteal`.** Pool size is 32. Size 0.75 overlaps about 9 grains, so stealing never runs.
- **The granular processor's clicks are still unexplained.** It isn't the voice-stealing fade or the bus gain's instant drop. It may not be grain edges at all, which is untested.
- **The bus gain's blend ignores Density's own randomness.** `ComputeSpread` reads full spread across the whole random half, so it can't tell the fixed patch from the cloud. Time under CV counts as no randomness and keeps the linear gain, unrendered.
- **Time randomness decorrelates grains at about 1% of the attenuverter.** Anything scaled to Time spread has to be fully in by then.
- **`beads_cloud.wav` is about 1.7 dB under stock.** That's what matching the dry costs, and Jesse accepted it.
- **The attack probe** is a test-only hook in `grain_envelope.h` (`#ifdef TEST`). Keep the two probe calls if the file is rewritten.
- **Stock renders of new patches:** `git archive f4cfa76` into the scratchpad, set up as above. Use `beads/test/` from `46b2679`, because today's harness needs `Diffuser` and `diffuser_amount()`, plus any new test by hand. Use `git show 6f6880d:beads/dsp/grain_envelope.h` with `min_attack_` back at 0.0005. Confirm the build reproduces `renders/stock/beads_wetdry_wet.wav` with `cmp`.
- **A sweep value can be a no-op on a render.** `cmp` each render against the control before reading anything into a flat score.
- **At Shape 0.25 (square) the attack floor is 12 ms**, because it's multiplied by `1 + 3 × sustain`. Short grains there are unrendered.
- **The wet render's click count is near the detector's noise.** The dry has 85 of its 93–99, and the detector flags `kettel.wav`'s own note onsets. Compare against dry and stock, not as absolutes.
- **A "no change" check can't fail before the change.** Pair it with one that does. A check's threshold must clear both the old and new values by a margin.
- **Ticket numbers aren't `CLAUDE.md` row numbers.** Row 10 is ticket 13, row 11 is ticket 14, row 12 is ticket 15. Ticket 16 is under row 2.
- **The harness's random sequence depends on test run order.** Render ticket folders one test per process: `for t in TestGranularPlayer TestGranularProcessor TestProcessor TestProcessorWetDry TestProcessorWetDrive TestProcessorVoiceSteal TestProcessorCloud TestProcessorShapeSweep TestProcessorDensitySweep; do ./beads_test $t; done`. `measure.py --check` does this already.
- **The Density sweep's full spread is dominated by the audio-rate ends** (Density 0.0 and 1.0). For texture, look at the 0.6–0.9 rows too.
- **The wavetable player still uses the stock per-grain gain and pan.** Only the delay player normalises the bus, takes the random level or gets the width stages.
- **The harness writes WAVs into the repo root.** For a ticket folder, render as above, move the files (including `beads_wetdry_wet_attacks.csv`) into `renders/<NN-name>/`, then `python3 beads/test/measure.py renders/<NN-name>/*.wav > renders/<NN-name>/metrics.txt`.
- **The listening page binds 127.0.0.1 only.** WAVs are under `/files/`. Changing the hash reloads the comparison, so it replaces whatever Jesse has loaded. Changes to `listen.py` need a restart; `listen_notes.json` and `listen.html` only need a page reload.
- **The Arm `nm` and `size` tools are in `/Applications/ArmGNUToolchain/15.3.rel2/arm-none-eabi/bin/`,** not `…/bin/`. Always build with `./firmware.sh`, never bare `make` (ADR 0002).
- **`/implement` and `/retro` aren't in the skill list.** Use `/mattpocock-skills:tdd`, or work from the ticket directly.
- **Reference source:** ticket 02 still has to make clip 09 the reference. Its dry copy is `renders/sources/guitar.wav`, but the ticket wants it in `audio_samples/`. Don't use the lossy `*-dry.mp3`.

## Gates

Run on `63038da` after a forced clean build. Only `beads/test/listen_notes.json` and this handoff have changed since.

- Harness: builds with 0 errors or warnings.
- Firmware: image ends at 0x0805BBC0, 1553 KB below the settings sector (stock 0x0805ABB0).
- `measure.py --check`: 21 of 21 pass. Wet − dry gated RMS +0.3 dB, cloud − dry +0.3 dB, Density sweep spread 4.9 dB, quiet render −37.38 dBFS (−0.03 dB against baseline), shortest grain rise 3.12 ms, granular player 325 clicks, hot cloud peak −1.6 dBFS. Width: side − mid −24.3 dB, L/R correlation 0.9930 on the clocked mono-summed patch. Mono input L − R is exactly 0.
- `levels.py renders/check/`: no clipping. `beads_cloud` peak −5.4 / RMS −27.0 dBFS, `beads_shape_100` −9.2 / −27.5, `beads_wetdry_wet` −3.3 / −27.0, `beads_drive_hot` −0.7 / −21.0.
- A/B listening: Jesse kept ticket 13 at ±1 dB. Ticket 14 had nothing to listen to.

## Where to look

- `CLAUDE.md`: the CPU rule and the flashing steps
- `beads/drivers/debug_pin.h`: the CPU measurement
- `.scratch/soft-character/issues/15-pitch-quantiser.md` and `.scratch/soft-character/spec.md` decision 12
- `.scratch/soft-character/issues/02-reference-source-and-patches.md`: the other unstarted ticket
- `.scratch/soft-character/issues/13-stereo-width.md`: Comments, the most recent verdict
- `.scratch/soft-character/issues/14-freeze-seam.md`: Comments, the seam measurement
