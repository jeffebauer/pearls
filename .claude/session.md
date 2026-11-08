# Session file: pearls

Facts for `/session-start` and `/session-wrap`. Rules that always apply are in `CLAUDE.md`.

## Handoff
- Path: docs/handoff/NEXT-SESSION.md
- Owner list: none. The handoff's "Waiting on Jesse" is the list.

## Read first
- `CLAUDE.md`: the planned-changes table says which DSP change is next
- the files the next change touches (named in that table)

## Reality checks
```bash
test -x beads_test && echo "harness built" || echo "harness not built: make -f beads/test/makefile"
test -f audio_samples/kettel.wav && echo "test source ok" || echo "no test source: python3 beads/test/make_test_source.py"
/bin/ls renders/ 2>/dev/null                     # stock/ is the A/B reference; one folder per change after that
/bin/ls backups/ 2>/dev/null || echo "NO SETTINGS BACKUP"
```
No `renders/stock/` means there's nothing to A/B against: render it from the upstream commit before
changing DSP.

## Environment checks
```bash
test -x /Applications/ArmGNUToolchain/15.3.rel2/arm-none-eabi/bin/arm-none-eabi-gcc && echo "arm gcc ok" || echo "arm toolchain MISSING"
test -d /Applications/STMicroelectronics/STM32Cube/STM32CubeProgrammer && echo "programmer ok" || echo "STM32CubeProgrammer missing"
git submodule status | grep -v '^ ' || echo "submodules ok"   # a leading - or + means not checked out or moved
```

## Safe repairs
- `git submodule update --init` when a submodule isn't checked out
- rebuild the harness, regenerate `audio_samples/kettel.wav`

## Ask first
- anything that flashes, erases or reads the module (flashing is always done by Jesse)
- `git submodule update` that would move a submodule to a different commit

## Hazards
- **Never erase or write the settings sector** (sector 15, 0x081E0000 to 0x081FFFFF): it holds
  calibration. No custom flash until a backup exists in `backups/` and a copy is somewhere off this
  machine.
- `./firmware.sh` fails if the image reaches 0x081E0000. Never work around that.
- Never push to `upstream` (pichenettes/eurorack). Nothing goes back to it.
- Don't publish anything named "Mutable Instruments" or "Beads". Keep MIT licence headers intact.
- One DSP change per commit, so each can be reverted and A/B'd alone.

## Gates
No remote CI: these are the only gates. All fast (seconds).
```bash
make -f beads/test/makefile 2>&1 | grep -E "(error|warning):" ; test -x beads_test && echo "harness ok"
./firmware.sh 2>&1 | tail -1                    # record "Flash image ends at 0x…, N KB below the settings sector"
python3 beads/test/levels.py renders/<latest>/*.wav   # record peak/RMS/clipped per render; exit 1 = clipping
python3 beads/test/measure.py --check           # renders, measures, runs beads/test/checks.py; exit 1 = a check failed
```
`measure.py --check` has passed since ticket 05, so a failure is real. Stock baseline
metrics (levels, clicks, attacks, wet − dry) are in `renders/stock/metrics.txt`; compare with
`python3 beads/test/measure.py renders/<latest>/*.wav`.
Record: harness built (yes/no), firmware end address and headroom, levels of the latest renders
against `renders/stock/`, and the sha. For DSP changes, the A/B listening verdict is Jesse's. Record
it, or record that it's pending.

## Docs to sync
- `CLAUDE.md`: the planned-changes table (mark changes done with their commit sha)

## Push
- Remote: origin (https://github.com/jeffebauer/pearls), public
- What a push does: backup only. No CI, no deploy.
- Never push to: upstream

## Branches
Work on `master` for single-commit changes. Branch `exp/<what>` for anything that may be thrown away.

## Prune
- Outputs: listening examples in `renders/`: sweeps and comparisons inside ticket folders (`sweep*/`, `sources/`, `q*/`, `pre/`, `*-before/`). Settled once the ticket records Jesse's verdict.
- Keep: `renders/stock/`, `renders/stock-shape/`, `renders/sources/`, `renders/check/`, and the top level of each ticket folder (final renders and `metrics.txt`).
- Index: `beads/test/listen_notes.json`

## Delegation
off
