# 09: Shape sweep

Status: ready-for-human
Blocked by: 08
Type: task
Spec: `../spec.md`, Spec decision 6, ADR 0003

## What

Remap Shape in `beads/dsp/grain_envelope.h` to one continuous morph: decay, square, Tukey, Hann, Gaussian. Reverse is dropped.

## Acceptance

- [x] Every envelope parameter is continuous across the whole knob, so consecutive grains never jump
- [ ] The Gaussian is computed cheaply (a lookup table if needed), with CPU measured. Table done; CPU needs the debug pin
- [x] Windows reach zero at both edges, with no DC step
- [x] Renders at five Shape positions, with the measurement tool run against stock
- [x] Standard gates, plus Jesse's A/B verdict

## Comments

**8 November 2026, landed on `exp/shape-sweep`.** Checks in `46b2679` (both failed first: landmarks wrong at every quarter, 0.085 between neighbouring positions), morph in `9836485`.

Layout, with `x = 4 × Shape`: decay at 0, square at 0.25, Tukey (a quarter taper each side) at 0.5, Hann at 0.75, Gaussian at 1. The Gaussian is a 130-point table, sigma 0.3 of a half-window, offset to reach zero at the edge (`lut_gaussian`, 520 bytes). It's only read above 0.75. Hann is SmoothStep, within about 1% of a true raised cosine, as stock's bell was. `resources.cc` and `.h` were spliced by hand because the resource compiler needs Python 2; `lookup_tables.py` has the matching source.

Largest change between neighbouring positions is now 0.050, limit 0.065. Edge value 0.0001.

Cloud patch, 30 s each. Knob positions mean different windows in stock.

| Shape | New window | Gated RMS | Clicks | Stock window | Stock gated RMS | Stock clicks |
| --- | --- | --- | --- | --- | --- | --- |
| 0.00 | decay | −27.1 | 371 | square | −19.3 | 433 |
| 0.25 | square | −24.4 | 447 | towards decay | −23.4 | 360 |
| 0.50 | Tukey | −24.7 | 375 | just past decay | −26.9 | 337 |
| 0.75 | Hann | −25.3 | 379 | just past bell | −24.0 | 402 |
| 1.00 | Gaussian | −25.6 | 340 | reverse | −24.3 | 403 |

Stock's levels include its 1.5 wet gain, so read the shape of the column, not the absolute figures. Stock's bell on this patch (Shape 0.66, `renders/stock-shape/beads_cloud.wav`): −23.7 dB, 348 clicks.

Test patches that compare against stock moved to the position with the same window (0.33 to 0.0025, 0.3 to 0.025), and renders on those differ from `renders/16-wet-level/` by at most 1 LSB on 2 to 93 samples, so the quiet guard didn't need re-baselining. The cloud moved from 0.66 to 0.75 (Hann). `TestGranularProcessor` stays at 0.5: stock's window there (mostly decay, with a long attack) doesn't exist on the new knob, so its figures no longer compare with stock.

Knob feel on hardware not yet heard: stock's 12 o'clock was near decay and is now Tukey.

**8 November 2026, verdict.** Jesse kept it after the A/B on the listening page, so the Gaussian stays at sigma 0.3. CPU on the debug pin is still to measure, so this stays `ready-for-human` until then.
