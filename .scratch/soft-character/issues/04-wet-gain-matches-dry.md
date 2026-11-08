# 04: Wet gain matches dry

Status: done
Blocked by: 01
Type: task
Spec: `../spec.md`, Spec decision 2

## What

Set the wet post-gain (`post_gain_wet`, 1.5) in `beads/dsp/processor.cc` to the value that brings the check command's dense patch within 1 dB of the dry by gated RMS. Stock reads +4.1 dB, so expect about 0.94 (1.5 × 10^(−4.1/20)). The pre-reverb and output limiters sit after the mix, so confirm by rendering, not by arithmetic.

## Acceptance

- [x] The chosen gain and its measured wet − dry are recorded in Comments
- [x] The check command's "wet and dry within 1 dB at 12 o'clock" assertion goes from failing to passing
- [x] The measurement tool shows the wet level dropping by about 4 dB against stock, with dry unchanged, and no clipping in the wet render (stock clips 8 samples)
- [x] Standard gates, plus Jesse's A/B verdict

## Comments

**8 November 2026, from ticket 01's measurements.** At the check's dense patch, stock wet is +4.1 dB above dry (gated RMS), not 6 dB. A wet gain of 0.75 would land at about −1.9 dB and still fail the 1 dB assertion. A gain of about 0.94 (1.5 × 10^(−4.1/20)) would pass, but the wet level also depends on density, and ticket 05's normalisation will move it again. Before implementing, decide: take the gain from the measurement (changes spec decision 2), or keep 0.75 and move the assertion to after ticket 05.

**8 November 2026, decided.** Jesse chose to take the gain from the measurement rather than fix it at 0.75. Spec decision 2, ADR 0004 and `CLAUDE.md` are updated to match. Ticket 05 re-checks the level after normalisation.

**8 November 2026, implemented.** `post_gain_wet` 1.5 → 0.94 (`beads/dsp/processor.cc:212`), the arithmetic value; the first render passed, so no further tuning. Added a second check, "fully wet render doesn't clip", which failed on stock (8 samples) before the change.

| Render | Stock | 0.94 |
| --- | --- | --- |
| Wet − dry, gated RMS (check patch) | +4.1 dB | +0.2 dB |
| Wet gated RMS | −21.4 dBFS | −25.3 dBFS (−4.1 dB) |
| Dry gated RMS | −25.5 dBFS | −25.5 dBFS (unchanged) |
| Wet peak / clipped | −0.0 dBFS / 8 | −0.4 dBFS / 0 |
| `beads_processor` peak / RMS | −3.2 / −29.3 dBFS | −7.2 / −33.0 dBFS |

Wet peak is still within 0.4 dB of full scale on the dense patch: the soft saturator (ticket 06) and normalisation (ticket 05) should take care of it. Renders and metrics in `renders/04-wet-gain/`. Firmware ends at 0x0805ABB0, unchanged.

Aside: `renders/stock/beads_granular_player.wav` doesn't match a render at `457bc9c` (before this change), so that stock file is stale. It doesn't go through `processor.cc`, so this ticket isn't affected, but re-render stock player and granular-processor before a ticket that changes them. `beads_processor.wav` at `457bc9c` matches stock exactly.

A/B verdict, 8 November 2026: Jesse prefers 0.94 ("B sounds good"), heard on the listening page against stock, LUFS level-matched.
