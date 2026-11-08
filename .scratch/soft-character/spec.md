# Spec: soft grain character

Status: ready-for-agent

Settled by grilling on 8 November 2026. Source brief: https://claude.ai/artifact/Nzy7HQb927z7DKiCY4a2Gs. Background research is in the Pearls Obsidian vault, `Research/Web/2026-11-08 — *.md`. Terms are as defined in `GLOSSARY.md`.

## Problem

Compared with the Arbhar, stock Pearls' wet signal is hot, edgy and level-unstable:

- The wet sits 6 dB above the dry at 12 o'clock, so the useful mix lives around 9 o'clock.
- Grain gain is fixed at birth, so loudness depends on spawn history and jumps as density changes.
- Windows have corners, and attacks can be as short as 0.5 ms. Voice stealing fades over 5 ms. Both cause clicks and broadband splatter.
- There is no soft end of Shape. The bell sits about two-thirds of the way round, followed by reverse.
- Every grain plays at the same level, and an unrandomised cloud is panned dead centre.

## Goal

Get a soft, smooth, Arbhar-like wet character without giving up live, always-on processing. The dry stays clear and the reverb stays as stock. Every change is measured against stock and the reference captures, and Jesse judges it by ear.

## Decisions

1. **Measure first.** A render tool reports, per WAV:
   - wet vs dry level (peak and RMS)
   - clicks, by a detector working on 1 ms frames
   - grain attack time, which the harness can log because it sees each grain's envelope
   
   Clip 09 from the Sublustris demos (picked D chords, CC0 Emily guitar) is the reference source for the harness, the Arbhar and the Multigrain. The synthetic `kettel.wav` stays as a regression check. Hardware captures get level and click metrics only.
2. **Wet gain** goes from 1.5 to 0.75, matching dry (0.5625 each at 12 o'clock).
3. **Envelope-sum normalisation** replaces per-grain 1/√n at birth (ADR 0004):
   - a bus gain g = 1/√(max(Σenv², 1)), falling fast and recovering slowly
   - pan spread stays count-based
   - CPU is measured on the debug pin before it lands
4. **Soft saturator on the wet bus**, tanh-style, starting around −6 dBFS, placed after normalisation. The threshold is tuned from renders, and the stock limiter before the reverb stays.
5. **Grain edges.** The minimum attack (now 0.5 ms) and the voice-stealing fade (now 5 ms) both go up. Each value is chosen from a sweep of renders scored by the click detector and Jesse's ear, with no value fixed in advance.
6. **Shape** becomes one continuous morph, anticlockwise to clockwise: decay, square, Tukey, Hann, Gaussian. Reverse is dropped (ADR 0003). The morph must be continuous in every parameter, so moving the knob or its CV never jumps between consecutive grains.
7. **Diffuser.** Clouds' 4+4 allpass diffuser goes on the wet path, before the mix. Its amount runs from 0 at Hann to 1 at fully clockwise.
   - Delay lengths ×1.5 for 48 kHz: L 189/270/404/666, R 227/308/368/608, with `kap` 0.625 and `FxEngine<4096, FORMAT_32_BIT>`.
   - Read offsets are scaled by sample rate at runtime, so 32 kHz Vintage Digital matches stock Clouds and 24 kHz is covered too.
8. **Random per-grain level**, set at birth. Its depth follows the random half of Density, growing alongside random timing. It is downward only, reaching −6 dB at full random. The clocked half has none.
9. **Density as overlap.** When no clock is patched, Density sets the average overlap (grains per grain length) instead of grains per second, so changing Size keeps the cloud equally dense. With a clock patched, division and probability stay stock.
10. **Stereo width**, one change:
    - a ±30% minimum pan spread on the wet path
    - Multigrain-style opposite-polarity random level on L and R (small, set at birth)
    
    Mono mode stays mono.
11. **Freeze seam.** Render frozen audio and run the click detector across the loop point. Add a crossfade only if it finds a click.
12. **Pitch quantiser**, as described in `CLAUDE.md`, comes last. It gets its own short grilling (LED display of the mask, a modulation-only mode) before implementation.

## Order and gates

Implement in ticket order, one change per commit, each on `master` unless it's exploratory (then `exp/<what>`). Every DSP ticket finishes with:

- the harness building clean
- `./firmware.sh` passing, with the image end address recorded
- renders in `renders/<NN>-<slug>/`, with `levels.py` and the measurement tool run against stock and the reference captures
- Jesse's A/B verdict recorded in the ticket's Comments

Nothing is flashed until a backup of the settings sector exists (ADR 0002).

## Out of scope

The parked ideas are in `.scratch/backlog/parked.md`: latency work, live position mapping, grain-only reverb, the wet low-pass, the per-grain tone filter, and the correlated-sum blend.

## Risks

- **CPU.** Envelope-sum normalisation, the Gaussian window and the diffuser all add per-sample work. The Gaussian likely wants a lookup table. Measure on the debug pin before and after each.
- **RAM.** The diffuser adds 16 KB. Check the linker map.
- **Feel.** Density as overlap and the dropped reverse change familiar controls. Jesse's A/B decides whether they stay.
