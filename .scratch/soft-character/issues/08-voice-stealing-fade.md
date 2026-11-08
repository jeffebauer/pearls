# 08: Voice stealing fade

Status: done
Blocked by: 07
Type: task
Spec: `../spec.md`, Spec decision 5

## What

Lengthen `kInstantCrossfadeTime` in `beads/dsp/granular_player.h:43` (currently 5 ms). Pick the value from a render sweep at high density.

## Acceptance

- [x] Sweep at least 5, 10, 15 and 20 ms, scored by the click detector, with results tabled in Comments
- [x] Check that the scheduler thresholds that depend on it (comment at `granular_player.h:41`) still hold
- [x] Jesse picks the value by ear
- [x] Standard gates

## Comments

**8 November 2026, sweep.** Each value rendered one test per process into `renders/08-voice-stealing-fade/sweep/<value>/`, with full `metrics.txt`. The 5 ms column is bit-identical to ticket 07 on the shared renders. `kInstantCrossfadeTime` wasn't changed in the tree.

The ticket's existing renders can't judge this: granular player is bit-identical at every value, and granular processor at 10 and 15 ms. Size 0.75 overlaps only about 9 grains in a pool of 32, so nothing is stolen. `TestProcessorVoiceSteal` adds two renders that overflow the pool (Size 0.9 and 0.98 at Density 0.9). Size stops at 0.98 because the processor switches to delay mode above 0.99. A first pass used Size 1.0 by mistake, so it measured delay mode and its results were replaced.

| Fade | Clicks: granular player | processor | wet | steal Size 0.9 | steal Size 0.98 | Difference from 5 ms, wet | steal Size 0.9 | steal Size 0.98 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 5 ms | 359 | 321 | 95 | 91 | 90 | | | |
| 10 ms | 359 | 321 | 97 | 91 | 90 | −66.7 dB | −77.4 dB | −76.5 dB |
| 15 ms | 359 | 321 | 98 | 91 | 88 | −62.9 dB | −72.4 dB | −72.0 dB |
| 20 ms | 359 | 321 | 97 | 91 | 88 | −61.3 dB | −70.0 dB | −70.2 dB |

Signal levels for scale: wet −26.9, steal Size 0.9 −26.1, steal Size 0.98 −25.7 dBFS. Worst case, the change is 34 dB below the signal (wet, 20 ms). High-frequency energy (first difference) moves by 0.08 dB at most.

Findings:

- **The fade isn't the source of the extra clicks, and lengthening it does almost nothing here.** With stealing confirmed running (6.5–18% of samples differ at Size 0.9, 20–43% at Size 0.98), click counts stay within 2 of the 5 ms render.
- **Likely why, from reading `SortGrainList`:** a grain passes through the list's safety slots in turn. Slot 3 gives it the 100 ms pre-emptive fade and slot 2 the grain interval, so it's nearly silent by the time slots 0 and 1 apply the instant fade. Inferred from the code, not instrumented.
- **The 321 isn't the bus gain's instant drop either.** A throwaway build that drops over about 1 ms gives 322 on the granular processor render and 359 on the granular player. The rise from stock's 276 is still unexplained.
- **Monophonic mode is the one path that applies the instant fade directly.** It's unreachable: its combo is behind `ENABLE_ADVANCED_FEATURES`, commented out in `ui.h`.
- **Scheduler ordering holds for every value swept:** 5, 10, 15 and 20 ms are all below 25 ms, which is below 100 ms. The comment at `granular_player.h:41` names `kSimpleSchedulerThreshold` twice; the second should be `kPreemptiveCrossfadeTime`.
- **No check added.** No metric here moves with this constant, so a click check couldn't fail before a change.

Recommendation: keep 5 ms and close this with no DSP change, unless Jesse hears a difference.

Gates: harness builds clean, `measure.py --check` passes 7 of 7, and the wet, dry and drive renders are bit-identical to ticket 07 with the new default argument. Firmware not rebuilt, since no firmware source changed.

**8 November 2026, verdict.** Jesse compared 5, 10, 15 and 20 ms on the wet render and couldn't hear a difference. That render has the largest measured change of the three compared, so `kInstantCrossfadeTime` stays at 5 ms and this closes with no DSP change. The overflowing-pool renders weren't auditioned. The rise in the granular processor's clicks from stock's 276 to 321 stays unexplained.

**8 November 2026, caveat on that verdict.** The wet render has no dry in it, but it barely sounds granular: it correlates at 0.87 with the dry delayed by 404 ms, because nothing is randomised and every grain reads about the same spot. So the listening was on a plain patch. The measured change is tiny on every render, so 5 ms stands.

`TestProcessorCloud` adds a patch that does sound granular (Time randomised per grain, Shape 0.66), with a correlation to the dry of 0.11. Pitch stays fixed, because randomising it made the render seasick and hard to judge. `renders/stock/beads_cloud.wav` comes from stock DSP with today's harness, and that build reproduces `renders/stock/beads_wetdry_wet.wav` bit for bit.

**Finding for later.** The cloud is 5.8 dB quieter than stock (gated RMS −29.5 against −23.7 dBFS) and about 4.0 dB under the dry (gated RMS −25.5 dBFS). The wet gain was set on the fixed patch, whose grains add up nearly in phase; randomised grains don't, so linear normalisation over-attenuates them. ADR 0004 assumed the in-phase case.
