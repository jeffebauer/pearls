#!/usr/bin/env python3
"""Generate a synthetic test source for the desktop harness.

Writes audio_samples/kettel.wav: 30 s, 48 kHz, stereo, 16-bit PCM.
Content is chosen to expose grain artefacts: plucked notes with sharp
attacks, a sustained pad (clicks and zipper noise show up on it), and
sparse percussive hits. Pure stdlib, no numpy.

Usage: python3 beads/test/make_test_source.py [output.wav]
"""

import math
import os
import random
import struct
import sys
import wave

SR = 48000
SECONDS = 30
N = SR * SECONDS

random.seed(1)
left = [0.0] * N
right = [0.0] * N


def midi_hz(n):
    return 440.0 * 2 ** ((n - 69) / 12.0)


def pluck(start, freq, length, pan, amp):
    # Karplus-Strong string.
    period = max(2, int(SR / freq))
    buf = [random.uniform(-1, 1) for _ in range(period)]
    gl = amp * math.cos(pan * math.pi / 2)
    gr = amp * math.sin(pan * math.pi / 2)
    idx = 0
    for i in range(min(length, N - start)):
        s = buf[idx]
        nxt = buf[(idx + 1) % period]
        buf[idx] = 0.996 * 0.5 * (s + nxt)
        idx = (idx + 1) % period
        left[start + i] += s * gl
        right[start + i] += s * gr


def pad(freqs, amp):
    # Slow detuned saw-ish pad, soft fade in and out across the file.
    phases = [[random.random(), random.random()] for _ in freqs]
    for i in range(N):
        t = i / SR
        env = min(1.0, t / 3.0) * min(1.0, (SECONDS - t) / 3.0)
        l = r = 0.0
        for k, f in enumerate(freqs):
            for ch, det in enumerate((0.998, 1.002)):
                p = phases[k][ch] + f * det / SR
                p -= int(p)
                phases[k][ch] = p
                # Band-limited-ish: sum of first 4 harmonics.
                v = sum(math.sin(2 * math.pi * p * h) / h for h in range(1, 5))
                if ch == 0:
                    l += v
                else:
                    r += v
        left[i] += l * amp * env
        right[i] += r * amp * env


def hit(start, amp):
    # Short noise burst with fast decay.
    for i in range(min(int(0.08 * SR), N - start)):
        e = math.exp(-i / (0.012 * SR))
        v = random.uniform(-1, 1) * e * amp
        left[start + i] += v
        right[start + i] += v


pad([midi_hz(n) for n in (50, 57, 62, 65)], 0.035)

scale = [62, 64, 65, 67, 69, 72, 74, 76]
t = 0.5
while t < SECONDS - 1.5:
    note = random.choice(scale)
    pluck(int(t * SR), midi_hz(note), int(1.2 * SR), random.random(), 0.35)
    t += random.choice([0.25, 0.5, 0.5, 0.75, 1.0])

for beat in range(4, SECONDS - 2, 2):
    hit(int(beat * SR), 0.4)

peak = max(max(abs(x) for x in left), max(abs(x) for x in right))
gain = 0.7 / peak

out = sys.argv[1] if len(sys.argv) > 1 else "audio_samples/kettel.wav"
os.makedirs(os.path.dirname(out) or ".", exist_ok=True)
frames = bytearray()
for l, r in zip(left, right):
    frames += struct.pack("<hh", int(l * gain * 32767), int(r * gain * 32767))
with wave.open(out, "wb") as w:
    w.setnchannels(2)
    w.setsampwidth(2)
    w.setframerate(SR)
    w.writeframes(bytes(frames))
print(f"wrote {out}")
