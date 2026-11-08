# 04: Wet gain matches dry

Status: ready-for-agent
Blocked by: 01
Type: task
Spec: `../spec.md`, Spec decision 2

## What

Set the wet post-gain from 1.5 to 0.75 in `beads/dsp/processor.cc`.

## Acceptance

- [ ] At 12 o'clock, wet and dry gains are equal (0.5625 each)
- [ ] The check command's "wet and dry within 1 dB at 12 o'clock" assertion goes from failing to passing
- [ ] The measurement tool shows the wet level dropping by about 6 dB against stock, with dry unchanged
- [ ] Standard gates, plus Jesse's A/B verdict

## Comments
