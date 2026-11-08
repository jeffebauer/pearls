# 01: Render measurement tool

Status: done
Type: task
Spec: `../spec.md`, Spec decision 1

## What

Build a measurement tool for the desktop harness (likely `beads/test/measure.py`, plus envelope logging in `beads_test.cc`).

## Acceptance

- [x] Reports, per WAV, peak and RMS of the wet and the dry (the harness renders them separately) and their difference in dB
- [x] Click detector: splits audio into 1 ms frames and flags frames whose high-frequency energy (e.g. first-difference RMS) jumps well above the local median, with a count and timestamps
- [x] Grain attack time: the harness logs each grain's envelope, and the tool reports the minimum and median 10–90% rise time
- [x] Runs on stock renders and prints a baseline, saved in `renders/stock/metrics.txt`
- [x] Works on hardware captures too (level and click metrics only)
- [x] One check command (e.g. `python3 beads/test/measure.py --check`) renders, measures, and exits non-zero if any assertion fails. Assertions live in a single file so that each later ticket can add its own failing check first, for `/tdd`
- [x] The first assertion is "wet and dry within 1 dB at 12 o'clock". It fails on stock (+4.1 dB, not 6: see Comments). Ticket 04 is meant to make it pass
- [x] The check command is named in `.claude/session.md` under Gates

## Comments

**8 November 2026, implemented.**

- The harness didn't render wet and dry separately, so `TestProcessorWetDry` was added. It renders TestProcessor's patch twice (`dry_wet` 0 and 1) with reverb and feedback off, plus `beads_wetdry_wet_attacks.csv`, one row per grain. Both mix curves scale by 0.75 at 12 o'clock, so the end-to-end difference equals the 12 o'clock one.
- Attack logging uses a probe in `grain_envelope.h` under `#ifdef TEST`. The firmware image is unchanged (0x0805ABB0).
- The click detector uses the second difference, not the first, because it follows corners rather than pitch. Synthetic check: 0.5 ms linear edges and abrupt cuts are all flagged; 5 ms linear and Hann edges never are. On `kettel.wav` it also flags the source's own note onsets (85 in the dry), so compare wet click counts against dry and stock, not as absolutes.
- Hardware captures: the reader handles 16, 24 and 32-bit PCM, float and extensible WAVs. Not yet run on a real capture.
- **Wet level is patch-dependent.** The spec's 6 dB is the gain constants (1.5 against 0.75). Measured wet minus dry on stock, by Density and Size:

  | Density, size | Grains in 30 s | RMS | Gated RMS |
  | --- | --- | --- | --- |
  | 0.7, 0.75 (TestProcessor) | 89 | −2.0 dB | +0.3 dB |
  | 0.9, 0.75 | 748 | +4.1 dB | +4.1 dB |
  | 0.3, 0.75 | 141 | −1.6 dB | −1.5 dB |
  | 0.7, 0.3 | 89 | −16.0 dB | +0.1 dB |

  Jesse chose a dense patch (Density 0.9) and gated RMS (RMS over the non-silent 1 ms frames) for the assertion. Stock fails it at +4.1 dB.
- Stock fully wet clips 8 samples at this patch (peak 0.0 dBFS).
- Baseline is in `renders/stock/metrics.txt`. Stock attacks are 0.52 ms for every grain.

