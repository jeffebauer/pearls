# 07: Minimum grain attack

Status: done
Blocked by: 05 (its code landed in `bcf5e36`; only its CPU measurement is open, and that doesn't block this)
Type: task
Spec: `../spec.md`, Spec decision 5

## What

Raise `min_attack_` in `beads/dsp/grain_envelope.h` (currently 0.5 ms). Pick the value from a render sweep.

## Acceptance

- [x] Sweep at least 0.5, 1, 2, 3 and 5 ms, scored by the click detector and attack-time metric, with results tabled in Comments
- [x] Jesse picks the value by ear from the sweep (3 ms)
- [x] Standard gates

## Comments

**8 November 2026, sweep.** Each value rendered one test per process into `renders/07-min-attack/sweep/<value>/`, with full `metrics.txt`. The 0.5 ms column is bit-identical to ticket 06.

| `min_attack_` | Shortest rise, wet (10–90%) | Clicks: granular player | processor | wet | Wet peak (dBFS) | Quiet render gated RMS (dBFS) | Quiet peak (dBFS) |
| --- | --- | --- | --- | --- | --- | --- | --- |
| stock | 0.52 ms | 395 | 135 | 98 | 0.0 (clipped) | | |
| 0.5 ms | 0.52 ms | 412 | 136 | 97 | −2.8 | −37.50 | −16.0 |
| 1 ms | 1.04 ms | 393 | 134 | 96 | −2.8 | −37.47 | −15.9 |
| 2 ms | 2.08 ms | 389 | 131 | 93 | −2.6 | −37.41 | −15.8 |
| 3 ms | 3.12 ms | 359 | 130 | 95 | −2.5 | −37.35 | −15.1 |
| 5 ms | 5.20 ms | 325 | 127 | 99 | −2.4 | −37.24 | −14.5 |

Findings:

- **The granular processor render doesn't change at any value: 321 clicks, bit-identical.** Its grains are long at Shape 0.5, so `duration * attack` is already above 5 ms and the floor never applies. So the rise from 276 to 321 at ticket 05 isn't an attack-floor problem, and this ticket can't fix it. Ticket 08 (voice-stealing fade) and the bus gain's instant drop are the next suspects.
- **The floor only applies on the decay/square half of Shape.** At Shape ≤ 1/3 the attack share is 0, so the floor sets the attack outright; above that it matters only for short grains.
- **The wet render's clicks are mostly the source.** The dry render has 85, so 93–99 is near the detector's noise, and that's why the check uses the granular player instead.
- **The floor is multiplied by `1 + 3 × sustain`,** so at Shape 0 the attack is 4× the value: 20 ms for a 5 ms floor. No render here covers Shape 0 with short grains.
- **The quiet guard passes up to 2 ms (+0.09 dB) and trips at 3 ms (+0.15 dB).** Longer attacks raise the quiet level slightly and move the peaks by up to 1.5 dB. If the chosen value trips it, re-baseline the constant.

Checks added to `beads/test/checks.py`: shortest grain rise at least 1 ms, and granular player clicks no worse than stock. Both fail on current code (0.52 ms, 412 clicks).

**8 November 2026, verdict.** Jesse chose 3 ms. Landed in `7a18a0f`, with the checks in `dbe79e6`. The quiet guard tripped as predicted, and its baseline moved from −37.50 to −37.35 dBFS: the only difference between those renders is the attack floor, and at −15 dBFS peak the saturator isn't engaged.

Gates: harness builds clean. Firmware image ends at 0x0805AF30, 1556 KB below the settings sector. `measure.py --check` passes 7 of 7. `levels.py renders/07-min-attack/*.wav` shows no clipping (`beads_processor` peak −5.2 / RMS −32.1 dBFS, wet peak −2.5 / RMS −27.1, dry −5.6 / −27.1). The ticket renders are bit-identical to `sweep/a3`.
