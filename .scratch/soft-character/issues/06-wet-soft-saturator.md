# 06: Wet soft saturator

Status: ready-for-agent
Blocked by: 05
Type: task
Spec: `../spec.md`, Spec decision 4

## What

Add a gentle tanh-style soft clip on the wet bus, after normalisation and before the mix. The stock limiter before the reverb stays.

## Acceptance

- [ ] Threshold starts around −6 dBFS and is tuned from renders
- [ ] No change below the threshold (within 0.1 dB)
- [ ] Standard gates, plus Jesse's A/B verdict

## Comments
