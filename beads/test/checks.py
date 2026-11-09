"""Assertions for `python3 beads/test/measure.py --check`.

Each ticket that changes the DSP adds its check here first, sees it fail, then
makes it pass (see .scratch/soft-character/). A check names the harness tests
it needs rendered and a function that takes {render filename: metrics} (the
dicts built by measure.measure_file) and returns (passed, detail).
"""

import math
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


def long_grain_curves(m):
    """{Shape position: window of a 1 s grain, 129 points across it}."""
    rows = m["beads_envelope_shape.wav"].get("envelope_curves", [])
    return {r["shape"]: r["curve"] for r in rows if r["duration"] == 48000.0}


def asymmetry(c):
    return max(abs(a - b) for a, b in zip(c, reversed(c)))


def flat_fraction(c):
    return sum(1 for v in c if v >= 0.99) / len(c)


def hann_error(c):
    n = len(c) - 1
    return max(abs(v - 0.5 * (1.0 - math.cos(2.0 * math.pi * i / n)))
               for i, v in enumerate(c))


# What the knob should land on at each quarter. The Gaussian's width isn't
# pinned: it has to be symmetric, peak in the middle and be narrower than Hann
# (whose mean is 0.5).
def shape_landmarks(m):
    curves = long_grain_curves(m)
    if len(curves) != 65:
        return False, "expected 65 Shape positions, got %d" % len(curves)
    wrong = []
    decay = curves[0.0]
    if decay.index(max(decay)) > 6 or sum(decay) / len(decay) > 0.35:
        wrong.append("0.00 isn't a decay")
    if flat_fraction(curves[0.25]) < 0.9:
        wrong.append("0.25 isn't a square")
    tukey = curves[0.5]
    if not 0.4 <= flat_fraction(tukey) <= 0.6 or asymmetry(tukey) > 0.02:
        wrong.append("0.50 isn't a Tukey (flat %.2f, asymmetry %.2f)"
                     % (flat_fraction(tukey), asymmetry(tukey)))
    if hann_error(curves[0.75]) > 0.03:
        wrong.append("0.75 is %.2f from a Hann" % hann_error(curves[0.75]))
    gauss = curves[1.0]
    mean = sum(gauss) / len(gauss)
    if (asymmetry(gauss) > 0.02 or gauss.index(max(gauss)) != len(gauss) // 2
            or not 0.3 <= mean <= 0.45):
        wrong.append("1.00 isn't a Gaussian (asymmetry %.2f, mean %.2f)"
                     % (asymmetry(gauss), mean))
    return not wrong, "; ".join(wrong) or "decay, square, Tukey, Hann, Gaussian"


# The limit is the mean change in the window between neighbouring positions,
# 1/64 of the knob apart. The old layout fails it (0.085), at the fast end of
# its bell-to-reverse half, and the morph measures 0.050, so the limit sits
# between them. A jump between consecutive grains would show here.
MAX_SHAPE_STEP = 0.065
MAX_EDGE_VALUE = 0.01


def shape_continuous_with_zero_edges(m):
    rows = m["beads_envelope_shape.wav"].get("envelope_curves", [])
    if not rows:
        return False, "no envelope curves logged"
    curves = long_grain_curves(m)
    positions = sorted(curves)
    steps = [sum(abs(a - b) for a, b in zip(curves[p], curves[q])) / len(curves[p])
             for p, q in zip(positions, positions[1:])]
    edge = max(max(abs(r["first"]), abs(r["last"])) for r in rows)
    return max(steps) <= MAX_SHAPE_STEP and edge <= MAX_EDGE_VALUE, (
        "largest step between positions %.3f (limit %.2f), edge value %.4f (limit %.2f)"
        % (max(steps), MAX_SHAPE_STEP, edge, MAX_EDGE_VALUE))


# Ticket 10: the level halfway up the diffuser against the mean of Hann (no
# diffuser) and Gaussian (full diffuser). A linear blend measures 2.3 dB down.
MAX_DIFFUSER_DIP_DB = 1.0


def diffuser_level_flat(m):
    hann = m["beads_shape_075.wav"]["gated_rms_dbfs"]
    half = m["beads_shape_088.wav"]["gated_rms_dbfs"]
    full = m["beads_shape_100.wav"]["gated_rms_dbfs"]
    diff = half - 0.5 * (hann + full)
    return abs(diff) <= MAX_DIFFUSER_DIP_DB, (
        "halfway %+.1f dB against Hann and Gaussian (limit %.1f)"
        % (diff, MAX_DIFFUSER_DIP_DB))


def diffuser_values(m):
    return m["beads_diffuser.wav"].get("values", {})


# Ticket 10: the diffuser takes the same time in seconds at any sample rate, so
# in samples 32 kHz is 2/3 of 48 kHz. It started as a sample-exact port of
# Clouds; kap and the modulation were then changed by ear (see the ticket).
TIMING_RATIO = 2.0 / 3.0
MAX_TIMING_ERROR = 0.03


def diffuser_timing_scales(m):
    v = diffuser_values(m)
    if not v:
        return False, "no diffuser values logged"
    ratio, bypass = v["timing_ratio_32k_48k"], v["bypass_max_diff"]
    return abs(ratio - TIMING_RATIO) <= MAX_TIMING_ERROR and bypass == 0.0, (
        "32 kHz timing is %.3f of 48 kHz in samples (wants %.3f), "
        "at amount 0 differs by %.2e (must be 0)" % (ratio, TIMING_RATIO, bypass))


# Ticket 10: fixed delays ring metallic. The delays must move, so the same
# impulse a moment later comes out different.
MIN_IMPULSE_MOVED = 0.02
MAX_RING_30DB_MS = 113.0


def diffuser_not_metallic(m):
    v = diffuser_values(m)
    if not v:
        return False, "no diffuser values logged"
    moved, ring = v["impulse_moved"], v["ring_30db_ms"]
    return moved >= MIN_IMPULSE_MOVED and ring <= MAX_RING_30DB_MS, (
        "impulse moved by %.3f over 1.25 s (needs %.2f or more), "
        "ring to -30 dB %.1f ms (limit %.1f)"
        % (moved, MIN_IMPULSE_MOVED, ring, MAX_RING_30DB_MS))


def diffuser_spreads_energy(m):
    v = diffuser_values(m)
    if not v:
        return False, "no diffuser values logged"
    smear, energy = v["smear_fraction"], v["impulse_energy"]
    return smear >= 0.5 and 0.9 <= energy <= 1.1, (
        "%.0f%% of an impulse's energy after the first millisecond (needs 50%%), "
        "total energy %.2f (needs 0.9 to 1.1)" % (100.0 * smear, energy))


def diffuser_amount_follows_shape(m):
    v = diffuser_values(m)
    if not v:
        return False, "no diffuser values logged"
    want = {"amount_shape_050": 0.0, "amount_shape_075": 0.0,
            "amount_shape_088": 0.5, "amount_shape_100": 1.0,
            "amount_shape_cv": 1.0, "amount_delay_mode": 0.0}
    wrong = ["%s %.3f, wants %.1f" % (k, v[k], w) for k, w in sorted(want.items())
             if abs(v[k] - w) > 0.01]
    return not wrong, "; ".join(wrong) or (
        "off up to Hann, full at fully clockwise, follows CV, off in delay mode")


# Ticket 11: grain levels are relative to a grain with no random level, so 1.0
# is "as loud as without it". Full depth spreads the grains over 6 dB, quietest
# to loudest, and make-up gain keeps their average at 1 so the cloud holds its
# level. That puts the loudest grain at 1/0.75 at full depth, the most it can be.
GRAIN_LEVEL_TOLERANCE = 0.0005
MAX_GRAIN_LEVEL = 1.0 / 0.75
MAX_GRAIN_MEAN_ERROR = 0.03


def grain_level_values(m):
    return m["beads_grain_level.wav"].get("values", {})


def grain_level_reaches_6db(m):
    v = grain_level_values(m)
    if not v:
        return False, "no grain levels logged"
    wrong = []
    full = v["level_full_min"] / v["level_full_max"]
    half = v["level_half_min"] / v["level_half_max"]
    if not 0.49 <= full <= 0.52:
        wrong.append("full depth spans %.3f, wants about 0.5" % full)
    if not 0.74 <= half <= 0.77:
        wrong.append("half depth spans %.3f, wants about 0.75" % half)
    want = {"spread_d076": 0.5, "spread_d100": 1.0, "spread_cv": 1.0}
    wrong += ["%s %.3f, wants %.1f" % (k, v[k], w) for k, w in sorted(want.items())
              if abs(v[k] - w) > 0.01]
    return not wrong, "; ".join(wrong) or (
        "quietest grain is %.3f of the loudest at full depth, %.3f at half, "
        "depth follows the random half and its CV" % (full, half))


def grain_level_average_held(m):
    v = grain_level_values(m)
    if not v:
        return False, "no grain levels logged"
    means = [v["level_none_mean"], v["level_half_mean"], v["level_full_mean"]]
    loudest = max(v["level_none_max"], v["level_half_max"], v["level_full_max"])
    worst = max(abs(x - 1.0) for x in means)
    return (worst <= MAX_GRAIN_MEAN_ERROR
            and loudest <= MAX_GRAIN_LEVEL + GRAIN_LEVEL_TOLERANCE), (
        "mean grain level %.3f, %.3f, %.3f at no, half and full depth "
        "(limit 1 +/- %.2f), loudest grain %.3f (limit %.3f)"
        % (means[0], means[1], means[2], MAX_GRAIN_MEAN_ERROR, loudest,
           MAX_GRAIN_LEVEL))


def grain_level_none_when_regular(m):
    v = grain_level_values(m)
    if not v:
        return False, "no grain levels logged"
    keys = ["spread_d000", "spread_d040", "spread_noon", "spread_cv_down",
            "spread_clock_d040", "spread_clock_noon"]
    wrong = ["%s %.3f" % (k, v[k]) for k in keys if v[k] != 0.0]
    if abs(v["level_none_min"] - 1.0) > GRAIN_LEVEL_TOLERANCE:
        wrong.append("grains vary to %.4f with no depth" % v["level_none_min"])
    return not wrong, "; ".join(wrong) or (
        "no depth on the regular half, at noon or below it under CV")


# Ticket 13: both inputs carry the same signal and nothing is randomised, so
# without a pan floor the output is exactly mono (correlation 1, no side).
MIN_CLOCKED_SIDE_DB = -30.0


def clocked_cloud_is_wide(m):
    w = m["beads_width_clocked.wav"]
    return w["side_mid_db"] >= MIN_CLOCKED_SIDE_DB and w["lr_correlation"] < 1.0, (
        "side - mid %.1f dB (needs %.1f or more), L/R correlation %.4f"
        % (w["side_mid_db"], MIN_CLOCKED_SIDE_DB, w["lr_correlation"]))


def mono_stays_mono(m):
    diff = m["beads_width_mono.wav"]["lr_max_diff"]
    return diff == 0.0, "largest L - R with a mono input %.6f" % diff


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
    # Ticket 09 (fails before the morph: the old layout's windows).
    Check("Shape lands on decay, square, Tukey, Hann, Gaussian",
          ["TestGrainEnvelopeShape"], shape_landmarks),
    # Ticket 09 (fails before the morph: 0.085 between neighbouring positions).
    Check("Shape morph continuous, windows reach zero at both edges",
          ["TestGrainEnvelopeShape"], shape_continuous_with_zero_edges),
    # Ticket 10 (these three fail on the pass-through diffuser).
    Check("diffuser timing scales with sample rate, bypassed at amount 0",
          ["TestDiffuser"], diffuser_timing_scales),
    # Ticket 10 (fails on Clouds' fixed lines at kap 0.625).
    Check("diffuser's delays move and its ring is shorter",
          ["TestDiffuser"], diffuser_not_metallic),
    Check("diffuser spreads an impulse without changing its energy",
          ["TestDiffuser"], diffuser_spreads_energy),
    Check("diffuser amount follows Shape from Hann up, off in delay mode",
          ["TestDiffuser"], diffuser_amount_follows_shape),
    # Ticket 10 (fails with Clouds' linear blend: 2.3 dB down).
    Check("level stays flat halfway up the diffuser",
          ["TestProcessorShapeSweep"], diffuser_level_flat),
    # Ticket 11 (fails before the change: every grain at full level, depth 0).
    Check("random grain level spans 6 dB and follows the random half",
          ["TestGrainLevel"], grain_level_reaches_6db),
    # Ticket 11 (fails before make-up gain: mean 0.75 at full depth).
    Check("make-up gain holds the average grain level",
          ["TestGrainLevel"], grain_level_average_held),
    # Ticket 11 guard: passes before the change by design, so it leans on the
    # first check to prove the level is really being applied.
    Check("no random grain level on the regular half or at noon",
          ["TestGrainLevel"], grain_level_none_when_regular),
    # Ticket 13 (fails before the change: exactly mono).
    Check("unrandomised clocked cloud is wide on a mono-summed source",
          ["TestProcessorWidth"], clocked_cloud_is_wide),
    # Ticket 13 guard: passes before the change by design, so it leans on the
    # check above to prove the width is really being applied.
    Check("mono input stays mono with nothing randomised",
          ["TestProcessorWidth"], mono_stays_mono),
]
