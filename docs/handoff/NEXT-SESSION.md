# Next session: after 8 November 2026 (ticket 05 done bar CPU)

## Where things stand

On `master` at `04794c2`, pushed to origin and level with it. This handoff is a checkpoint commit on top, not pushed. **Ticket 05 landed** in `bcf5e36`: the delay player scales the grain sum by 1/max(Σenv, 1), dropping at once and recovering over 100 ms, replacing the per-grain 1/√(n+1) at birth. **Linear, not the √ that ADR 0004 first named.** An exponent sweep showed √ made the wet level climb with Density and clip; Jesse chose linear, and ADR 0004, spec decision 3 and `CLAUDE.md` row 2 are amended. `44df310` added `TestProcessorDensitySweep` and the spread check. Jesse's A/B verdict: "Sounds good". The ticket is `ready-for-human`, because only the CPU measurement is left. Stale stock renders are fixed: `renders/stock/beads_granular_player.wav` and `beads_granular_processor.wav` were re-rendered from `457bc9c`. **Nothing flashed. No settings backup exists.**

## Start here

**Ticket 06**, `.scratch/soft-character/issues/06-wet-soft-saturator.md` (`CLAUDE.md` row 3): a tanh-style soft clip on the wet bus, after normalisation and before the mix, from about −6 dBFS. The stock pre-reverb limiter stays. It's desktop-only, so the missing backup doesn't block it.

- The wet gain is applied in `beads/dsp/processor.cc:212–215` (`post_gain_wet` 0.94, then `wet_fade`). The pre-reverb limiter is at line 230.
- Write the check first: "no change below the threshold, within 0.1 dB". Then tune the threshold from renders, and keep the three existing checks passing.
- The wet peak is now −2.5 dBFS on the check patch (it was −0.4 before ticket 05), so a −6 dBFS knee will touch dense clouds.
- When renders are ready, give Jesse a listening-page link with them preloaded (memory `listening-page`).

## Waiting on Jesse

- **Settings backup.** Read sector 15 (0x081E0000, 128 KB) with STM32CubeProgrammer into `backups/` (git-ignored), and copy it off this machine. It blocks any flash, and so the CPU measurements in tickets 05, 09 and 10 (the CPU box in 05 is the only thing keeping it open).
- **Ticket 03, the reference captures**, once ticket 02 (`ready-for-agent`, not started) lands.
- **A/B verdicts** on each DSP ticket from 06 on.
- **The 15-ticket breakdown** is still unreviewed (07 and 08 could merge, for example).
- **Optional:** `uv` and a Perplexity key for `/research` (memory `research-script-broken`).

## In flight

Nothing. Tree clean apart from this handoff, no branches or worktrees. The `listen` server is still running on 127.0.0.1:8765, and stopping it is harmless.

## Will bite

- **Ticket numbers aren't `CLAUDE.md` row numbers.** Row 3 is ticket 06, row 4 (minimum attack) is ticket 07, row 5 (voice-stealing fade) is ticket 08.
- **The harness's random sequence depends on test run order.** `./beads_test A B` in one process gives different renders from running each alone, which shifted click counts by about 10 (146 against 136). Render ticket folders **one test per process**: `for t in TestGranularPlayer TestGranularProcessor TestProcessor TestProcessorWetDry; do ./beads_test $t; done`. `measure.py --check` runs each test in its own process already.
- **Clicks rose with ticket 05:** against stock, granular player 412 (395), granular processor 321 (276), processor 136 (132), wet 97 (98). A 1 ms or 3 ms gain fall didn't help. Likely cause: grains now start at full level, so their 0.5 ms attacks stand out against the bed. Ticket 07 should start from these numbers.
- **A check's threshold must clear stock by a margin.** Ticket 05's spread check first passed on stock, because the constant was rounded (14.6 against a measured 14.57). It now needs 1 dB under stock. Check new checks fail on the code before the change.
- **The Density sweep's full spread is dominated by the audio-rate ends** (Density 0.0 and 1.0). For texture, look at the 0.6–0.9 rows too.
- **With Time randomised, linear normalisation is weaker:** Density 1.0 sits about 5 dB under 0.6–0.9 (table in ticket 05's Comments). Bear it in mind for ticket 12 (Density as overlap).
- **The wavetable player still uses the stock per-grain gain.** Only the delay player normalises the bus.
- **The harness writes WAVs into the repo root.** For a ticket folder, render as above, move the files (including `beads_wetdry_wet_attacks.csv`) into `renders/<NN-name>/`, then `python3 beads/test/measure.py renders/<NN-name>/*.wav > renders/<NN-name>/metrics.txt`.
- **The listening page binds 127.0.0.1 only.** Changing the hash reloads the comparison, so you can link to a set of files directly.
- **The click detector flags `kettel.wav`'s own note onsets** (85 in the dry render). Compare click counts against dry and stock, not as absolutes.
- **The attack probe** is a test-only hook in `grain_envelope.h` (`#ifdef TEST`). Keep the two probe calls when ticket 09 rewrites the file.
- **`kInstantCrossfadeTime` is tied to the scheduler thresholds** (`granular_player.h:43`). Ticket 08 must check that ordering still holds.
- **Always build with `./firmware.sh`, never bare `make`** (ADR 0002).
- **`/implement` and `/retro` aren't in the skill list.** Use `/mattpocock-skills:tdd`, or work from the ticket directly.
- **Reference source:** clip 09 of the Sublustris demos. Built by `~/Documents/Sites/sublustris-versio/docs/minisite/tools/demos/build_sources.py`, which writes float WAV, and the harness reads 16-bit, so convert. Don't use the lossy `*-dry.mp3`.

## Gates

At `bcf5e36`. `04794c2` is docs only.

- Harness: builds with 0 errors or warnings.
- Firmware: image ends at 0x0805AD60, 1556 KB below the settings sector (stock 0x0805ABB0).
- `measure.py --check`: 3 of 3 pass. Wet − dry gated RMS −0.1 dB, 0 clipped in the wet render, Density sweep spread 4.7 dB (stock 14.6, needs 13.6 or less).
- `levels.py renders/05-envelope-sum/*.wav`: no clipping. `beads_processor` peak −5.2 / RMS −32.1 dBFS. Wet/dry: wet −2.5 / −27.3, dry −5.6 / −27.1.
- Metrics in `renders/05-envelope-sum/metrics.txt`, stock in `renders/stock/metrics.txt`. The sweep's listening set is in `renders/05-envelope-sum/pre`, `q1` and `q2`.

## Where to look

- `.scratch/soft-character/issues/06-wet-soft-saturator.md`: the next ticket
- `.scratch/soft-character/spec.md`, decision 4
- `beads/dsp/processor.cc:200–246`: wet gain, mix and both limiters
- `beads/test/checks.py`: add ticket 06's check here first
- `.scratch/soft-character/issues/05-envelope-sum-normalisation.md`: Comments, the sweep method and click baseline
