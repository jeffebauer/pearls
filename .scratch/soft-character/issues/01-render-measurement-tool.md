# 01: Render measurement tool

Status: ready-for-agent
Type: task
Spec: `../spec.md`, Spec decision 1

## What

Build a measurement tool for the desktop harness (likely `beads/test/measure.py`, plus envelope logging in `beads_test.cc`).

## Acceptance

- [ ] Reports, per WAV, peak and RMS of the wet and the dry (the harness renders them separately) and their difference in dB
- [ ] Click detector: splits audio into 1 ms frames and flags frames whose high-frequency energy (e.g. first-difference RMS) jumps well above the local median, with a count and timestamps
- [ ] Grain attack time: the harness logs each grain's envelope, and the tool reports the minimum and median 10–90% rise time
- [ ] Runs on stock renders and prints a baseline, saved in `renders/stock/metrics.txt`
- [ ] Works on hardware captures too (level and click metrics only)

## Comments
