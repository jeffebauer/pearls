"""Assertions for `python3 beads/test/measure.py --check`.

Each ticket that changes the DSP adds its check here first, sees it fail, then
makes it pass (see .scratch/soft-character/). A check names the harness tests
it needs rendered and a function that takes {render filename: metrics} (the
dicts built by measure.measure_file) and returns (passed, detail).
"""

from collections import namedtuple

Check = namedtuple("Check", "name renders fn")


def wet_dry_within_1db(m):
    # Gated RMS: the level while grains sound, so gaps between them don't count.
    wet = m["beads_wetdry_wet.wav"]["gated_rms_dbfs"]
    dry = m["beads_wetdry_dry.wav"]["gated_rms_dbfs"]
    return abs(wet - dry) <= 1.0, "wet - dry gated rms %+.1f dB" % (wet - dry)


def wet_not_clipped(m):
    clipped = m["beads_wetdry_wet.wav"]["clipped"]
    return clipped == 0, "%d samples clipped in the fully wet render" % clipped


# Gated RMS spread (loudest minus quietest) across TestProcessorDensitySweep,
# measured before ticket 05 on 8 November 2026: -31.0 dBFS at Density 0.3 to
# -16.4 dBFS at Density 0.0. Spread is in dB, so the wet post-gain doesn't move
# it. The check asks for at least MIN_SPREAD_GAIN_DB narrower, so render noise
# can't pass it.
STOCK_DENSITY_SPREAD_DB = 14.57
MIN_SPREAD_GAIN_DB = 1.0


def density_spread_narrower(m):
    levels = {k: v["gated_rms_dbfs"] for k, v in m.items()
              if k.startswith("beads_density_")}
    if not levels:
        return False, "no density sweep renders"
    loud = max(levels, key=levels.get)
    quiet = min(levels, key=levels.get)
    spread = levels[loud] - levels[quiet]
    return spread <= STOCK_DENSITY_SPREAD_DB - MIN_SPREAD_GAIN_DB, (
        "spread %.1f dB (%s %.1f to %s %.1f), stock %.1f dB, needs %.1f or less"
        % (spread, quiet[len("beads_density_"):-4], levels[quiet],
           loud[len("beads_density_"):-4], levels[loud],
           STOCK_DENSITY_SPREAD_DB,
           STOCK_DENSITY_SPREAD_DB - MIN_SPREAD_GAIN_DB))


CHECKS = [
    # Ticket 01 (fails on stock); ticket 04 should make it pass.
    Check("wet and dry within 1 dB at 12 o'clock",
          ["TestProcessorWetDry"], wet_dry_within_1db),
    # Ticket 04 (fails on stock: 8 samples).
    Check("fully wet render doesn't clip",
          ["TestProcessorWetDry"], wet_not_clipped),
    # Ticket 05 (fails on stock: 14.6 dB).
    Check("wet level spread across a Density sweep narrower than stock",
          ["TestProcessorDensitySweep"], density_spread_narrower),
]
