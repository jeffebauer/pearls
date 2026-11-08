# 05: Envelope sum normalisation

Status: ready-for-agent
Blocked by: 04
Type: task
Spec: `../spec.md`, Spec decision 3, ADR 0004

## What

Replace the per-grain 1/√(n+1) at birth (`beads/dsp/granular_player.h:122`) with a bus gain of g = 1/√(max(Σenv², 1)), computed from the live envelope values.

## Acceptance

- [ ] CPU measured on the debug pin before and after (record both)
- [ ] Gain smoothing is asymmetric: it falls fast when grains pile up and recovers slowly
- [ ] A lone grain's fade is unchanged (the floor of 1)
- [ ] In the measurement tool, the RMS spread across a Density sweep is narrower than stock
- [ ] The check command's "wet and dry within 1 dB at 12 o'clock" assertion still passes; if not, re-tune the wet post-gain in the same way as ticket 04 and record the new value
- [ ] Standard gates, plus Jesse's A/B verdict

## Comments
