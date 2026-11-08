# 14: Freeze seam

Status: ready-for-agent
Blocked by: 01
Type: task
Spec: `../spec.md`, Spec decision 11

## What

Render frozen audio across the loop point and run the click detector. If it finds a click, add a crossfade at the seam (`beads/dsp/delay_line.h`, near `freeze_tail_size`). If not, record the result and close.

## Acceptance

- [ ] Measurement result recorded in Comments
- [ ] If a crossfade is built: clicks at the seam are gone, with standard gates plus Jesse's A/B verdict

## Comments
