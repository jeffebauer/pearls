# 08: Voice stealing fade

Status: ready-for-agent
Blocked by: 07
Type: task
Spec: `../spec.md`, Spec decision 5

## What

Lengthen `kInstantCrossfadeTime` in `beads/dsp/granular_player.h:43` (currently 5 ms). Pick the value from a render sweep at high density.

## Acceptance

- [ ] Sweep at least 5, 10, 15 and 20 ms, scored by the click detector, with results tabled in Comments
- [ ] Check that the scheduler thresholds that depend on it (comment at `granular_player.h:41`) still hold
- [ ] Jesse picks the value by ear
- [ ] Standard gates

## Comments
