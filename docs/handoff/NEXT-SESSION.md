# Next session: after 8 November 2026 (grilling done, spec and tickets written)

## Where things stand

On `master`. This session added three commits on top of `500be04`:
- `ad1ed1c`: the plan write-up
- `37b4619`: the check command added to tickets 01 and 04
- this handoff

Whether they're pushed depends on Jesse's answer at this wrap; `/session-start`'s ahead/behind line settles it. **No DSP changes yet. Nothing flashed. No settings backup exists.** All gates are green (see below).

This session:
- **Grilling is finished.** `/grill-with-docs` ran on the brief. Every decision is recorded in:
  - `.scratch/soft-character/spec.md`
  - four ADRs in `docs/adr/` (GCC 15 build, settings sector, Shape sweep, envelope-sum normalisation)
  - `GLOSSARY.md`
  - the parked ideas in `.scratch/backlog/parked.md`
  - the planned-changes table in `CLAUDE.md`, now 13 rows numbered 0–12
- **15 tickets** are in `.scratch/soft-character/issues/01-…` to `15-…`. Each carries `Status:` and `Blocked by:` lines.
  - The spec and tickets were written by hand, following the `/to-spec` and `/to-tickets` conventions. Those skills existed but weren't in this session's invocable list.
  - **Jesse hasn't reviewed the ticket breakdown itself.** `/to-tickets` would normally quiz him on granularity before writing. He approved the plan, not the 15-way split.
- **Ticket 01 now asks for one pass/fail check command.** Its first assertion (wet and dry within 1 dB at 12 o'clock) fails on stock, and ticket 04 makes it pass. This gives `/tdd` a failing check to start from.
- **Workflow agreed** (via `/ask-matt`): `/implement` one ticket per fresh session, with `/clear` between. Not `/implement-spec`, because the chain is nearly linear and every DSP ticket waits on Jesse's A/B verdict. Run `/retro` after ticket 01, in that session.

## Start here

Run `/implement .scratch/soft-character/issues/01-render-measurement-tool.md`. Every later ticket's acceptance depends on its metrics and its check command, and it needs no hardware. Read `spec.md` decision 1 and ticket 01 first. Then run `/retro` in the same session before clearing. Optionally, glance at the ticket list with Jesse first, since he hasn't reviewed the breakdown (for example, 07 and 08 could merge).

## Waiting on Jesse

- **Settings backup.** Read sector 15 (0x081E0000, 128 KB) with STM32CubeProgrammer into `backups/` (git-ignored), and copy it off this machine. This blocks any flash, and so the CPU measurements in tickets 05, 09 and 10.
- **Ticket 03, the reference captures.** Play the reference source into the Arbhar and the Multigrain, using each reference patch. Possible once ticket 02 lands.
- **A/B verdicts** on each DSP ticket (04 onwards), recorded in the ticket's Comments.
- **Optional:** block pushes to upstream with `git remote set-url --push upstream DISABLED`. Offered twice, not done.
- **Optional:** install `uv` and a Perplexity key for `/research` (see memory `research-script-broken`).

## In flight

Nothing. No delegates, branches or worktrees.

## Will bite

- **The reference source is clip 09 of the Sublustris demos** (picked D chords, CC0 Emily guitar). It's built by `~/Documents/Sites/sublustris-versio/docs/minisite/tools/demos/build_sources.py` from `~/Documents/Sites/resilio-versio/test_audio/demo_sources/cc0/guitar_emily/`. That script writes 48 kHz float WAV, and the harness expects 16-bit, so convert. The `*-dry.mp3` in that repo is lossy, so don't use it.
- **Stock Shape order** is square → decay → bell (about ⅔ of the travel) → reverse (`grain_envelope.h:59-79`). The new order puts decay before square (ADR 0003).
- **`kInstantCrossfadeTime` is tied to the scheduler thresholds** (comment at `granular_player.h:41`). Ticket 08 must check that ordering still holds.
- **Density** is periodic left of noon and random right of noon (`trigger_generator.cc:110, 132-137`). Pan spread is zero unless something is randomised (`granular_player.h:85-126`).
- **Reading PDFs:** there's no pdftotext or pdftoppm. Use the PDFKit Swift route in memory `module-manuals`, which also lists Jesse's local manual paths. Multigrain v1.4 lists seven discrete shapes, with Tukey as the default.
- The firmware linker script lets the image run over the settings sector, so always build with `./firmware.sh`, never bare `make` (ADR 0002).
- `renders/` and `audio_samples/` are git-ignored, so a fresh clone must regenerate both.

## Gates

At `37b4619` (docs-only changes since `526021e`):
- harness: builds, 0 errors or warnings
- firmware: image ends at 0x0805ABB0, 1557 KB below the settings sector
- stock renders, 30 s each, 0 clipped samples:
  - `beads_processor`: peak −3.2 dBFS, RMS −29.3 dBFS
  - `beads_granular_player`: peak −5.3, RMS −37.2
  - `beads_granular_processor`: peak −10.7, RMS −32.3

## Where to look

- `.scratch/soft-character/spec.md`: the design, in 12 decisions
- `.scratch/soft-character/issues/01-render-measurement-tool.md`: the next ticket
- `CLAUDE.md`: the planned-changes table and workflow
- `GLOSSARY.md` and `docs/adr/`: vocabulary and the decisions not to undo
- `.claude/session.md`: gates and hazards
