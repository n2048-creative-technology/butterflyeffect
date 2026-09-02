# butterflyeffect

An art-installation project built around small ESP32-S3 "leaf" PCBs: each
leaf carries a radar presence sensor and a TFT display that reacts with a
glitch/text effect. Includes the PCB design (KiCad, three board revisions),
two firmware variants, and a source poem ("poetry monitors butterfly.pdf")
that the TFT glitch text is drawn from.

**Status: messy / in-progress.** This repo is committed close to how it sits
on the author's disk — see "Known issues" below before relying on it as a
clean starting point.

## What's in here

- **`PCB/leaf/`, `PCB/leaf2/`, `PCB/leaf3/`** — three KiCad project revisions
  for the same "leaf" board (ESP32-S6/S3 module, per `esp32-s3-leaf.kicad_sch`
  at the repo root). `leaf3` is the newest revision judging by file
  timestamps; `leaf`/`leaf2` look like earlier iterations kept for reference.
  Each folder also has a `production/` subfolder with JLCPCB-ready gerbers,
  BOM and pick-and-place files, plus a `*-backups/` folder full of
  auto-save zips from KiCad — safe to ignore/delete, not source files.
- **`esp32-s3/`** — a fourth, separate KiCad PCB export (gerbers + production
  files only, no schematic source committed alongside it) — likely an older
  or alternate board revision. Not clearly connected to `PCB/leaf*` from the
  file layout alone.
- **`code/esp32-s3-radar-test/`** — PlatformIO project reading an RD-03D
  mmWave presence/distance radar module over UART (`RD03D.h/.cpp`).
- **`code/esp32-s3-tft-display/`** — PlatformIO project driving a TFT
  display (via `TFT_eSPI`) with a scrolling/glitching text effect —
  `GLITCH_PERCENT`, `FRAME_DELAY_MS`, and the displayed text are `#define`d
  at the top of `leafs/src/main.cpp` and `code/esp32-s3-tft-display/src/main.cpp`.
  Uses a RAM-framebuffer sprite for performance (see the comment block in
  `main.cpp` explaining why).
- **`leafs/`** — a third, PlatformIO-structured copy of the TFT glitch
  firmware (`include/User_Setup.h` for `TFT_eSPI` pin config). Appears to be
  the "current" leaf firmware; unclear how it differs from
  `code/esp32-s3-tft-display/` without a diff.
- **`3dmodels/wings.scad`** — OpenSCAD source for a mechanical "wings" part.
- **`poetry monitors butterfly.pdf`** — the source text the glitch display
  renders excerpts of.

## Hardware

- ESP32-S3 module (per `esp32-s3-leaf.kicad_sch` / the `leaf` PCB silkscreen)
- RD-03D mmWave radar module (UART, 256000 baud) for the radar-test firmware
- A TFT display wired for `TFT_eSPI` (pinout is in `leafs/include/User_Setup.h`
  — check it before wiring a display, it is not documented here separately)
- Custom "leaf" PCB (KiCad projects above) carrying the ESP32-S3 and
  supporting components

No end-to-end wiring diagram tying the radar + display + leaf PCB into one
finished device exists in this repo — piece it together from the two
firmware projects' pin `#define`s and the KiCad schematic.

## Build & flash

Both firmware projects use PlatformIO:

```bash
cd code/esp32-s3-radar-test   # or code/esp32-s3-tft-display, or leafs/
pio run
pio run -t upload
pio device monitor
```

## Known issues

- **Three near-duplicate PCB revisions** (`PCB/leaf`, `PCB/leaf2`, `PCB/leaf3`)
  and a fourth unrelated-looking one (`esp32-s3/`) are committed side by
  side with no changelog explaining what changed between them. Before
  fabricating, diff the schematics or ask the author which is current
  (`leaf3` is the most recently modified).
- **KiCad auto-save backup zips are committed** (`*-backups/*.zip`, dozens
  of them, several MB each) — these bloat the repo and aren't needed to
  build the board; the checked-in `.kicad_pcb`/`.kicad_sch` files are the
  actual source of truth.
- **Two copies of the TFT glitch firmware** (`code/esp32-s3-tft-display/`
  and `leafs/`) with no documented relationship between them.
- No top-level wiring diagram or bill of materials tying the radar sensor,
  display, and PCB together into one buildable device.
