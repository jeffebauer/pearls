# 10: Wet diffuser

Status: ready-for-human
Blocked by: 09
Type: task
Spec: `../spec.md`, Spec decision 7

## What

Port `clouds/dsp/fx/diffuser.h` into `beads/dsp/fx/`, on the wet path before the mix. Its amount runs from 0 at Hann to 1 at fully clockwise on Shape.

## Acceptance

- [x] Delay lines at 48 kHz lengths (L 189/270/404/666, R 227/308/368/608), with `kap` 0.625 and `FxEngine<4096, FORMAT_32_BIT>`
- [x] Read offsets scaled by sample rate at runtime; in the harness, a 32 kHz render matches Clouds' timing
- [ ] RAM checked in the linker map, and CPU measured. RAM done; CPU needs the debug pin
- [x] MIT licence header kept
- [ ] Standard gates, plus Jesse's A/B verdict. Gates pass; verdict pending

## Comments

**8 November 2026, landed on `exp/diffuser`.** Checks in the first commit on the branch, against a pass-through. Port in `18955fe`, equal-power blend in `387f40b`, with its check committed first.

At 32 kHz the port matches `clouds::Diffuser` to 6e-08 (rounding only), compared directly in `TestDiffuser`. At 48 kHz, 98% of an impulse's energy lands after the first millisecond and total energy is 0.99.

Beyond the ticket:

- **Equal-power blend.** Clouds' linear blend measured 2.3 dB down at Shape 0.875, because the allpassed copy is decorrelated from its input. Equal-power measures +0.7 dB. Both ends are unchanged.
- **Off in delay mode.** Shape does something else there. The amount switches while the wet path is faded right down.
- **Off below Hann, and skipped.** Every render at or below Hann is byte-identical to `renders/09-shape-sweep/`, and it costs no CPU there. The lines hold stale audio until it's turned up, at most 14 ms of it, entering at a tiny amount.
- **Follows knob and CV,** not per-grain Shape randomness, since the diffuser is on the bus.
- **`Configure` re-scales the offsets** and clears the lines, so it follows Quality (48, 32 and 24 kHz).

RAM: 16 KB, a member of `Diffuser`, so it lands in DTCM with `processor`. `.data` + `.bss` is 50.3 KB of 128 KB. `.bss` went from 32,672 to 49,136 bytes.

Cloud patch, 30 s each.

| Shape | Render | Gated RMS | Clicks |
| --- | --- | --- | --- |
| 0.75 | Hann, no diffuser | −25.3 | 379 |
| 0.875 | halfway | −24.8 | 177 |
| 1.00 | Gaussian, full diffuser | −25.6 | 2 |
| 1.00 | Gaussian, no diffuser (ticket 09) | −25.6 | 340 |

The drop to 2 clicks is partly the detector: it looks for sudden frame-to-frame jumps, and the diffuser spreads those over about 30 ms. Don't read it as literal click removal.
