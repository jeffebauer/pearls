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


CHECKS = [
    # Ticket 01 (fails on stock); ticket 04 should make it pass.
    Check("wet and dry within 1 dB at 12 o'clock",
          ["TestProcessorWetDry"], wet_dry_within_1db),
]
