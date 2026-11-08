# Next session: after 8 November 2026 (tickets 09 and 10 kept, 11 next)

## Where things stand

On `master` at `be6c493`, with this handoff as a checkpoint commit on top. **Committed:** tickets 09 and 10, merged to `master`. **Pushed:** nothing since `6f6880d`; `master` is 18 commits ahead of origin before this checkpoint. **Flashed:** nothing. **No settings backup exists.** Ticket 09 (`9836485`): Shape is one continuous morph, decay, square, Tukey, Hann, Gaussian, at the knob's quarters; Jesse kept it. Ticket 10 (`c6b7803`): diffuser on the wet bus, equal-power fade from Hann to fully clockwise. Jesse heard Clouds' fixed lines at `kap` 0.625 as metallic and picked `kap` 0.5 with each channel's first line modulated, by ear across four sources. Tickets 05, 09, 10 and 16 still need CPU measured, and ticket 10 is the heaviest.

## Start here

**Ticket 11**, `.scratch/soft-character/issues/11-random-grain-level.md` (`CLAUDE.md` row 8): a random per-grain level set at birth, downward only to −6 dB, with depth following Density's random half and none on the clocked half or at noon. It's next in the planned order and desktop-only, so the missing backup doesn't block it.

- Add the check to `beads/test/checks.py` first and see it fail. The downward-only and clocked-half criteria are the two to pin. A no-change check on the clocked half can't fail first, so pair it with one that can.
- Unsure, decide before writing it: `UpdateBusGain` normalises by the envelope sum, so work out whether the random level goes inside that sum. If it doesn't, the bus gain won't know those grains are quieter.
- Judge it on the extra sources too (see "Will bite"), with a preloaded link and a distinct title and listen-for line per option.

## Waiting on Jesse

- **Settings backup.** Read sector 15 (0x081E0000, 128 KB) with STM32CubeProgrammer into `backups/` (git-ignored), and copy it off this machine. It blocks any flash, and so the CPU measurements in tickets 05, 09, 10 and 16.
- **Ticket 03, the reference captures**, once ticket 02 (`ready-for-agent`, not started) lands.
- **A/B verdicts** on each DSP ticket from 11 on.
- **The ticket breakdown** is still unreviewed (16 tickets).
- **Whether to push.** Everything after `6f6880d` is local only.
- **Optional:** `uv` and a Perplexity key for `/research` (memory `research-script-broken`).

## In flight

Nothing is half-built. The tree is clean apart from this handoff. Branches `exp/diffuser`, `exp/shape-sweep` and `exp/wet-level` are fully merged and can be deleted. The `listen` page is serving on 127.0.0.1:8765; stopping it is harmless.

## Will bite

- **CPU is the unmeasured risk.** Past 3 o'clock on Shape the diffuser adds eight allpasses and two interpolated reads per sample on top of up to 32 grains. Below Hann it's skipped entirely.
- **Clicks fall to almost nothing once the diffuser is on** (340 to 8 on the cloud). That's partly the detector: the diffuser spreads the sudden jumps it looks for. Don't compare click counts across the diffuser.
- **Nothing measures "metallic".** The ring and movement checks in `checks.py` only guard the choice Jesse made by ear.
- **Knob positions in test patches changed meaning at ticket 09.** Patches that compare with stock moved to the position with the same window (0.33 to 0.0025, 0.3 to 0.025, within 1 LSB of before). The cloud moved from 0.66 to 0.75. `TestGranularProcessor` stayed at 0.5, now Tukey, so its figures don't compare with stock.
- **A/B on more than `kettel.wav`.** `renders/sources/` (git-ignored) has `guitar.wav`, `synth.wav` and `groove.wav`, built from Sublustris clips 09, 08 and 03, looped to 30 s. Select one with `BEADS_TEST_SOURCE=renders/sources/synth.wav ./beads_test <Test>`. Sustained synth shows ring best. Regenerate with `.venv/bin/python docs/minisite/tools/demos/build_sources.py <dir> 09 08 03` in `sublustris-versio`, then `ffmpeg -stream_loop -1 -t 30 ... -ar 48000 -ac 2 -c:a pcm_s16le -bitexact -map_metadata -1`.
- **Listening options that share a folder need their own titles.** Use `{label, about}` as the render's value in `listen_notes.json`. File names are global keys, so reuse only with care.
- **`TestProcessorShapeSweep` has six positions now** (0.875 was added before 1.0), so its random sequence for 1.0 differs from ticket 09's five-position run. `renders/09-shape-sweep/beads_shape_100.wav` isn't a same-seed reference for later renders.
- **Clouds' linear wet/dry blend dips about 2.3 dB halfway on the decorrelated signal.** Equal-power fixed it. Any new blend of a signal with an allpassed copy will do the same.
- **`beads/resources.cc` and `.h` are hand-spliced for `lut_gaussian`,** because the resource compiler needs Python 2. `lookup_tables.py` has the matching source, so a regeneration would agree.
- **Size above 0.99 is delay mode** (`beads/dsp/processor.cc`, in `Process`). Any "long grains" patch has to stop at 0.98.
- **In delay mode Shape does something else,** so anything keyed to Shape has to be off there, as the diffuser is.
- **The fixed wet patch isn't granular.** `beads_wetdry_wet.wav` correlates at 0.87 with the delayed dry. Use `beads_cloud.wav` or the Shape sweep for grain character.
- **Don't randomise Pitch in listening demos.** Jesse found it seasick and hard to judge.
- **Nothing fills the grain pool except `TestProcessorVoiceSteal`.** Pool size is 32. Size 0.75 overlaps about 9 grains, so stealing never runs.
- **The granular processor's clicks are still unexplained.** It isn't the voice-stealing fade or the bus gain's instant drop. It may not be grain edges at all, which is untested.
- **The blend ignores Density's own randomness.** `ComputeSpread` reads full spread across the whole random half, so it can't tell the fixed patch from the cloud. Time under CV counts as no randomness and keeps the linear gain, unrendered.
- **Time randomness decorrelates grains at about 1% of the attenuverter.** Anything scaled to Time spread has to be fully in by then.
- **`beads_cloud.wav` is 1.7 dB under stock.** That's what matching the dry costs, and Jesse accepted it.
- **`quiet_wet_unchanged` compares against a fixed −37.35 dBFS** (the constant is still called `STOCK_…`, though it isn't stock). If it fails, confirm the cause, then re-baseline it. Don't loosen the tolerance.
- **The attack probe** is a test-only hook in `grain_envelope.h` (`#ifdef TEST`). Keep the two probe calls if the file is rewritten.
- **Stock renders of new patches:** `git archive f4cfa76` into the scratchpad. `rmdir` its empty `stmlib` and `stm_audio_bootloader` before symlinking them (otherwise the links land inside them), and symlink `audio_samples`. Use `beads/test/` from `46b2679`, because today's harness needs `Diffuser` and `diffuser_amount()`, plus any new test by hand. Use `git show 6f6880d:beads/dsp/grain_envelope.h` with `min_attack_` back at 0.0005. Confirm the build reproduces `renders/stock/beads_wetdry_wet.wav` with `cmp`.
- **A sweep value can be a no-op on a render.** `cmp` each render against the control before reading anything into a flat score.
- **At Shape 0.25 (square) the attack floor is 12 ms**, because it's multiplied by `1 + 3 × sustain`. Short grains there are unrendered.
- **The wet render's click count is near the detector's noise.** The dry has 85 of its 93–99, and the detector flags `kettel.wav`'s own note onsets. Compare against dry and stock, not as absolutes.
- **A "no change" check can't fail before the change.** Pair it with one that does. A check's threshold must clear both the old and new values by a margin.
- **Ticket numbers aren't `CLAUDE.md` row numbers.** Row 8 is ticket 11. Ticket 16 is under row 2.
- **The harness's random sequence depends on test run order.** Render ticket folders one test per process: `for t in TestGranularPlayer TestGranularProcessor TestProcessor TestProcessorWetDry TestProcessorWetDrive TestProcessorVoiceSteal TestProcessorCloud TestProcessorShapeSweep; do ./beads_test $t; done`. `measure.py --check` does this already.
- **The Density sweep's full spread is dominated by the audio-rate ends** (Density 0.0 and 1.0). For texture, look at the 0.6–0.9 rows too.
- **The wavetable player still uses the stock per-grain gain.** Only the delay player normalises the bus.
- **The harness writes WAVs into the repo root.** For a ticket folder, render as above, move the files (including `beads_wetdry_wet_attacks.csv`) into `renders/<NN-name>/`, then `python3 beads/test/measure.py renders/<NN-name>/*.wav > renders/<NN-name>/metrics.txt`.
- **The listening page binds 127.0.0.1 only.** WAVs are under `/files/`. Changing the hash reloads the comparison, so it replaces whatever Jesse has loaded. Changes to `listen.py` need a restart; `listen_notes.json` and `listen.html` only need a page reload.
- **The Arm `nm` and `size` tools are in `/Applications/ArmGNUToolchain/15.3.rel2/arm-none-eabi/bin/`,** not `…/bin/`. Always build with `./firmware.sh`, never bare `make` (ADR 0002).
- **`/implement` and `/retro` aren't in the skill list.** Use `/mattpocock-skills:tdd`, or work from the ticket directly.
- **Reference source:** ticket 02 still has to make clip 09 the reference. Its dry copy is `renders/sources/guitar.wav`, but the ticket wants it in `audio_samples/`. Don't use the lossy `*-dry.mp3`.

## Gates

Run on `c6b7803`. `be6c493` changed only docs and the listening notes, and the gates haven't been re-run since.

- Harness: builds with 0 errors or warnings.
- Firmware: image ends at 0x0805B6E8, 1554 KB below the settings sector (stock 0x0805ABB0). `.bss` is 49,136 bytes, and DTCM holds about 50 KB of 128 KB.
- `measure.py --check`: 16 of 16 pass. Wet − dry gated RMS +0.1 dB, Density sweep spread 4.8 dB, quiet render −37.35 dBFS, shortest grain rise 3.12 ms, granular player 359 clicks. Shape morph step 0.050 (limit 0.065). Diffuser ring 97.1 ms (limit 113.0), impulse moved 0.32, 32 kHz timing 0.666, halfway −0.0 dB.
- `levels.py renders/10-diffuser/*.wav`: no clipping. `beads_cloud` peak −7.1 / RMS −27.1 dBFS at Hann; `beads_shape_100` peak −8.1 / −27.5.
- Below Hann, every render in `renders/10-diffuser/` is byte-identical to `renders/09-shape-sweep/`.

## Where to look

- `.scratch/soft-character/issues/11-random-grain-level.md`: the next ticket
- `.scratch/soft-character/spec.md`, decision 8
- `beads/dsp/granular_player.h`: `UpdateBusGain`, `ComputeSpread`, where grains start
- `beads/test/checks.py`: add ticket 11's check here first
- `.scratch/soft-character/issues/10-wet-diffuser.md`: Comments, the most recent sweep and verdict format
