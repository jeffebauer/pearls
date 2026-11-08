# Next session: after 8 November 2026 (ticket 07 done, 08 next)

## Where things stand

On `master` at `edd9045`, with this handoff as a checkpoint commit on top. Origin is at `04794c2`, so the 10 commits from `991d77e` to `edd9045`, plus this one, are committed but **not pushed**. **Ticket 07 is done** in `7a18a0f`: `min_attack_` went from 0.5 ms to 3 ms, chosen by Jesse from a 0.5, 1, 2, 3 and 5 ms sweep in `renders/07-min-attack/sweep/`. `dbe79e6` added two checks, and `edd9045` recorded the verdict and gates. Ticket 05 is still `ready-for-human`, with only its CPU measurement left. **Nothing flashed. No settings backup exists.**

## Start here

**Ticket 08**, `.scratch/soft-character/issues/08-voice-stealing-fade.md` (`CLAUDE.md` row 5): lengthen `kInstantCrossfadeTime` at `beads/dsp/granular_player.h:43` (5 ms now). The granular processor render still has 321 clicks against stock's 276, and ticket 07 proved its attack floor can't reach them, so the fade is the next suspect. It's desktop-only, so the missing backup doesn't block it.

- Sweep at least 5, 10, 15 and 20 ms at high density, scored by the click detector. Table the results in the ticket's Comments.
- It must stay below `kSimpleSchedulerThreshold` (25 ms, `granular_player.h:44`), because the code relies on that ordering to avoid a sort. 20 ms is fine; 25 ms or more needs the thresholds moved too.
- Click baseline to beat, at `7a18a0f`: granular player 359, granular processor 321, processor 130, wet 95. Stock: 395, 276, 135, 98. In `renders/07-min-attack/metrics.txt`.
- Sweep method: copy the source file to the scratchpad, `sed` the constant, build, render one test per process, move the WAVs to `renders/08-<name>/sweep/<value>/`, then restore the file and rebuild. Run it under `bash`, since zsh doesn't word-split. Render all five tests for every value, so each folder is complete.
- Add every new folder to `beads/test/listen_notes.json`, reload the page, and give Jesse a preloaded listening link. Jesse picks the value by ear.
- If the 321 doesn't move here either, look at the bus gain's instant drop (ticket 05) before treating it as a ticket 08 failure.

## Waiting on Jesse

- **Settings backup.** Read sector 15 (0x081E0000, 128 KB) with STM32CubeProgrammer into `backups/` (git-ignored), and copy it off this machine. It blocks any flash, and so the CPU measurements in tickets 05, 09 and 10.
- **Ticket 03, the reference captures**, once ticket 02 (`ready-for-agent`, not started) lands.
- **A/B verdicts** on each DSP ticket from 08 on.
- **The 15-ticket breakdown** is still unreviewed.
- **A push**, when you want one. The 11 unpushed commits exist only on this machine.
- **Optional:** `uv` and a Perplexity key for `/research` (memory `research-script-broken`).

## In flight

Nothing. Tree clean apart from this handoff, no branches or worktrees. The `listen` preview is running on 127.0.0.1:8765, and stopping it is harmless.

## Will bite

- **Ticket 06's quiet guard will probably trip on later tickets.** `quiet_wet_unchanged` in `beads/test/checks.py` compares against a fixed −37.35 dBFS, re-baselined at ticket 07 (the constant is still called `STOCK_…`, though it isn't stock any more). A ticket that legitimately changes the grains can move that level by more than 0.1 dB without touching the saturator. If it fails, confirm the cause, then re-baseline the constant. Don't loosen the tolerance.
- **A sweep value can be a no-op on a render.** At ticket 07 the granular processor render was bit-identical at every value, because its long grains never reach the attack floor. `cmp` each render against the control before reading anything into a flat score.
- **Shape 0 with short grains is unrendered.** The attack floor is multiplied by `1 + 3 × sustain`, so at Shape fully anticlockwise the 3 ms floor gives a 12 ms attack. Judge it on the module, or add a render.
- **The wet render's click count is near the detector's noise.** The dry has 85 of its 93–99. Use the granular player render to judge grain-edge changes.
- **A "no change" check can't fail before the change.** Pair it with a check that does, as ticket 06 did: quiet is the guard, and the hot render clipping 7 samples is the one that failed first.
- **Sweep folders are partial.** `renders/06-wet-saturator/sweep/off/` has only `beads_drive_hot.wav`; `m06`, `m09` and `m12` have that plus `beads_wetdry_wet.wav`. Those three came from a run of the same patch but were moved by hand, so their processor renders aren't there.
- **The saturator also runs in delay mode**, but not on the feedback path, which is taken inside the granular processor.
- **Ticket numbers aren't `CLAUDE.md` row numbers.** Row 4 (minimum attack) is ticket 07, row 5 (voice-stealing fade) is ticket 08.
- **The harness's random sequence depends on test run order.** `./beads_test A B` in one process gives different renders from running each alone, which shifted click counts by about 10. Render ticket folders **one test per process**: `for t in TestGranularPlayer TestGranularProcessor TestProcessor TestProcessorWetDry TestProcessorWetDrive; do ./beads_test $t; done`. `measure.py --check` runs each test in its own process already.
- **A check's threshold must clear stock by a margin.** Ticket 05's spread check first passed on stock, because the constant was rounded. Check new checks fail on the code before the change.
- **The Density sweep's full spread is dominated by the audio-rate ends** (Density 0.0 and 1.0). For texture, look at the 0.6–0.9 rows too.
- **With Time randomised, linear normalisation is weaker:** Density 1.0 sits about 5 dB under 0.6–0.9 (table in ticket 05's Comments). Bear it in mind for ticket 12 (Density as overlap).
- **The wavetable player still uses the stock per-grain gain.** Only the delay player normalises the bus.
- **The harness writes WAVs into the repo root.** For a ticket folder, render as above, move the files (including `beads_wetdry_wet_attacks.csv`) into `renders/<NN-name>/`, then `python3 beads/test/measure.py renders/<NN-name>/*.wav > renders/<NN-name>/metrics.txt`.
- **The listening page binds 127.0.0.1 only.** Changing the hash reloads the comparison, so you can link to a set of files directly, but it replaces whatever Jesse has loaded, loop included. Start it with `preview_start` and don't assume it's up. Changes to `listen.py` need a restart; `listen_notes.json` and `listen.html` only need a page reload, and until then new folders show as bare paths.
- **The click detector flags `kettel.wav`'s own note onsets** (85 in the dry render). Compare click counts against dry and stock, not as absolutes.
- **The attack probe** is a test-only hook in `grain_envelope.h` (`#ifdef TEST`). Keep the two probe calls when ticket 09 rewrites the file.
- **`kInstantCrossfadeTime` is tied to the scheduler thresholds** (`granular_player.h:43`). Ticket 08 must check that ordering still holds.
- **Always build with `./firmware.sh`, never bare `make`** (ADR 0002).
- **`/implement` and `/retro` aren't in the skill list.** Use `/mattpocock-skills:tdd`, or work from the ticket directly.
- **Reference source:** clip 09 of the Sublustris demos. Built by `~/Documents/Sites/sublustris-versio/docs/minisite/tools/demos/build_sources.py`, which writes float WAV, and the harness reads 16-bit, so convert. Don't use the lossy `*-dry.mp3`.

## Gates

At `7a18a0f`. `edd9045` changed only docs and the listening notes, so the gates weren't re-run for it.

- Harness: builds with 0 errors or warnings.
- Firmware: image ends at 0x0805AF30, 1556 KB below the settings sector (stock 0x0805ABB0).
- `measure.py --check`: 7 of 7 pass. Wet − dry gated RMS +0.1 dB, 0 clipped in the wet render, Density sweep spread 4.8 dB, quiet render 0.00 dB against −37.35, hot render 0 clipped with peak −0.7 dBFS, shortest grain rise 3.12 ms, granular player 359 clicks.
- `levels.py renders/07-min-attack/*.wav`: no clipping. `beads_processor` peak −5.2 / RMS −32.1 dBFS. Wet/dry: wet −2.5 / −27.1, dry −5.6 / −27.1.
- The ticket renders are bit-identical to `renders/07-min-attack/sweep/a3/`.
- Metrics in `renders/07-min-attack/metrics.txt`, stock in `renders/stock/metrics.txt`.

## Where to look

- `.scratch/soft-character/issues/08-voice-stealing-fade.md`: the next ticket
- `.scratch/soft-character/spec.md`, decision 5
- `beads/dsp/granular_player.h:41` to `:45`: the fade constant and the ordering it must keep
- `beads/test/checks.py`: add ticket 08's check here first
- `.scratch/soft-character/issues/07-minimum-grain-attack.md`: Comments, the click baseline and sweep table format
- `.scratch/soft-character/issues/05-envelope-sum-normalisation.md`: Comments, where the granular processor's clicks rose
