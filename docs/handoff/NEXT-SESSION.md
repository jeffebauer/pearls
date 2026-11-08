# Next session: after 8 November 2026 (ticket 06 done, 07 next)

## Where things stand

On `master` at `75cd424`, with this handoff as a checkpoint commit on top. Origin is at `04794c2`, so everything since then is committed but **not pushed** (`991d77e` and the five commits below, plus this one). **Ticket 06 is done** in `a081d46`: `SoftSaturate()` in `beads/dsp/processor.cc` runs on each wet channel just before the dry/wet mix, linear to −6 dBFS, then stmlib's Padé tanh toward a 0 dBFS ceiling, measured before the 0.94 wet gain. Jesse compared off, −6, −9 and −12 dBFS and kept −6 (`75cd424`). `9e22a90` added `TestProcessorWetDrive` and two checks. `02838a7` gave the listening page names and descriptions, from `beads/test/listen_notes.json`. Ticket 05 is still `ready-for-human`, with only its CPU measurement left. **Nothing flashed. No settings backup exists.**

## Start here

**Ticket 07**, `.scratch/soft-character/issues/07-minimum-grain-attack.md` (`CLAUDE.md` row 4): raise `min_attack_`, set at `beads/dsp/grain_envelope.h:64` (`0.0005f * sample_rate`). It's the likeliest fix for the clicks that rose with ticket 05, and it's desktop-only, so the missing backup doesn't block it.

- Sweep at least 0.5, 1, 2, 3 and 5 ms, and score each with the click detector and attack-time metric. Table the results in the ticket's Comments.
- Click baseline to beat, at `a081d46`: granular player 412, granular processor 321, processor 136, wet 97. Stock: 395, 276, 135, 98. In `renders/06-wet-saturator/metrics.txt`.
- Sweep method that worked for ticket 06: copy the source file to the scratchpad, `sed` the constant, build, render one test per process, move the WAVs out, then restore the file and rebuild. Run it under `bash`, since zsh doesn't word-split.
- Render into `renders/07-<name>/`, add every new folder to `beads/test/listen_notes.json`, and give Jesse a preloaded listening link. Jesse picks the value by ear.

## Waiting on Jesse

- **Settings backup.** Read sector 15 (0x081E0000, 128 KB) with STM32CubeProgrammer into `backups/` (git-ignored), and copy it off this machine. It blocks any flash, and so the CPU measurements in tickets 05, 09 and 10.
- **Ticket 03, the reference captures**, once ticket 02 (`ready-for-agent`, not started) lands.
- **A/B verdicts** on each DSP ticket from 07 on.
- **The 15-ticket breakdown** is still unreviewed (07 and 08 could merge, for example).
- **Optional:** `uv` and a Perplexity key for `/research` (memory `research-script-broken`).

## In flight

Nothing. Tree clean apart from this handoff, no branches or worktrees. The `listen` preview is running on 127.0.0.1:8765, and stopping it is harmless.

## Will bite

- **Ticket 06's quiet guard will probably trip on later tickets.** `quiet_wet_unchanged` in `beads/test/checks.py` compares against a fixed −37.50 dBFS measured at `bcf5e36`. A ticket that legitimately changes the grains (07 onward) can move that level by more than 0.1 dB without touching the saturator. If it fails, confirm the cause, then re-baseline the constant in that ticket's check commit. Don't loosen the tolerance.
- **`.claude/session.md` is stale on one line:** it says `measure.py --check` is expected to fail until ticket 04 or 05. All 5 checks passed at `a081d46`, so a failure now is real.
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
- **The listening page binds 127.0.0.1 only.** Changing the hash reloads the comparison, so you can link to a set of files directly. A shell connection to port 8765 was refused this session and `preview_start` then got the port fresh, so the earlier server had probably stopped (unsure). Start it with `preview_start` and don't assume it's up. Changes to `listen.py` need a restart; `listen_notes.json` and `listen.html` only need a page reload.
- **The click detector flags `kettel.wav`'s own note onsets** (85 in the dry render). Compare click counts against dry and stock, not as absolutes.
- **The attack probe** is a test-only hook in `grain_envelope.h` (`#ifdef TEST`). Keep the two probe calls when ticket 09 rewrites the file.
- **`kInstantCrossfadeTime` is tied to the scheduler thresholds** (`granular_player.h:43`). Ticket 08 must check that ordering still holds.
- **Always build with `./firmware.sh`, never bare `make`** (ADR 0002).
- **`/implement` and `/retro` aren't in the skill list.** Use `/mattpocock-skills:tdd`, or work from the ticket directly.
- **Reference source:** clip 09 of the Sublustris demos. Built by `~/Documents/Sites/sublustris-versio/docs/minisite/tools/demos/build_sources.py`, which writes float WAV, and the harness reads 16-bit, so convert. Don't use the lossy `*-dry.mp3`.

## Gates

At `a081d46`. `02838a7` and `75cd424` changed only the listening page and docs, so the gates weren't re-run for those.

- Harness: builds with 0 errors or warnings.
- Firmware: image ends at 0x0805AF28, 1556 KB below the settings sector (stock 0x0805ABB0).
- `measure.py --check`: 5 of 5 pass. Wet − dry gated RMS −0.1 dB, 0 clipped in the wet render, Density sweep spread 4.7 dB, quiet render +0.00 dB against −37.50, hot render 0 clipped with peak −0.7 dBFS.
- `levels.py renders/06-wet-saturator/*.wav`: no clipping. `beads_processor` peak −5.2 / RMS −32.1 dBFS. Wet/dry: wet −2.8 / −27.3, dry −5.6 / −27.1.
- Granular player, granular processor and dry renders are bit-identical to ticket 05's.
- Metrics in `renders/06-wet-saturator/metrics.txt`, stock in `renders/stock/metrics.txt`.

## Where to look

- `.scratch/soft-character/issues/07-minimum-grain-attack.md`: the next ticket
- `.scratch/soft-character/spec.md`, decision 5
- `beads/dsp/grain_envelope.h:64` and `:97`: where `min_attack_` is set and used
- `beads/test/checks.py`: add ticket 07's check here first
- `.scratch/soft-character/issues/05-envelope-sum-normalisation.md`: Comments, the click baseline
- `.scratch/soft-character/issues/06-wet-soft-saturator.md`: Comments, the sweep table format
