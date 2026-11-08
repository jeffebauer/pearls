# 12: Density as overlap

Status: ready-for-agent
Blocked by: 11
Type: task
Spec: `../spec.md`, Spec decision 9

## What

When no clock is patched, map Density to average overlap (grains per grain length) instead of grains per second. With a clock patched, behaviour stays stock.

## Acceptance

- [ ] Changing Size at a fixed Density keeps the measured overlap roughly constant
- [ ] Clocked division and probability are unchanged (render with a clock and diff against stock)
- [ ] Standard gates, plus Jesse's A/B verdict

## Comments
