# Normalise the wet bus by envelope sum

Stock Beads fixes each grain's gain at birth to 1/√(active grains + 1). Loudness therefore depends on spawn history, and survivors never come back up when other grains die. We replace this with a single gain on the summed wet bus, recomputed continuously: g = 1/√(max(Σ env², 1)), where env is each live grain's current envelope value. The gain falls fast when grains pile up and recovers slowly. The floor of 1 keeps a lone grain's fade from being undone, which would otherwise turn every window into a square. Square root rather than linear because Beads' grains are mostly uncorrelated. Pan spread stays count-based, as stock.

## Considered options

- **Clouds' method** (a slewed grain count, 1/√(n−1), plus window-shape compensation): proven, but it only estimates the overlap. The envelope sum measures it, which also compensates soft windows automatically.
- **Keep per-grain gain at birth:** this is the cause of the level jumps.

## Consequences

- It costs one multiply-add per grain per sample. Measure CPU on the debug pin before landing it.
- The wet post-gain is set to 0.75, matching dry, before this lands, so the level is set once.
