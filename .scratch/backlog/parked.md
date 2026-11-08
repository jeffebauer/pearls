# Parked ideas

Status: needs-triage

These were left out of the soft-character plan during grilling on 8 November 2026. Each is a candidate for later, not a commitment.

| Idea | Why parked | Revisit when |
| --- | --- | --- |
| Latency work: per-grain minimum safe offset, Time knob remapped so its lowest setting is "as live as possible", a smaller safety zone | A separate goal from softness, and stock already trims grain start to the minimum safe offset | The soft-character plan is done and live use feels late |
| Live position mapping (from Multigrain: start fully clockwise tracks the record head) | Part of the latency work | Together with the latency work |
| Grain-only reverb (old change 7): move the reverb before the dry/wet mix | The brief keeps the reverb as it is, and turning the reverb down already keeps the dry clear | Reverberated dry becomes a problem in use |
| Wet low-pass tone filter | Softer windows and the diffuser target the same harshness, and Hermite interpolation is already good | After the diffuser lands, if the wet still sounds bright |
| Per-grain tone filter (from Multigrain: a 2-pole LP/HP per grain) | Expensive on CPU, and overlaps the wet low-pass | Together with the wet low-pass, after measuring CPU headroom |
| Correlated-sum blend for normalisation (exponent moves toward 1 when grains are near-identical) | Low priority refinement of the envelope-sum normalisation | Frozen, unrandomised clouds measure louder than random ones |

## Comments
