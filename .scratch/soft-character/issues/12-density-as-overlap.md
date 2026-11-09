# 12: Density as overlap

Status: wontfix
Blocked by: 11
Type: task
Spec: `../spec.md`, Spec decision 9

## What

When no clock is patched, map Density to average overlap (grains per grain length) instead of grains per second. With a clock patched, behaviour stays stock.

## Acceptance

- [ ] Changing Size at a fixed Density keeps the measured overlap roughly constant
- [ ] Clocked division and probability are unchanged (render with a clock and diff against stock).
- [ ] Standard gates, plus Jesse's A/B verdict

## Comments

Built on `exp/density-overlap`, never committed, and deleted after the verdict. The grain rate scales by how much shorter a grain is than at Size 0.75, so every patch at Size 0.75 renders byte-identically to before and the level gates kept their meaning. The Size it uses is the knob plus CV, without per-grain randomness. `external_input` decides "patched", so a gate with Seed off also stays stock.

Measured overlap across Size 0.6, 0.75 and 0.9: 5.2, 5.2, 5.2 at Density 0.2, and 8.9, 8.9, 8.5 at Density 0.9. It was a 17.8 times swing before. Clocked renders at Size 0.6 are byte-identical to the commit before.

Trade-offs for the A/B: the rate is capped at the stock knob maximum (130.8 Hz) because CPU can't be measured yet, so at Density 0.9 overlap holds from about Size 0.55 up and falls away below. Sparse short grains are gone: Density just past noon at the shortest Size is still about 15 grains a second. At Size 0.98 near noon it's one grain every 20 s or so.

`TestProcessorVoiceSteal` moved to Density 0, the only place overlap now passes the 32-grain pool.

Verdict (9 November 2026): Jesse binned it after the A/B. Density stays grains per second, as stock. No reason recorded yet.
