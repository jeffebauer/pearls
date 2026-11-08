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


# TestProcessorWetDrive's quiet pass (source 12 dB down, wet peak -15 dBFS). It
# sits below the wet saturator's threshold, so the saturator mustn't move it.
# Re-baselined at ticket 07 from -37.50: the 3 ms grain attack floor adds
# 0.15 dB of its own, with the saturator nowhere near engaged.
STOCK_QUIET_GATED_RMS_DBFS = -37.35
QUIET_TOLERANCE_DB = 0.1


def quiet_wet_unchanged(m):
    level = m["beads_drive_quiet.wav"]["gated_rms_dbfs"]
    diff = level - STOCK_QUIET_GATED_RMS_DBFS
    return abs(diff) <= QUIET_TOLERANCE_DB, (
        "gated rms %.2f dBFS, %+.2f dB against the %.2f baseline"
        % (level, diff, STOCK_QUIET_GATED_RMS_DBFS))


def hot_wet_not_clipped(m):
    clipped = m["beads_drive_hot.wav"]["clipped"]
    return clipped == 0, (
        "%d samples clipped with the source 6 dB up, peak %.1f dBFS"
        % (clipped, m["beads_drive_hot.wav"]["peak_dbfs"]))


# Shortest 10-90% grain rise in the fully wet render. It's 0.52 ms with the
# stock 0.5 ms floor at this patch's Shape, and 1.04 ms with a 1 ms floor.
MIN_ATTACK_RISE_MS = 1.0


def min_attack_raised(m):
    attacks = m["beads_wetdry_wet.wav"].get("attacks_ms")
    if not attacks:
        return False, "no grain attacks logged"
    shortest = min(attacks)
    return shortest >= MIN_ATTACK_RISE_MS, (
        "shortest grain rise %.2f ms, needs %.2f or more"
        % (shortest, MIN_ATTACK_RISE_MS))


# TestGranularPlayer on stock (renders/stock/metrics.txt). Shape 0.33 and short
# grains put it on the attack floor, and ticket 05 raised it to 412.
STOCK_PLAYER_CLICKS = 395


def player_clicks_back_to_stock(m):
    n = len(m["beads_granular_player.wav"]["clicks"])
    return n <= STOCK_PLAYER_CLICKS, (
        "%d clicks in the granular player render, stock %d"
        % (n, STOCK_PLAYER_CLICKS))


def cloud_within_1db_of_dry(m):
    # A patch with Time randomised, whose grains don't add up in phase.
    cloud = m["beads_cloud.wav"]["gated_rms_dbfs"]
    dry = m["beads_wetdry_dry.wav"]["gated_rms_dbfs"]
    return abs(cloud - dry) <= 1.0, (
        "cloud - dry gated rms %+.1f dB" % (cloud - dry))


def cloud_hot_not_clipped(m):
    clipped = m["beads_cloud_hot.wav"]["clipped"]
    return clipped == 0, (
        "%d samples clipped in the hot cloud, peak %.1f dBFS"
        % (clipped, m["beads_cloud_hot.wav"]["peak_dbfs"]))


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
    # Ticket 06 guard: passes before and after the saturator, by design.
    Check("wet below the saturator threshold unchanged within 0.1 dB",
          ["TestProcessorWetDrive"], quiet_wet_unchanged),
    # Ticket 06 (fails before the saturator: 7 samples).
    Check("fully wet render with the source 6 dB up doesn't clip",
          ["TestProcessorWetDrive"], hot_wet_not_clipped),
    # Ticket 07 (fails before the change: 0.52 ms).
    Check("shortest grain rise at least 1 ms",
          ["TestProcessorWetDry"], min_attack_raised),
    # Ticket 07 (fails before the change: 412 clicks).
    Check("granular player clicks no worse than stock",
          ["TestGranularPlayer"], player_clicks_back_to_stock),
    # Ticket 16 (fails before the change: -4.0 dB).
    Check("cloud with Time randomised within 1 dB of the dry",
          ["TestProcessorCloud", "TestProcessorWetDry"],
          cloud_within_1db_of_dry),
    # Ticket 16 guard: the louder cloud mustn't clip with the source 6 dB up.
    Check("cloud with the source 6 dB up doesn't clip",
          ["TestProcessorCloud"], cloud_hot_not_clipped),
]
