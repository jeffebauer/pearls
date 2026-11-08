# 05: Envelope sum normalisation

Status: ready-for-agent
Blocked by: 04
Type: task
Spec: `../spec.md`, Spec decision 3, ADR 0004

## What

Replace the per-grain 1/√(n+1) at birth (`beads/dsp/granular_player.h:122`) with a bus gain of g = 1/max(Σenv, 1), computed from the live envelope values. (First written as 1/√(max(Σenv², 1)); changed to linear on 8 November 2026 after the sweep in Comments.)

## Acceptance

- [ ] CPU measured on the debug pin before and after (record both)
- [x] Gain smoothing is asymmetric: it falls fast when grains pile up and recovers slowly
- [x] A lone grain's fade is unchanged (the floor of 1)
- [x] In the measurement tool, the RMS spread across a Density sweep is narrower than stock
- [x] The check command's "wet and dry within 1 dB at 12 o'clock" assertion still passes; if not, re-tune the wet post-gain in the same way as ticket 04 and record the new value
- [ ] Standard gates, plus Jesse's A/B verdict

## Comments

### 8 November 2026: the √ law doesn't flatten level across Density

Implemented as ADR 0004 states (uncommitted): grains start at gain 1, `DelayGrain::Render` returns its envelope, and the delay player scales the grain sum by g = 1/√(max(Σenv², 1)). g drops at once and recovers with a 100 ms one-pole. The wavetable player still uses the stock per-grain gain. Stale stock `beads_granular_player.wav` and `beads_granular_processor.wav` were re-rendered from `457bc9c` (both hashes now match master's renders).

New harness test `TestProcessorDensitySweep` renders the wet path at Density 0.0–0.4 and 0.6–1.0 (15 s each), and the check in `checks.py` needs a gated-RMS spread at least 1 dB under stock's 14.57 dB. On the check patch (Time fixed) it passes at 12.6 dB, but only because the audio-rate ends (0.0, 1.0) come in. Across the random half the level now climbs with Density. Also tried with Time randomised (attenuverter −0.5, experiment only):

| Patch, law | Spread | Random half (0.6–1.0) | Wet − dry | Clipped |
| --- | --- | --- | --- | --- |
| Fixed Time, stock | 14.6 | 7.5 | +0.2 | 0 at 0.9 |
| Fixed Time, √ (ADR) | 12.6 | 10.9 | +5.3 | 90 |
| Fixed Time, linear 1/max(Σenv, 1) | 5.1 | 3.2 | −0.1 | 0 |
| Fixed Time, √ of Σenv²/0.375 | 16.2 | 13.0 | +1.1 | 33 |
| Random Time, stock | 3.3 | 1.2 | −4.0 | 0 |
| Random Time, √ (ADR) | 6.0 | 5.2 | +1.3 | 0 |
| Random Time, linear | 7.7 | 5.9 | −3.9 | 0 |
| Random Time, √ of Σenv²/0.375 | 5.5 | 4.7 | −2.9 | 0 |

Readings: with Time fixed, neighbouring grains read near-identical audio, so they add closer to linearly than as √n, and √ under-corrects. Stock's 1/√(n+1) counts fading and stolen grains too, so it over-attenuates dense clouds, which happens to offset the correlation. Linear is flat across 0.6–0.9 on both patches; only the audio-rate ends fall away on random Time. Needs Jesse's call before going further (ADR 0004 names √).

### 8 November 2026: exponent sweep

Normaliser generalised to a power mean, g = 1/max((Σenv^q)^(1/q), 1): q = 2 is the ADR's √Σenv², q = 1 is linear Σenv. The test-only knobs `BUS_NORM_Q` and `TIME_RANDOM` (environment variables read by `beads_test`) drive it; they come out once q is chosen. Gated-RMS spread in dB, wet − dry at Density 0.9, clipped samples over the sweep and the wet render:

| q | Fixed Time: 0.6–1.0 | Fixed Time: wet − dry | Fixed Time: clipped | Random Time: 0.6–1.0 | Random Time: wet − dry |
| --- | --- | --- | --- | --- | --- |
| stock | 7.5 | +0.2 | 0 at 0.9 | 1.2 | −4.0 |
| 1 | **3.2** | **−0.1** | **0** | 5.9 | −4.0 |
| 1.25 | 7.5 | +2.2 | 3 | **2.4** | −1.8 |
| 1.5 | 10.1 | +3.6 | 43 | 3.0 | −0.5 |
| 1.75 | 10.6 | +4.6 | 65 | 3.9 | +0.4 |
| 2 | 10.9 | +5.3 | 83 | 5.2 | +1.1 |

Proposed: q = 1. Best worst case over both patches (5.9 dB, against stock's 7.5), no clipping, wet within 0.1 dB of dry at the existing 0.94 gain, and the cheapest on the module (no squares or square root: one add per grain, one divide per sample). Its weak spot is random Time at Density 1.0, which falls 5 dB below 0.6–0.9. Fixed Time is the default patch, so that weighs more. Needs ADR 0004 amended if chosen. Listening set in `renders/05-envelope-sum/` (`pre`, `q1`, `q2`).

### 8 November 2026: landed as linear (q = 1)

Jesse chose q = 1. ADR 0004, spec decision 3 and `CLAUDE.md` row 2 are amended. The sweep knobs are removed; the wavetable player keeps the stock per-grain gain.

- Gain: drops at once to 1/Σenv, recovers with a 100 ms one-pole (`kBusGainRecoveryTime`). A 1 ms or 3 ms fall instead made no difference to clicks.
- `measure.py --check`: 3 of 3 pass. Wet − dry −0.1 dB at the unchanged 0.94 post-gain, 0 clipped, Density sweep spread 4.7 dB (stock 14.6).
- Firmware: image ends at 0x0805AD60, 1556 KB below the settings sector (stock 0x0805ABB0).
- Renders in `renders/05-envelope-sum/` (one test per process; the harness's random sequence depends on run order). No clipping. Clicks against stock: granular player 412 (395), granular processor 321 (276), processor 136 (132), wet 97 (98). Grains no longer start pre-attenuated, so their 0.5 ms attacks stand out more against the bed; ticket 07 (minimum attack) is aimed at those edges.
- Still open: CPU on the debug pin (needs the settings backup and a flash), and Jesse's A/B verdict.

