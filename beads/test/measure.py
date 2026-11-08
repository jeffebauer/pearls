#!/usr/bin/env python3
"""Measure renders and hardware captures: levels, clicks and grain attacks.

Usage:
  python3 beads/test/measure.py FILE.wav [FILE.wav ...]
      Level and click metrics for every file. Works on any PCM or float WAV,
      so hardware captures too. For a pair named X_wet.wav / X_dry.wav it also
      prints the wet minus dry difference, and for X_wet_attacks.csv (written
      by the harness beside the wet render) the grain attack times.

  python3 beads/test/measure.py --check
      Builds the harness, renders what beads/test/checks.py needs into
      renders/check/, measures it and runs every assertion. Exits 1 if any
      fails.

  python3 beads/test/measure.py renders/stock/*.wav > renders/stock/metrics.txt
      Writes the stock baseline.

Click detector: the audio is split into 1 ms frames. For each frame it takes
the RMS of the second difference (x[n] - 2x[n-1] + x[n-2]), which follows
corners and steps in the waveform rather than its pitch. A frame is a click
when that is more than CLICK_RATIO times the median of the non-silent frames
within CLICK_WINDOW_MS either side. Neighbouring flagged frames count as one
click. Silent frames (RMS under SILENCE_DBFS) are never flagged, except for a
step into silence. Checked on synthetic grains: 0.5 ms linear edges are all
flagged; 5 ms linear and Hann edges never are.
"""

import array
import math
import os
import shutil
import statistics
import struct
import subprocess
import sys
import time

CLICK_RATIO = 4.0          # 12 dB above the local median
CLICK_WINDOW_MS = 50
SILENCE_DBFS = -60.0
MAX_TIMESTAMPS = 10

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))


def db(x):
    return 20.0 * math.log10(x) if x > 0 else -math.inf


# --- Reading ------------------------------------------------------------------

def read_wav(path):
    """Return (sample_rate, [channel, ...]) with samples as floats in [-1, 1].

    Handles 16, 24 and 32-bit PCM, 32-bit float, and WAVE_FORMAT_EXTENSIBLE.
    """
    with open(path, "rb") as f:
        data = f.read()
    if data[0:4] != b"RIFF" or data[8:12] != b"WAVE":
        raise ValueError("%s: not a WAV file" % path)
    fmt = None
    pcm = None
    pos = 12
    while pos + 8 <= len(data):
        cid = data[pos:pos + 4]
        size = struct.unpack("<I", data[pos + 4:pos + 8])[0]
        body = data[pos + 8:pos + 8 + size]
        if cid == b"fmt ":
            tag, channels, rate = struct.unpack("<HHI", body[0:8])
            bits = struct.unpack("<H", body[14:16])[0]
            if tag == 0xFFFE:
                tag = struct.unpack("<H", body[24:26])[0]
            fmt = (tag, channels, rate, bits)
        elif cid == b"data":
            pcm = body
        pos += 8 + size + (size & 1)
    if fmt is None or pcm is None:
        raise ValueError("%s: missing fmt or data chunk" % path)
    tag, channels, rate, bits = fmt

    if tag == 3 and bits == 32:
        samples = array.array("f")
        samples.frombytes(pcm[:len(pcm) // 4 * 4])
        scale = 1.0
    elif tag == 1 and bits == 16:
        samples = array.array("h")
        samples.frombytes(pcm[:len(pcm) // 2 * 2])
        scale = 1.0 / 32768
    elif tag == 1 and bits == 32:
        samples = array.array("i")
        samples.frombytes(pcm[:len(pcm) // 4 * 4])
        scale = 1.0 / 2147483648
    elif tag == 1 and bits == 24:
        n = len(pcm) // 3
        samples = [int.from_bytes(pcm[3 * i:3 * i + 3], "little", signed=True)
                   for i in range(n)]
        scale = 1.0 / 8388608
    else:
        raise ValueError("%s: unsupported format %d, %d-bit" % (path, tag, bits))
    if sys.byteorder != "little" and isinstance(samples, array.array):
        samples.byteswap()

    chans = [[s * scale for s in samples[c::channels]] for c in range(channels)]
    return rate, chans


# --- Metrics ------------------------------------------------------------------

def levels(chans):
    peak = max(max(abs(s) for s in c) for c in chans)
    total = sum(sum(s * s for s in c) for c in chans)
    count = sum(len(c) for c in chans)
    rms = math.sqrt(total / count) if count else 0.0
    clipped = sum(1 for c in chans for s in c if abs(s) >= 32767 / 32768)
    return peak, rms, clipped


def frame_stats(chans, frame):
    """Per 1 ms frame: (RMS, second-difference RMS), each the max over channels."""
    n = min(len(c) for c in chans)
    nframes = n // frame
    rms = [0.0] * nframes
    hf = [0.0] * nframes
    for c in chans:
        for f in range(nframes):
            a = f * frame
            seg = c[a:a + frame]
            e = math.sqrt(sum(s * s for s in seg) / frame)
            lo = max(a, 2)
            h2 = 0.0
            for i in range(lo, a + frame):
                d = c[i] - 2.0 * c[i - 1] + c[i - 2]
                h2 += d * d
            h = math.sqrt(h2 / frame)
            if e > rms[f]:
                rms[f] = e
            if h > hf[f]:
                hf[f] = h
    return rms, hf


def gated_rms(rms_frames):
    """RMS over the non-silent frames only: the level of the sound when it plays."""
    floor = 10 ** (SILENCE_DBFS / 20)
    active = [e for e in rms_frames if e > floor]
    if not active:
        return 0.0
    return math.sqrt(sum(e * e for e in active) / len(active))


def clicks(rms_frames, hf, frame_ms=1.0):
    """Return click times in ms (one per run of flagged frames)."""
    floor = 10 ** (SILENCE_DBFS / 20)
    # A frame is active if it has signal, or a step out of the frame before it
    # (a cut to silence that lands on a frame boundary shows up only in hf).
    active = [e > floor or h > floor for e, h in zip(rms_frames, hf)]
    w = int(CLICK_WINDOW_MS / frame_ms)
    global_vals = [h for h, a in zip(hf, active) if a]
    global_median = statistics.median(global_vals) if global_vals else 0.0
    times = []
    previous = False
    for f in range(len(hf)):
        flagged = False
        if active[f]:
            neighbours = [hf[g] for g in range(max(0, f - w), min(len(hf), f + w + 1))
                          if g != f and active[g]]
            median = statistics.median(neighbours) if len(neighbours) >= 3 \
                else global_median
            flagged = median > 0 and hf[f] > CLICK_RATIO * median
        if flagged and not previous:
            times.append(f * frame_ms)
        previous = flagged
    return times


def read_attacks(path):
    rises = []
    with open(path) as f:
        next(f)
        for line in f:
            parts = line.strip().split(",")
            if len(parts) == 2:
                rises.append(float(parts[1]))
    return rises


def read_envelope_curves(path):
    """Rows of TestGrainEnvelopeShape's CSV, one per Shape position and grain."""
    rows = []
    with open(path) as f:
        next(f)
        for line in f:
            v = [float(x) for x in line.strip().split(",")]
            rows.append({"shape": v[0], "duration": v[1], "first": v[2],
                         "last": v[3], "peak": v[4], "curve": v[5:]})
    return rows


def read_values(path):
    """A key,value CSV written alongside a render."""
    with open(path) as f:
        next(f)
        return {k: float(v) for k, v in
                (line.strip().split(",") for line in f if line.strip())}


def measure_file(path):
    rate, chans = read_wav(path)
    frame = max(1, rate // 1000)
    peak, rms, clipped = levels(chans)
    rms_frames, hf = frame_stats(chans, frame)
    m = {
        "path": path,
        "seconds": len(chans[0]) / rate,
        "peak_dbfs": db(peak),
        "rms_dbfs": db(rms),
        "gated_rms_dbfs": db(gated_rms(rms_frames)),
        "clipped": clipped,
        "clicks": clicks(rms_frames, hf),
    }
    if path.endswith("_wet.wav"):
        csv = path[:-len(".wav")] + "_attacks.csv"
        if os.path.exists(csv):
            m["attacks_ms"] = read_attacks(csv)
    curves = path[:-len(".wav")] + "_curves.csv"
    if os.path.exists(curves):
        m["envelope_curves"] = read_envelope_curves(curves)
    values = path[:-len(".wav")] + "_values.csv"
    if os.path.exists(values):
        m["values"] = read_values(values)
    return m


def wet_dry_pairs(metrics):
    """Yield (stem, wet, dry) for every X_wet.wav with a matching X_dry.wav."""
    by_path = {m["path"]: m for m in metrics}
    for m in metrics:
        if m["path"].endswith("_wet.wav"):
            dry = by_path.get(m["path"][:-len("_wet.wav")] + "_dry.wav")
            if dry:
                yield m["path"][:-len("_wet.wav")], m, dry


# --- Reporting ----------------------------------------------------------------

def report(metrics, out=sys.stdout):
    for m in metrics:
        c = m["clicks"]
        stamps = ", ".join("%.3f s" % (t / 1000) for t in c[:MAX_TIMESTAMPS])
        if len(c) > MAX_TIMESTAMPS:
            stamps += ", ..."
        print("%s" % m["path"], file=out)
        print("  %.1f s  peak %.1f dBFS  rms %.1f dBFS  gated rms %.1f dBFS  clipped %d"
              % (m["seconds"], m["peak_dbfs"], m["rms_dbfs"], m["gated_rms_dbfs"],
                 m["clipped"]), file=out)
        print("  clicks %d%s" % (len(c), ("  at " + stamps) if c else ""), file=out)
        if "attacks_ms" in m:
            a = m["attacks_ms"]
            if a:
                print("  grain attacks (10-90%%) %d grains  min %.2f ms  median %.2f ms"
                      % (len(a), min(a), statistics.median(a)), file=out)
            else:
                print("  grain attacks: none logged", file=out)
    for stem, wet, dry in wet_dry_pairs(metrics):
        print("%s wet - dry:  peak %+.1f dB  rms %+.1f dB  gated rms %+.1f dB"
              % (stem, wet["peak_dbfs"] - dry["peak_dbfs"],
                 wet["rms_dbfs"] - dry["rms_dbfs"],
                 wet["gated_rms_dbfs"] - dry["gated_rms_dbfs"]), file=out)


# --- Check --------------------------------------------------------------------

def run_check():
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    import checks

    os.chdir(REPO)
    build = subprocess.run(["make", "-f", "beads/test/makefile"],
                           capture_output=True, text=True)
    if build.returncode != 0:
        print(build.stdout + build.stderr)
        print("FAIL: harness did not build")
        return 1

    out_dir = os.path.join("renders", "check")
    os.makedirs(out_dir, exist_ok=True)
    tests = []
    for c in checks.CHECKS:
        for t in c.renders:
            if t not in tests:
                tests.append(t)
    for t in tests:
        started = time.time() - 1
        run = subprocess.run(["./beads_test", t], capture_output=True, text=True)
        if run.returncode != 0:
            print(run.stdout + run.stderr)
            print("FAIL: ./beads_test %s exited %d" % (t, run.returncode))
            return 1
        # The harness writes into the working directory; move what it wrote.
        for name in sorted(os.listdir(".")):
            if (name.startswith("beads_") and name.endswith((".wav", ".csv"))
                    and os.path.getmtime(name) >= started):
                shutil.move(name, os.path.join(out_dir, name))

    wavs = sorted(os.path.join(out_dir, n) for n in os.listdir(out_dir)
                  if n.endswith(".wav"))
    metrics = {os.path.basename(p): measure_file(p) for p in wavs}
    report(list(metrics.values()))
    print()

    failed = 0
    for c in checks.CHECKS:
        ok, detail = c.fn(metrics)
        print("%s  %s  (%s)" % ("PASS" if ok else "FAIL", c.name, detail))
        failed += not ok
    print("\n%d of %d checks failed" % (failed, len(checks.CHECKS)))
    return 1 if failed else 0


def main(argv):
    if not argv or argv[0] in ("-h", "--help"):
        print(__doc__)
        return 0
    if argv[0] == "--check":
        return run_check()
    report([measure_file(p) for p in argv])
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
