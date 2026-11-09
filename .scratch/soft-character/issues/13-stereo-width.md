# 13: Stereo width

Status: done
Blocked by: 11 (12 was binned)
Type: task
Spec: `../spec.md`, Spec decision 10

## What

Add a ±30% minimum pan spread on the wet path, plus a small opposite-polarity random level on L and R, both set at birth.

## Acceptance

- [x] An unrandomised, clocked cloud is measurably wide (L/R correlation below stock). True on a near-mono source, which is what the check renders. Sources that are already wide measure narrower, as below
- [x] Mono mode stays mono
- [x] Standard gates, plus Jesse's A/B verdict

## Comments

The pan floor lifts each grain's pan range to at least ±30% (`kMinPanSpread`, pan 0.35 to 0.65) on the delay player's stereo path, and each grain gets an opposite L/R level up to about ±1 dB (`kGrainLrLevelMax` 0.12). A mono input skips both, random draws included, and renders byte-identically to before. The wavetable player stays stock.

Measured on the clocked patch with nothing randomised (L/R correlation, before to after): groove 0.999 to 0.991, guitar 0.936 to 0.955, kettel 0.209 to 0.444, synth -0.082 to 0.184. Only the near-mono source gets wider. The stock pan law feeds the opposite channel across when it pans, so sources that are already wide get narrower on this patch. On the randomised cloud correlation moved by 0.03 or less, because stock's pan spread already exceeds the floor there. L/R level depth barely shows in these numbers.

The clocked patch sounds like the source delayed (best match to the dry guitar 0.77), so it's for measuring, not listening. The A/B uses `TestProcessorGrains`: 54 ms grains at about 25 a second, Time randomised (best match 0.09 random timing, -0.08 regular). L/R correlation, before to after at ±1 dB, random then regular timing: groove 0.888 to 0.821 and 0.987 to 0.939, guitar 0.839 to 0.813 and 0.933 to 0.910, kettel 0.387 to 0.431 and 0.276 to 0.401, synth 0.192 to 0.250 and 0.030 to 0.180. Gated level rises up to 2.1 dB on the near-mono groove (regular timing) and 1.1 dB on guitar, with no clipping.

The fully wet fixed patch's peak rose from -4.6 to -3.3 dBFS, with no clipping. CPU is unmeasured: one extra random draw and four multiplies per grain birth, nothing per sample.

Gates: `measure.py --check` 21 of 21. Wet − dry +0.3 dB, cloud − dry +0.3 dB, quiet render −0.03 dB against its baseline. Hot cloud peak −1.6 dBFS, no clipping. Firmware ends at 0x0805BBC0, 1553 KB below the settings sector. Renders: `renders/13-stereo-width/`, with before and three L/R depths on four sources in `sweep/`.

**9 November 2026, verdict.** Jesse kept ±1 dB after the A/B. Committed as `cee1b1c`. The level-only pan floor (no crossfeed) wasn't built. CPU isn't on the debug pin yet.
