# 09: Shape sweep

Status: ready-for-agent
Blocked by: 08
Type: task
Spec: `../spec.md`, Spec decision 6, ADR 0003

## What

Remap Shape in `beads/dsp/grain_envelope.h` to one continuous morph: decay, square, Tukey, Hann, Gaussian. Reverse is dropped.

## Acceptance

- [ ] Every envelope parameter is continuous across the whole knob, so consecutive grains never jump
- [ ] The Gaussian is computed cheaply (a lookup table if needed), with CPU measured
- [ ] Windows reach zero at both edges, with no DC step
- [ ] Renders at five Shape positions, with the measurement tool run against stock
- [ ] Standard gates, plus Jesse's A/B verdict

## Comments
