# 11: Random grain level

Status: done
Blocked by: 10
Type: task
Spec: `../spec.md`, Spec decision 8

## What

Add a random per-grain level, set at birth, whose depth follows Density's random half.

## Acceptance

- [x] None on the clocked half or at noon, spreading grains over 6 dB at full random
- [x] Make-up gain holds the average grain level, so the cloud keeps its level. Changed from "downward only: no grain is louder than without it" by Jesse's decision below
- [x] Standard gates
- [x] Jesse's A/B verdict

## Comments

**8 November 2026, built on `exp/random-grain-level`, not committed.** Depth is worked out in `TriggerGenerator::Render`, because Density plus its CV only exists there, and travels out on each trigger as `level_spread`: 0 up to 0.52, rising to 1 at fully clockwise. It applies with or without a clock patched, so on a clocked input the right half (probability) gets it too. `ComputeGrainLevel` draws `1 − 0.5 × spread × random` at birth and it goes into the grain's existing gain, so there is no new per-sample work: one random draw and one multiply per grain. `CLAUDE.md` asks for CPU before anything per-grain; Jesse went ahead without it because the backup that would allow it doesn't exist and the cost is this small.

The level stays out of the envelope sum, as Jesse chose: counting it would raise the bus gain and turn neighbouring grains up, which breaks "downward only".

Checks added first: the depth check failed before the change (1 of 19), and now passes (grains reach 0.500 at full depth, 0.752 at half). Density 0 to 0.4 is byte-identical to the commit before, since no random number is drawn at zero depth.

**Three level gates now fail, and are left as they are pending a decision:** wet − dry −1.7 dB (limit ±1), cloud − dry −1.7 dB (limit ±1), quiet render −1.83 dB against its baseline (limit ±0.1). All three render at Density 0.9, about 80% of full depth. That's the cost of keeping the level out of the sum. RMS on the cloud falls 1.6 to 1.9 dB across the four sources. Firmware ends at 0x0805B950, 1553 KB below the settings sector.

Options put to Jesse: accept the drop and re-baseline those three gates, or add make-up gain of 1/(1 − 0.25 × spread).

**8 November 2026, Jesse chose make-up gain.** It's applied per grain at birth, not on the bus: same average, nothing per sample, and it can't step when Density's CV moves. A grain is now `(1 − 0.5 × spread × random) / (1 − 0.25 × spread)`. "Downward only" now means relative to the cloud's average: at full depth grains run from 0.667 to 1.333 of the level without the feature, so the loudest is +2.5 dB. The check on it failed first (mean 0.752 at full depth), and now reads 1.000, 0.995 and 1.002 at no, half and full depth.

Gates: `measure.py --check` 19 of 19. Wet − dry +0.2 dB, cloud − dry +0.2 dB, quiet render +0.08 dB against its baseline (limit ±0.1, so close; it's the random sequence and a re-baseline may come). Hot cloud peak −1.5 dBFS, no clipping. Firmware ends at 0x0805B988, 1553 KB below the settings sector. Density 0 to 0.4 is still byte-identical to the commit before. Cloud RMS on the four sources matches before to within 0.4 dB. Peaks rise 0.4 to 2.3 dB. Renders: `renders/11-random-grain-level/`, with before and after on four sources in `sources/`.

**8 November 2026, verdict.** Jesse kept it after the A/B on four sources. Committed as `63ae170`. CPU isn't on the debug pin yet, but this adds one random draw and a little arithmetic per grain, and nothing per sample.
