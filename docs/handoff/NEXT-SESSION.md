# Next session: after 8 November 2026 (ticket 04 done, listening page added)

## Where things stand

On `master` at `2f09e03`, pushed to origin, which is level with master. This handoff is a checkpoint commit on top of that, and it is not pushed. **Ticket 04 is done:** `d4381c0` sets `post_gain_wet` from 1.5 to 0.94 (`beads/dsp/processor.cc:212`) and adds a second check, "fully wet render doesn't clip". Jesse's A/B verdict is recorded in the ticket ("B sounds good", LUFS-matched against stock). `53b327e` adds the listening page (`beads/test/listen.py` and `listen.html`, `listen` in `.claude/launch.json`). `CLAUDE.md` row 1 is marked done. **Nothing flashed. No settings backup exists.**

## Start here

**Ticket 05**, `.scratch/soft-character/issues/05-envelope-sum-normalisation.md`: replace the per-grain 1/√(n+1) set at grain birth (`beads/dsp/granular_player.h:122`) with a bus gain of 1/√(max(Σenv², 1)). The gain falls fast when grains pile up and recovers slowly (spec decision 3, ADR 0004). It's next in `CLAUDE.md`'s order.

- The desktop part can go ahead now: write the check first (RMS spread across a Density sweep narrower than stock), then make it pass. Keep the two ticket 04 checks passing. If the wet − dry check fails, re-tune `post_gain_wet` the way ticket 04 did and record the new value.
- **The CPU acceptance item is blocked:** it needs the debug pin on the module, so a flash, so the settings backup first. Do the desktop work and leave that box open, or ask Jesse whether to wait.
- When renders are ready, give Jesse a listening-page link with them preloaded (memory `listening-page`).

## Waiting on Jesse

- **Settings backup.** Read sector 15 (0x081E0000, 128 KB) with STM32CubeProgrammer into `backups/` (git-ignored), and copy it off this machine. It blocks any flash, and so the CPU measurements in tickets 05, 09 and 10.
- **Ticket 03, the reference captures**, once ticket 02 (`ready-for-agent`, not started) lands.
- **A/B verdicts** on each DSP ticket from 05 on.
- **The 15-ticket breakdown** is still unreviewed (07 and 08 could merge, for example).
- **Optional:** `uv` and a Perplexity key for `/research` (memory `research-script-broken`).

## In flight

Nothing. Tree clean apart from this handoff, no branches or worktrees. The `listen` preview server may still be running in the browser pane, and stopping it is harmless.

## Will bite

- **`renders/stock/beads_granular_player.wav` is stale.** It doesn't match a render made at `457bc9c`, before any DSP change. That render is deterministic, so the stock file came from some earlier state. `beads_processor.wav` and the wet/dry pair do match. Ticket 05 changes `granular_player.h`, so re-render stock `TestGranularPlayer` and `TestGranularProcessor` from `457bc9c` first (`git stash` or a worktree, rebuild the harness, render, move the files into `renders/stock/`, re-run `measure.py` into `metrics.txt`). `beads_granular_processor` wasn't hash-checked: its click count went from 336 to 276 with no code path through `processor.cc`, so assume it's stale too.
- **The wet peak is −0.4 dBFS** on the dense check patch at 0.94. Normalisation could push it either way, and the no-clip check catches it.
- **Renders:** the harness writes WAVs into the repo root. For a ticket folder, render `TestGranularPlayer`, `TestGranularProcessor`, `TestProcessor` and `TestProcessorWetDry`, move the files (including `beads_wetdry_wet_attacks.csv`) into `renders/<NN-name>/`, then `python3 beads/test/measure.py renders/<NN-name>/*.wav > renders/<NN-name>/metrics.txt`.
- **The listening page binds 127.0.0.1 only.** Changing the hash reloads the comparison, so you can link to a set of files directly.
- **`/implement` and `/retro` aren't in the skill list.** Use `/mattpocock-skills:tdd`, or work from the ticket directly.
- **The wet level depends on the patch:** the check uses Density 0.9 and gated RMS. See ticket 01's Comments for the density table.
- **The click detector flags `kettel.wav`'s own note onsets** (85 in the dry render). Compare click counts against dry and stock, not as absolutes.
- **The attack probe** is a test-only hook in `grain_envelope.h` (`#ifdef TEST`). Keep the two probe calls when ticket 09 rewrites the file.
- **`kInstantCrossfadeTime` is tied to the scheduler thresholds** (`granular_player.h:43`). Ticket 08 must check that ordering still holds.
- **Always build with `./firmware.sh`, never bare `make`** (ADR 0002).
- **Reference source:** clip 09 of the Sublustris demos. Built by `~/Documents/Sites/sublustris-versio/docs/minisite/tools/demos/build_sources.py`, which writes float WAV, and the harness reads 16-bit, so convert. Don't use the lossy `*-dry.mp3`.

## Gates

At `d4381c0`. Since then: tooling and docs only, no DSP changes.

- Harness: builds with 0 errors or warnings.
- Firmware: image ends at 0x0805ABB0, 1557 KB below the settings sector (unchanged from stock).
- `measure.py --check`: 2 of 2 pass. Wet − dry gated RMS is +0.2 dB, and the wet render has 0 clipped samples.
- `levels.py renders/04-wet-gain/*.wav`: no clipping. `beads_processor` peak −7.2 / RMS −33.0 dBFS (stock −3.2 / −29.3). Wet/dry: wet −0.4 / −27.0, dry −5.6 / −27.1.
- Metrics in `renders/04-wet-gain/metrics.txt`, stock in `renders/stock/metrics.txt`.

## Where to look

- `.scratch/soft-character/issues/05-envelope-sum-normalisation.md`: the next ticket
- `docs/adr/` (ADR 0004) and `.scratch/soft-character/spec.md`, decision 3
- `beads/dsp/granular_player.h`: the per-grain gain at line 122, `kInstantCrossfadeTime` at line 43
- `beads/test/checks.py`: add ticket 05's check here first
- `.scratch/soft-character/issues/04-wet-gain-matches-dry.md`: Comments, the method to repeat if the gain needs re-tuning
