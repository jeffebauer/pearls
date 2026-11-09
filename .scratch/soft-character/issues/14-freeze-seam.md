# 14: Freeze seam

Status: done
Blocked by: 01
Type: task
Spec: `../spec.md`, Spec decision 11

## What

Render frozen audio across the loop point and run the click detector. If it finds a click, add a crossfade at the seam (`beads/dsp/delay_line.h`, near `freeze_tail_size`). If not, record the result and close.

## Acceptance

- [x] Measurement result recorded in Comments
- [x] If a crossfade is built: not built, because there's no click at the seam

## Comments

**9 November 2026, measured: no click at the seam, so no crossfade.** `./beads_test TestProcessorFreezeSeam` renders each patch with Freeze held from 10 s and a no-Freeze control. The source is a steady 223.7 Hz sine, where a seam is a step in smooth audio.

The detector can see a seam: reading straight through a frozen line once a second flagged exactly 4 clicks, at 0.5, 1.5, 2.5 and 3.5 s (`beads_seam_step.wav`).

Clicks after 10 s, frozen against control:

| Patch | Frozen | Control |
| --- | --- | --- |
| Short grains (Size 0.6), Time across the buffer | 0 | 5 |
| Long grains (Size 0.9) | 0 | 0 |
| Reversed (Size 0.2) | 0 | 2 |
| Pitch +12 | 0 | 0 |
| Pitch -12 | 2 | 2 |
| Real source, short grains | 48 | 66 |
| Delay mode, sine | 2 | 0 |
| Delay mode, real source | 0 | 88 |

Freeze toggled every 1.5 s flagged a run of 9 at 11.58 s, away from any toggle.

Grains can't click here because they never read across the seam: the delay player uses `HEAD_CROSSING_POLICY_STOP` with a 256-sample fade near either end of the buffer. The controls and the toggle run flag the same thing frame by frame: 1 ms frames on a 223 Hz sine, at second-difference levels near -70 dBFS, so those are detector noise.

Delay mode's two: 15.26 s is that same noise. 10.0 s is real but isn't the seam. Stock's `DelayController` deliberately resets every play head when Freeze engages ("Reset all play heads when freezing"), so the output dips about 30 dB for a few milliseconds and fades back. It didn't flag on the real source, and `delay_controller` is unchanged from stock, so it's left alone.
