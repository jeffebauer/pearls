# Pearls

A personal firmware for a Eurorack granular texture module, derived from Mutable Instruments' open-source Beads code. It aims for the soft, smooth grain character of the Instruo Arbhar while keeping live, always-on processing of the input.

## Project

**Pearls**:
The published name of this firmware and repo.
_Avoid_: Beads, Soft Beads (internal description only), Mutable Instruments

**Stock**:
The unmodified upstream firmware and its behaviour, the baseline every change is A/B'd against.
_Avoid_: original, vanilla, factory

**Reference capture**:
A recording of the reference source played through the Arbhar or the Multigrain using a reference patch, used as a target for comparison.
_Avoid_: sample, demo

**Reference source**:
The one recording fed to every module and to the desktop harness, so that their outputs can be compared directly.
_Avoid_: test file, input

**Reference patch**:
A written set of control positions, recreated on each module, that makes a reference capture repeatable.
_Avoid_: preset, setting

## Signal path

**Dry**:
The input signal as it passes to the output without granular processing.
_Avoid_: clean, direct

**Wet**:
The summed output of all grains, before the dry/wet mix.
_Avoid_: grain bus, effect signal

**Settings sector**:
The flash region that holds the module's factory calibration, which nothing may ever write.
_Avoid_: calibration area, sector 15 (location only)

## Grains

**Grain**:
One short, windowed read of the recorded buffer, with its own position, pitch, length, level and pan.
_Avoid_: voice, particle

**Window**:
The amplitude envelope applied across a grain's life, chosen by the Shape control.
_Avoid_: envelope (except for its momentary value), grain envelope

**Soft end**:
The fully clockwise region of Shape, where the window is a Gaussian and diffusion is at its strongest.
_Avoid_: smooth side, bell end

**Diffusion**:
Short allpass smearing applied to the wet signal to blur grain edges without adding a reverb tail.
_Avoid_: blur, reverb

**Voice stealing**:
Ending an existing grain early, with a fade, to make room for a new one when every grain slot is in use.
_Avoid_: grain cutting, recycling

**Overlap**:
How many grains sound at once on average, as set by Density when no clock is patched.
_Avoid_: density (the control), grain rate

**Click**:
An audible discontinuity at a grain's start or end, or at a seam in the buffer.
_Avoid_: pop, glitch
