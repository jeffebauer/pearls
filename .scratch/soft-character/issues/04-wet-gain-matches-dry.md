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

**8 November 2026, from ticket 01's measurements.** At the check's dense patch, stock wet is +4.1 dB above dry (gated RMS), not 6 dB. A wet gain of 0.75 would land at about −1.9 dB and still fail the 1 dB assertion. A gain of about 0.94 (1.5 × 10^(−4.1/20)) would pass, but the wet level also depends on density, and ticket 05's normalisation will move it again. Before implementing, decide: take the gain from the measurement (changes spec decision 2), or keep 0.75 and move the assertion to after ticket 05.

