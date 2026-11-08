# 16: Wet level with Time randomised

Status: done
Blocked by: 05
Type: task
Spec: `../spec.md`, Spec decision on wet normalisation (ADR 0004)

## What

With Time randomised the wet sits about 4 dB under the dry and 5.8 dB under stock (`beads_cloud.wav`, found in ticket 08). The bus gain is 1/Σ env, which assumes grains add up in phase. Grains scattered in Time or Pitch don't, so they get turned down too far.

Blend the bus gain from 1/Σ towards 1/√Σ as the Time and Pitch randomness amounts rise. With no randomness the blend is exactly today's maths, so the fixed-Time renders stay bit-identical. On branch `exp/wet-level` until Jesse has judged it.

## Acceptance

- [x] `beads_cloud.wav` within 1 dB of the dry (gated RMS), by a check that fails before the change
- [x] Fixed-Time renders bit-identical to ticket 08
- [x] Hot render doesn't clip, and the Density sweep spread doesn't widen
- [x] Level across Density 0.6–1.0 and across Time spreads tabled in Comments
- [x] Jesse judges it by ear on the cloud
- [x] ADR 0004 amended
- [x] Standard gates

## Comments

**8 November 2026, sweep.** The bus gain's target is now `(1 − x)/Σ + x/√Σ`, where x is `kBusGainMaxIncoherence × min((Time randomness + Pitch randomness) × kBusGainSpreadScale, 1)`. Density's own randomness isn't used: `ComputeSpread` reads full spread across the whole random half, so it can't tell the fixed patch from the cloud.

Maximum blend, at sensitivity 20, gated RMS in dBFS. Dry is −25.5, stock cloud −23.7.

| Max blend | Cloud | Density 0.6 | 0.7 | 0.8 | 0.9 | 1.0 | Spread, 0.6–1.0 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 0 (ticket 08) | −29.5 | −27.0 | −27.5 | −26.9 | −29.7 | −34.7 | 7.8 dB |
| 0.25 | −27.9 | −27.0 | −27.5 | −26.4 | −28.1 | −30.5 | 4.1 dB |
| 0.4 | −27.1 | −27.0 | −27.4 | −26.1 | −27.3 | −28.6 | 2.5 dB |
| 0.5 | −26.6 | −27.0 | −27.4 | −26.0 | −26.8 | −27.6 | 1.6 dB |
| 0.6 | −26.1 | −26.9 | −27.3 | −25.8 | −26.3 | −26.7 | 1.5 dB |
| **0.75** | **−25.4** | −26.9 | −27.3 | −25.5 | −25.6 | −25.5 | 1.8 dB |
| 1.0 | −24.4 | −26.9 | −27.2 | −25.1 | −24.6 | −23.8 | 3.4 dB |

0.75 puts the cloud 0.1 dB above the dry, and takes Density 1.0 from −34.7 to −25.5, which was the weak spot ADR 0004 recorded.

Sensitivity, at maximum blend 0.75, by Time randomness (attenuverter), gated RMS:

| Sensitivity | 0 | 0.01 | 0.02 | 0.05 | 0.1 | 0.5 |
| --- | --- | --- | --- | --- | --- | --- |
| 20 | −24.3 | −28.2 | −27.4 | −25.3 | −25.4 | −25.7 |
| 50 | −24.3 | −26.9 | −25.1 | −25.3 | −25.4 | −25.7 |
| **100** | −24.3 | −25.1 | −25.1 | −25.3 | −25.4 | −25.7 |
| 200 | −24.3 | −25.1 | −25.1 | −25.3 | −25.4 | −25.7 |

Grains decorrelate at about 1% of the attenuverter, so the blend needs to be fully in by then. Chose 100. Constants landed: `kBusGainMaxIncoherence = 0.75`, `kBusGainSpreadScale = 100`.

Findings:

- **Fixed-Time renders are bit-identical at every value**: wet, dry, both drive renders and both steal renders.
- **Renders with Time randomised change, as intended.** Granular processor gated RMS goes from −27.0 to about −25.4, and its clicks stay at 316–321.
- **Hot cloud (source 6 dB up): 0 clipped, peak −1.1 dBFS.** That check is a guard and doesn't fail before the change.
- **CPU is unmeasured:** this adds one square root per sample on the wet bus. Needs the debug pin on the module.
- **Time modulated by CV counts as no randomness**, so it keeps the linear gain. Not rendered.

Gates: harness builds clean, `measure.py --check` passes 9 of 9, `levels.py renders/16-wet-level/*.wav` shows no clipping, firmware image ends at 0x0805ADB8, 1556 KB below the settings sector.

**8 November 2026, verdict.** Jesse kept it by ear on the cloud. Landed in `3d03f0d`, with the checks in `9cfff04`. CPU on the debug pin is still to measure, with ticket 05's.
