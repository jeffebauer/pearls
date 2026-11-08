# Next session: after 8 November 2026 (ticket 16 done, 09 next)

## Where things stand

On `master` at `6f6880d`, with this handoff as a checkpoint commit on top. `6f6880d` and everything before it is pushed to origin; this checkpoint isn't, unless the log says otherwise. **Ticket 08 closed with no DSP change** (`560b6fe`, `e500119`): a 5 to 20 ms sweep left clicks flat and Jesse couldn't hear a difference, so `kInstantCrossfadeTime` stays at 5 ms. **Ticket 16 is done** (`9cfff04`, `3d03f0d`, `6f6880d`): the bus gain blends towards 1/√Σ when Time or Pitch is randomised, Jesse kept it by ear, and ADR 0004 is amended. Tickets 05 and 16 still need CPU measured. **Nothing flashed. No settings backup exists.**

## Start here

**Ticket 09**, `.scratch/soft-character/issues/09-shape-sweep.md` (`CLAUDE.md` row 6): remap Shape in `beads/dsp/grain_envelope.h` to one continuous morph (decay, square, Tukey, Hann, Gaussian), with reverse dropped (ADR 0003). It's desktop-only until the CPU measurement in its acceptance, so the missing backup doesn't block the work. It's the next ticket in the planned order, and the one most likely to change the sound.

- Judge it on `beads_cloud.wav` as well as the existing patches. It's the render that sounds granular and now has the right level.
- Render five Shape positions against stock, as the ticket asks. `TestProcessorCloud` is the pattern: copy `FixedWetPatch()` and change what the patch needs.
- Add the ticket's check to `beads/test/checks.py` first and see it fail.
- Name every new folder and render in `beads/test/listen_notes.json`, and give Jesse a preloaded link.

## Waiting on Jesse

- **Settings backup.** Read sector 15 (0x081E0000, 128 KB) with STM32CubeProgrammer into `backups/` (git-ignored), and copy it off this machine. It blocks any flash, and so the CPU measurements in tickets 05, 09, 10 and 16.
- **Ticket 03, the reference captures**, once ticket 02 (`ready-for-agent`, not started) lands.
- **A/B verdicts** on each DSP ticket from 09 on.
- **The ticket breakdown** is still unreviewed (now 16 tickets).
- **Optional:** `uv` and a Perplexity key for `/research` (memory `research-script-broken`).

## In flight

Nothing is half-built. The tree is clean apart from this handoff. Branch `exp/wet-level` is fully merged and can be deleted. The `listen` page was serving on 127.0.0.1:8765 at this checkpoint; stopping it is harmless.

## Will bite

- **Size above 0.99 is delay mode** (`beads/dsp/processor.cc:130`). Any "long grains" patch has to stop at 0.98. A voice-stealing render at Size 1.0 measured the wrong thing and had to be redone in ticket 08.
- **The fixed wet patch isn't granular.** `beads_wetdry_wet.wav` correlates at 0.87 with the dry delayed by 404 ms, because nothing is randomised. It has no dry in it, but it barely shows grain character. Use `beads_cloud.wav` for that.
- **Don't randomise Pitch in listening demos.** Jesse found it seasick and hard to judge.
- **Nothing fills the grain pool except `TestProcessorVoiceSteal`.** Pool size is 32. Size 0.75 overlaps about 9 grains, so stealing never runs.
- **The granular processor's clicks are still unexplained** (319 now, stock 276). It isn't the voice-stealing fade, and it isn't the bus gain's instant drop (softening it to about 1 ms gave 322). It may not be grain edges at all, which is untested.
- **The blend ignores Density's own randomness.** `ComputeSpread` reads full spread across the whole random half, so it can't tell the fixed patch from the cloud. Time under CV counts as no randomness and keeps the linear gain, unrendered.
- **Time randomness decorrelates grains at about 1% of the attenuverter.** Anything scaled to Time spread has to be fully in by then.
- **`beads_cloud.wav` is 1.7 dB under stock.** That's what matching the dry costs, and Jesse accepted it.
- **Ticket 06's quiet guard will probably trip on ticket 09.** `quiet_wet_unchanged` in `beads/test/checks.py` compares against a fixed −37.35 dBFS (the constant is still called `STOCK_…`, though it isn't stock any more). If it fails, confirm the cause, then re-baseline the constant. Don't loosen the tolerance.
- **The attack probe** is a test-only hook in `grain_envelope.h` (`#ifdef TEST`). Keep the two probe calls when ticket 09 rewrites the file.
- **Stock renders of new patches:** `git archive f4cfa76` into the scratchpad, symlink `stmlib`, `stm_audio_bootloader` and `audio_samples`, copy today's `beads/test/` over it, and use the probe version of `grain_envelope.h` with `min_attack_` back at 0.0005. Confirm the build reproduces `renders/stock/beads_wetdry_wet.wav` with `cmp`. Once ticket 09 lands, the probe version has to be stock's file with the probe hooks added by hand.
- **A sweep value can be a no-op on a render.** `cmp` each render against the control before reading anything into a flat score, as tickets 07 and 08 both found.
- **Shape 0 with short grains is unrendered.** The attack floor is multiplied by `1 + 3 × sustain`, so at Shape fully anticlockwise the 3 ms floor gives a 12 ms attack.
- **The wet render's click count is near the detector's noise.** The dry has 85 of its 93–99. Use the granular player render to judge grain-edge changes.
- **A "no change" check can't fail before the change.** Pair it with one that does.
- **Sweep folders are partial.** `renders/06-wet-saturator/sweep/off/` has only `beads_drive_hot.wav`; `m06`, `m09` and `m12` have that plus `beads_wetdry_wet.wav`.
- **The saturator also runs in delay mode**, but not on the feedback path, which is taken inside the granular processor.
- **Ticket numbers aren't `CLAUDE.md` row numbers.** Row 6 (Shape) is ticket 09. Ticket 16 is under row 2.
- **The harness's random sequence depends on test run order.** Render ticket folders one test per process: `for t in TestGranularPlayer TestGranularProcessor TestProcessor TestProcessorWetDry TestProcessorWetDrive TestProcessorVoiceSteal TestProcessorCloud; do ./beads_test $t; done`. `measure.py --check` runs each test in its own process already.
- **A check's threshold must clear stock by a margin,** and must fail on the code before the change.
- **The Density sweep's full spread is dominated by the audio-rate ends** (Density 0.0 and 1.0). For texture, look at the 0.6–0.9 rows too.
- **The wavetable player still uses the stock per-grain gain.** Only the delay player normalises the bus.
- **The harness writes WAVs into the repo root.** For a ticket folder, render as above, move the files (including `beads_wetdry_wet_attacks.csv`) into `renders/<NN-name>/`, then `python3 beads/test/measure.py renders/<NN-name>/*.wav > renders/<NN-name>/metrics.txt`.
- **The listening page binds 127.0.0.1 only.** WAVs are under `/files/`. Changing the hash reloads the comparison, so it replaces whatever Jesse has loaded. Changes to `listen.py` need a restart; `listen_notes.json` and `listen.html` only need a page reload.
- **The click detector flags `kettel.wav`'s own note onsets** (85 in the dry render). Compare click counts against dry and stock, not as absolutes.
- **Always build with `./firmware.sh`, never bare `make`** (ADR 0002).
- **`/implement` and `/retro` aren't in the skill list.** Use `/mattpocock-skills:tdd`, or work from the ticket directly.
- **Reference source:** clip 09 of the Sublustris demos. Built by `~/Documents/Sites/sublustris-versio/docs/minisite/tools/demos/build_sources.py`, which writes float WAV, and the harness reads 16-bit, so convert. Don't use the lossy `*-dry.mp3`.

## Gates

`measure.py --check` run on `master` at `6f6880d`. The firmware and render figures are from the same code before `6f6880d`, which changed only docs and the listening notes.

- Harness: builds with 0 errors or warnings.
- Firmware: image ends at 0x0805ADB8, 1556 KB below the settings sector (stock 0x0805ABB0).
- `measure.py --check`: 9 of 9 pass. Wet − dry gated RMS +0.1 dB, cloud − dry +0.1 dB, Density sweep spread 4.8 dB, quiet render 0.00 dB against −37.35, hot render 0 clipped with peak −0.7 dBFS, hot cloud 0 clipped with peak −1.1 dBFS, shortest grain rise 3.12 ms, granular player 359 clicks.
- `levels.py renders/16-wet-level/*.wav`: no clipping. `beads_processor` peak −5.2 / RMS −32.1 dBFS, wet −2.5 / −27.1, cloud −7.6 / −27.2.
- Click baseline in `renders/16-wet-level/metrics.txt`: granular player 359, granular processor 319, processor 130, wet 95, cloud 358. Stock in `renders/stock/metrics.txt` (stock cloud 348, measured separately).

## Where to look

- `.scratch/soft-character/issues/09-shape-sweep.md`: the next ticket
- `.scratch/soft-character/spec.md`, decision 6, and `docs/adr/0003-shape-sweep-drops-the-reverse-window.md`
- `beads/dsp/grain_envelope.h`: the file ticket 09 rewrites
- `beads/test/checks.py`: add ticket 09's check here first
- `.scratch/soft-character/issues/16-wet-level-with-random-time.md`: Comments, the most recent sweep table format
