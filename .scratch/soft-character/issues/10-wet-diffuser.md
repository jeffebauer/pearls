# 10: Wet diffuser

Status: ready-for-agent
Blocked by: 09
Type: task
Spec: `../spec.md`, Spec decision 7

## What

Port `clouds/dsp/fx/diffuser.h` into `beads/dsp/fx/`, on the wet path before the mix. Its amount runs from 0 at Hann to 1 at fully clockwise on Shape.

## Acceptance

- [ ] Delay lines at 48 kHz lengths (L 189/270/404/666, R 227/308/368/608), with `kap` 0.625 and `FxEngine<4096, FORMAT_32_BIT>`
- [ ] Read offsets scaled by sample rate at runtime; in the harness, a 32 kHz render matches Clouds' timing
- [ ] RAM checked in the linker map, and CPU measured
- [ ] MIT licence header kept
- [ ] Standard gates, plus Jesse's A/B verdict

## Comments
