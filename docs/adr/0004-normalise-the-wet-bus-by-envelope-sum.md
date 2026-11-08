# Normalise the wet bus by envelope sum

Stock Beads fixes each grain's gain at birth to 1/√(active grains + 1). Loudness therefore depends on spawn history, and survivors never come back up when other grains die. We replace this with a single gain on the summed wet bus, recomputed continuously: g = 1/max(Σ env, 1), where env is each live grain's current envelope value. The gain drops at once when grains pile up and recovers slowly (100 ms). The floor of 1 keeps a lone grain's fade from being undone, which would otherwise turn every window into a square. Linear rather than square root: with Time unmodulated, neighbouring grains read near-identical audio and add up nearly in phase. Pan spread stays count-based, as stock.

**Amended 8 November 2026.** This first said g = 1/√(max(Σ env², 1)), on the assumption that Beads' grains are mostly uncorrelated. Ticket 05 measured otherwise: on the default patch (Time fixed), √ made the wet level climb 10.9 dB across Density 0.6–1.0 (stock 7.5) and clip at Density 0.9, while linear held it to 3.2 dB with no clipping and wet within 0.1 dB of dry at the existing post-gain. A sweep of power means (Σ env^q)^(1/q) from q = 1 to 2 had q = 1 best in the worst case across a fixed-Time and a random-Time patch. Linear is also cheaper: no squares or square root. Jesse chose it. Its known weak spot: with Time randomised, the densest setting (Density 1.0) is about 5 dB quieter than 0.6–0.9. The sweep is in ticket 05's Comments.

## Considered options

- **Clouds' method** (a slewed grain count, 1/√(n−1), plus window-shape compensation): proven, but it only estimates the overlap. The envelope sum measures it, which also compensates soft windows automatically.
- **√ of the sum of squares** (the first version of this decision) and power means between it and linear: measured in ticket 05, flatter only when grain positions are randomised.
- **Keep per-grain gain at birth:** this is the cause of the level jumps.

## Consequences

- It costs one add per grain and one divide per sample. Measure CPU on the debug pin before landing it.
- The wet post-gain is set from measurement before this lands (ticket 04), then re-checked after it with the same check command, since the envelope sum changes the wet level. (Originally "set to 0.75, so the level is set once"; changed 8 November 2026 when ticket 01 measured the stock wet at +4.1 dB, not 6.)
