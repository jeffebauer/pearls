#!/usr/bin/env python3
"""Print peak, RMS and clipped-sample count for 16-bit WAV renders.

Usage: python3 beads/test/levels.py renders/stock/*.wav
Exits 1 if any file has clipped samples.
"""

import math
import struct
import sys
import wave

clipped_any = False
for path in sys.argv[1:]:
    with wave.open(path) as w:
        n = w.getnframes()
        ch = w.getnchannels()
        s = struct.unpack("<%dh" % (n * ch), w.readframes(n))
        rate = w.getframerate()
    peak = max(abs(x) for x in s) / 32768
    rms = math.sqrt(sum(x * x for x in s) / len(s)) / 32768
    clipped = sum(1 for x in s if abs(x) >= 32767)
    clipped_any |= clipped > 0
    print(
        "%-45s %5.1fs  peak %6.1f dBFS  rms %6.1f dBFS  clipped %d"
        % (path, n / rate, 20 * math.log10(peak + 1e-12),
           20 * math.log10(rms + 1e-12), clipped)
    )
sys.exit(1 if clipped_any else 0)
