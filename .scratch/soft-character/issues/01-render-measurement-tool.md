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
- [ ] One check command (e.g. `python3 beads/test/measure.py --check`) renders, measures, and exits non-zero if any assertion fails. Assertions live in a single file so that each later ticket can add its own failing check first, for `/tdd`
- [ ] The first assertion is "wet and dry within 1 dB at 12 o'clock". It fails on stock (about 6 dB apart) and ticket 04 makes it pass
- [ ] The check command is named in `.claude/session.md` under Gates

## Comments
