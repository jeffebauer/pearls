# 07: Minimum grain attack

Status: ready-for-agent
Blocked by: 05
Type: task
Spec: `../spec.md`, Spec decision 5

## What

Raise `min_attack_` in `beads/dsp/grain_envelope.h` (currently 0.5 ms). Pick the value from a render sweep.

## Acceptance

- [ ] Sweep at least 0.5, 1, 2, 3 and 5 ms, scored by the click detector and attack-time metric, with results tabled in Comments
- [ ] Jesse picks the value by ear from the sweep
- [ ] Standard gates

## Comments
