# Next session: after 8 November 2026 (ticket 11 kept, 12 next)

## Where things stand

On `master` at `0eee5e3`, with this handoff as a commit on top. **Committed:** ticket 11 (`63ae170`, verdict in `5e102e0`). **Pushed:** everything, through this handoff, so origin has all the work after `6f6880d`. **Flashed:** nothing. **No settings backup exists.** Ticket 11: each grain gets a random level at birth, spread over 6 dB across Density's random half, with make-up gain holding the average. Jesse kept it after the A/B on four sources. Tickets 05, 09, 10, 11 and 16 still need CPU measured, and ticket 10 is the heaviest.

## Start here

**Ticket 12**, `.scratch/soft-character/issues/12-density-as-overlap.md` (`CLAUDE.md` row 9): with no clock patched, Density sets average overlap (grains per grain length) rather than grains per second, so changing Size keeps the cloud equally dense. Clocked behaviour stays stock. It's next in the planned order and desktop-only, so the missing backup doesn't block it.

- Add the check to `beads/test/checks.py` first and see it fail: measured overlap roughly constant as Size changes at a fixed Density. "Clocked stays stock" can't fail first, so pair it.
- Unsure, decide before writing it: `TriggerGenerator::Render` doesn't receive Size or the grain duration today, so the overlap mapping needs one of them passed in. Randomised Size also makes "grain length" a moving target.
- Unsure: the wet level gates all render at Density 0.9 and Size 0.75. Remapping Density will move how many grains overlap there, so those patches may need new positions to keep meaning what they did. Work that out before reading a failure as a regression.
- `level_spread` (ticket 11) is computed from the Density knob in the same function. Keep it keyed to the knob, not to the new rate.

## Waiting on Jesse

- **Settings backup.** Read sector 15 (0x081E0000, 128 KB) with STM32CubeProgrammer into `backups/` (git-ignored), and copy it off this machine. It blocks any flash, and so the CPU measurements in tickets 05, 09, 10, 11 and 16.
- **Ticket 03, the reference captures**, once ticket 02 (`ready-for-agent`, not started) lands.
- **A/B verdicts** on each DSP ticket from 12 on.
- **The ticket breakdown** is still unreviewed (16 tickets).
- **Optional:** `uv` and a Perplexity key for `/research` (memory `research-script-broken`).

## In flight

Nothing is half-built. The tree is clean apart from this handoff. No branches other than `master`, and no extra worktrees. The `listen` page is serving on 127.0.0.1:8765; stopping it is harmless.

## Will bite

- **CPU is the unmeasured risk.** Past 3 o'clock on Shape the diffuser adds eight allpasses and two interpolated reads per sample on top of up to 32 grains. Below Hann it's skipped entirely. Ticket 11 went ahead without a CPU figure, which `CLAUDE.md` asks for, because it adds only a random draw and a little arithmetic per grain.
- **"Downward only" is gone.** Make-up gain means grains run from 0.667 to 1.333 of their level without the feature at full depth, so the loudest is +2.5 dB. Cloud RMS matches the commit before within 0.4 dB, but peaks rose 0.4 to 2.3 dB. Keeping the level out of the envelope sum is deliberate: inside it, the bus gain cancels the variation.
- **The random level also applies on the right half of Density with a clock patched**, where that half is probability. Spec row 9's "clocked stays stock" is about overlap; ticket 12's clocked diff against stock will now show this level, so compare against the commit before, or account for it.
- **`quiet_wet_unchanged` sits at +0.08 dB against a ±0.1 limit.** It compares against a fixed −37.35 dBFS (the constant is still called `STOCK_…`, though it isn't stock). If it fails, confirm the cause, then re-baseline it. Don't loosen the tolerance.
- **Only Density 0 to 0.4 still renders byte-identically to before ticket 11.** The random draw shifts the sequence on the random half, so grain-for-grain comparison across that commit means nothing. The granular player's click count moved 307 to 336 between runs of near-identical code for this reason. Treat click counts as approximate.
- **Listening sweeps for settled tickets are deleted** (8 November, 388 WAVs). Ticket comments still mention them. Only each ticket folder's top level survives, and `renders/11-random-grain-level/` is the current "before". The session prune step now removes these as they settle; see `.claude/session.md`.
- **For a before and after on the extra sources,** build the previous commit in the scratchpad: `git archive <sha>`, `rmdir` its empty `stmlib` and `stm_audio_bootloader`, symlink those plus `audio_samples` and `renders/sources`, and `make -f beads/test/makefile`. Then `BEADS_TEST_SOURCE=renders/sources/<name>.wav ./beads_test TestProcessorCloud`.
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
- **The fixed wet patch isn't granular.** `beads_wetdry_wet.wav` correlates at 0.87 with the delayed dry. Use `beads_cloud.wav` or the Shape sweep for grain character.
- **Don't randomise Pitch in listening demos.** Jesse found it seasick and hard to judge.
- **Nothing fills the grain pool except `TestProcessorVoiceSteal`.** Pool size is 32. Size 0.75 overlaps about 9 grains, so stealing never runs.
- **The granular processor's clicks are still unexplained.** It isn't the voice-stealing fade or the bus gain's instant drop. It may not be grain edges at all, which is untested.
- **The bus gain's blend ignores Density's own randomness.** `ComputeSpread` reads full spread across the whole random half, so it can't tell the fixed patch from the cloud. Time under CV counts as no randomness and keeps the linear gain, unrendered.
- **Time randomness decorrelates grains at about 1% of the attenuverter.** Anything scaled to Time spread has to be fully in by then.
- **`beads_cloud.wav` is 1.7 dB under stock.** That's what matching the dry costs, and Jesse accepted it.
- **The attack probe** is a test-only hook in `grain_envelope.h` (`#ifdef TEST`). Keep the two probe calls if the file is rewritten.
- **Stock renders of new patches:** `git archive f4cfa76` into the scratchpad, set up as above. Use `beads/test/` from `46b2679`, because today's harness needs `Diffuser` and `diffuser_amount()`, plus any new test by hand. Use `git show 6f6880d:beads/dsp/grain_envelope.h` with `min_attack_` back at 0.0005. Confirm the build reproduces `renders/stock/beads_wetdry_wet.wav` with `cmp`.
- **A sweep value can be a no-op on a render.** `cmp` each render against the control before reading anything into a flat score.
- **At Shape 0.25 (square) the attack floor is 12 ms**, because it's multiplied by `1 + 3 × sustain`. Short grains there are unrendered.
- **The wet render's click count is near the detector's noise.** The dry has 85 of its 93–99, and the detector flags `kettel.wav`'s own note onsets. Compare against dry and stock, not as absolutes.
- **A "no change" check can't fail before the change.** Pair it with one that does. A check's threshold must clear both the old and new values by a margin.
- **Ticket numbers aren't `CLAUDE.md` row numbers.** Row 9 is ticket 12. Ticket 16 is under row 2.
- **The harness's random sequence depends on test run order.** Render ticket folders one test per process: `for t in TestGranularPlayer TestGranularProcessor TestProcessor TestProcessorWetDry TestProcessorWetDrive TestProcessorVoiceSteal TestProcessorCloud TestProcessorShapeSweep TestProcessorDensitySweep; do ./beads_test $t; done`. `measure.py --check` does this already.
- **The Density sweep's full spread is dominated by the audio-rate ends** (Density 0.0 and 1.0). For texture, look at the 0.6–0.9 rows too.
- **The wavetable player still uses the stock per-grain gain.** Only the delay player normalises the bus or takes the random level.
- **The harness writes WAVs into the repo root.** For a ticket folder, render as above, move the files (including `beads_wetdry_wet_attacks.csv`) into `renders/<NN-name>/`, then `python3 beads/test/measure.py renders/<NN-name>/*.wav > renders/<NN-name>/metrics.txt`.
- **The listening page binds 127.0.0.1 only.** WAVs are under `/files/`. Changing the hash reloads the comparison, so it replaces whatever Jesse has loaded. Changes to `listen.py` need a restart; `listen_notes.json` and `listen.html` only need a page reload.
- **The Arm `nm` and `size` tools are in `/Applications/ArmGNUToolchain/15.3.rel2/arm-none-eabi/bin/`,** not `…/bin/`. Always build with `./firmware.sh`, never bare `make` (ADR 0002).
- **`/implement` and `/retro` aren't in the skill list.** Use `/mattpocock-skills:tdd`, or work from the ticket directly.
- **Reference source:** ticket 02 still has to make clip 09 the reference. Its dry copy is `renders/sources/guitar.wav`, but the ticket wants it in `audio_samples/`. Don't use the lossy `*-dry.mp3`.

## Gates

Run on `0eee5e3`. Only this handoff has changed since.

- Harness: builds with 0 errors or warnings.
- Firmware: image ends at 0x0805B988, 1553 KB below the settings sector (stock 0x0805ABB0).
- `measure.py --check`: 19 of 19 pass. Wet − dry gated RMS +0.2 dB, cloud − dry +0.2 dB, Density sweep spread 4.8 dB, quiet render −37.27 dBFS, shortest grain rise 3.12 ms, granular player 336 clicks. Shape morph step 0.050. Diffuser ring 97.1 ms, impulse moved 0.32, halfway +0.4 dB. Random level: quietest grain 0.500 of the loudest at full depth, mean level 1.000, 0.995 and 1.002 at no, half and full depth.
- `levels.py renders/check/`: no clipping. `beads_cloud` peak −4.8 / RMS −27.1 dBFS, `beads_shape_100` −9.3 / −27.5, `beads_wetdry_wet` −4.6 / −27.0, `beads_drive_hot` −1.1 / −21.0.
- A/B listening: Jesse kept ticket 11 on four sources.

## Where to look

- `.scratch/soft-character/issues/12-density-as-overlap.md`: the next ticket
- `.scratch/soft-character/spec.md`, decision 9
- `beads/dsp/trigger_generator.cc`: `Render`, where Density becomes a rate
- `beads/dsp/granular_processor.h`: named by `CLAUDE.md` row 9
- `beads/test/checks.py`: add ticket 12's check here first
- `.scratch/soft-character/issues/11-random-grain-level.md`: Comments, the most recent verdict format
