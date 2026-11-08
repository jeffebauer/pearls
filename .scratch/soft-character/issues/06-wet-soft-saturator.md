# 06: Wet soft saturator

Status: done
Blocked by: 05
Type: task
Spec: `../spec.md`, Spec decision 4

## What

Add a gentle tanh-style soft clip on the wet bus, after normalisation and before the mix. The stock limiter before the reverb stays.

## Acceptance

- [x] Threshold starts around −6 dBFS and is tuned from renders (−6 kept; sweep below)
- [x] No change below the threshold (within 0.1 dB)
- [x] Standard gates, plus Jesse's A/B verdict

## Comments

**8 November 2026, implementation.** `SoftSaturate()` in `beads/dsp/processor.cc` runs on each wet channel just before the dry/wet mix. It's linear up to `kWetSaturatorThreshold` (0.5, −6 dBFS), then `threshold + range · SoftClip((|x| − threshold) / range)` toward `kWetSaturatorCeiling` (1.0). `SoftClip` is stmlib's Padé tanh, and the curve has slope 1 at the knee, so there's no corner. Levels are on the bus before the 0.94 wet gain, so the output ceiling is −0.5 dBFS. Feedback is taken inside the granular processor, so it doesn't pass through the saturator. Delay mode does.

Checks (`9e22a90`): `TestProcessorWetDrive` renders the wet path with the source 12 dB down and 6 dB up. The quiet pass is a guard (gated RMS −37.50 dBFS, which passes before and after by design). The hot pass clipped 7 samples before the saturator and clips 0 after.

Threshold sweep (fully wet, Density 0.9). The share of samples over each threshold, measured on the bus before saturation:

| Threshold | Normal level | Source +6 dB | Wet peak, normal | Hot peak |
| --- | --- | --- | --- | --- |
| off | — | — | −2.5 | 0.0 (7 clipped) |
| −3 dBFS | 0.000% | ~0.005% | −2.6 | −0.6 |
| −6 dBFS | 0.001% | 0.038% | −2.8 | −0.7 |
| −9 dBFS | 0.005% | 0.29% | −3.1 | −0.9 |
| −12 dBFS | 0.037% | 1.6% | −3.4 | −1.1 |

RMS, gated RMS and click counts move by 0.01 dB or less at every threshold. At −6 dBFS it's a safety net for overs, and it doesn't colour normal playing. Audible softening of dense clouds would need about −9 to −12. Kept −6 as the spec says; the listening set is in `renders/06-wet-saturator/sweep/` (`off`, `m06`, `m09`, `m12`), for Jesse to choose by ear.

Gates: harness builds clean. Firmware image ends at 0x0805AF28, 1556 KB below the settings sector. `measure.py --check` passes 5 of 5. `levels.py renders/06-wet-saturator/*.wav` shows no clipping. Granular player, granular processor and dry renders are bit-identical to ticket 05's.

**8 November 2026, verdict.** Jesse compared off, −6, −9 and −12 dBFS on the source +6 dB render and chose to keep −6 dBFS. No code change: `a081d46` already has it.
