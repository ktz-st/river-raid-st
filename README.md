# River Raid STE

Standalone GodLib + C/68000 port based on Atari 800 behaviour and graphics.
Native YM2149 audio only, 512 KiB STE, packed with UPX. The game needs only
`release/RR.PRG`; it never loads the build assets and does not create a log.

## Layout

- `src/`: C, headers and handwritten 68000 assembly.
- `assets/`: editable STE PNG atlases/title, original cartridge input, captured
  HUD data, palette/ink tables and the migration regression manifest. No DMA samples.
- `scripts/`: asset, audio, HUD and assembly generators plus build verification.
- `build/`: all generated C/headers/assembly, objects, dependencies, link maps,
  inventory report, raw executable and matching debug `RR.PRG`.
- `release/`: stripped, UPX-packed `RR.PRG` only.

## Build

See [AGENTS.md](AGENTS.md) for a fresh-machine setup: dependency clone commands,
directory layout, exact tested toolchain versions, ABI checks and ROM requirements.
The verified compiler is **MiNT ELF GCC 15.2.0** with `mshort/mfastcall` multilibs;
the host GCC or the older MiNT a.out compiler is not a substitute.

```sh
make                 # release/RR.PRG
make debug           # build/RR.PRG with symbols
make verify          # original asset hashes and current release/link audit
make smoke-hatari    # boot/pause/HUD/exit on STE 512 KiB
make verify-hatari   # also test bridge destruction, checkpoints and respawn
make run             # Hatari STE, 512 KiB, default TOS
make clean           # removes build products, never sources/assets
```

Required tools: `m68k-atari-mintelf-gcc`, `vasmm68k_mot`, UPX and Python 3
(with Pillow). GodLib and libcmini are sibling directories by
default; override `GODLIB_DIR` and `CMINI_DIR` if needed. Headers are expected
under the common parent (`-I..`). This project does not use SGDL, AGT
or a C++ compiler, and has no dependency on the old `river-raid/` directory.

The original 8192-byte cartridge must be present locally as `assets/RIVER_A8.BIN`.
It has been copied for this local build but is ignored by Git; it is not part
of the distributable game. All derived output goes to `build/generated/`.
`make verify` checks the original asset hashes and current release invariants.
The historical executable hash is kept for reference, not enforced after gameplay changes.
Automated Hatari checks are optional: they use the locally installed
`hatari-hrdb` and helpers from `~/.agents/skills/hatari-debugging/scripts`.
They write screenshots, test disks, RAM dumps and reports only under `build/`.

## Controls

Arrows steer/change speed; either SHIFT or joystick FIRE shoots/launches.
SPACE toggles pause; either SHIFT or joystick FIRE also resumes it.
F1 restarts, ESC asks to exit. Y/Enter confirms, N/ESC cancels.
Losing the last life pauses for 1.5 seconds after the explosion, then returns
to the title. A fresh FIRE press starts a new game with three lives.
Arrows alone do not leave the title or launch a checkpoint respawn.
Destroying a bridge commits the next-section checkpoint immediately; ramming
costs a life but also unlocks the next section. No AUTO directory is created.

The old research repository and its historical tests/profiling captures remain
separate. No such archives or emulator memory dumps are copied into this build.

## Historical Migration Verification

At migration, clean `make -j4 verify` passed all 11 input/generated/program hashes.
The packed release was byte-identical to the tested old layout: 53552 bytes, SHA256
`dd77ae88baf2586144c3aa10deda6a46893799a7fa084d25a044a656cecb02ab`.
At migration, the new-path Hatari smoke check passed on STE 512 KiB: boot, exact HUD
restore, preserved pause on cancelled ESC, confirmed TOS exit and no log file.
This is migration/boot verification, not a new whole-game performance profile.

The current build no longer calls GodLib File_Init/DeInit: all runtime assets
are embedded. Bridge checkpoint restore discards completed tank destructions
without removing live tanks surviving on the banks.

## Current Regression Verification

Before GodLib trimming, the packed release was 52124 bytes. `make verify` passes the 10 original asset
hashes, stripped/YM-only checks and absence of the File subsystem. The 512 KiB
Hatari regression passes pause/FIRE, restart, automatic game-over reset, bridge
checkpoints, repeated respawn, tank cleanup and confirmed exit to TOS. The tank
case uses injected on-bridge/bank tank states with a naturally generated bridge;
this is a targeted regression, not a complete campaign playthrough.

## Private STE GodLib

`src/ste_platform.c` and `src/ste_graphic.c` provide the game's narrow lifecycle
and sprite-only four-plane dispatch. `scripts/build_godlib_subset.py` builds a
private subset under `build/godlib/`; it never edits the shared GodLib checkout.
It rebuilds referenced C modules with per-function sections and runs section GC
in a relocatable m68k ELF link before the final MiNT PRG link. The PRG backend
alone did not discard unused functions in our tests.

The original blitter sprite/clipping and STE video assembly are extracted
verbatim. Each build compares their instructions against the original archive,
ignoring relocation fields only. CPU sprite preshifts, unrolled copy/terrain
kernels, native HUD glyph masks and collision code are unchanged. Font loaders,
sprite converters, generic drawing primitives, fades, and TT/Falcon video/audio
paths are not linked. Generic hardware detection and state restoration remain.

Immediately after trimming: 33364-byte UPX release (previously 52124), 72782-byte raw PRG
(previously 111270). GodLib code/data falls from 49172 to 11956 bytes; BSS falls
from 64260 to 62514 bytes, excluding dynamic screen allocations and stack.
There are no replacement empty font arrays: the unused font module is absent.

Hatari STE/68000/8 MHz, 512 KiB: the full targeted gameplay regression passes.
A separate old/new comparison at maximum scroll with pulsed FIRE runs 64 game
frames in 64 VBLs in both builds (50 FPS). Ready and final terrain/sprite/HUD
bytes match exactly. Profiled program cycles excluding Vbl_WaitVbl are 6192660
before and 6193172 after (+0.0083%, effectively unchanged in this test).
This controlled test disables player collisions and fuel depletion; it is not
a full-campaign performance guarantee. Results are under
`build/godlib_comparison/report.json` and `build/tester_regression/report.json`.

## Projectile Terrain Collisions

The player shot now follows A8 $A4B6/$A58D and the DLI's `M2PF & $03`:
object hits are resolved first, then COLPF0/COLPF1 terrain cancels a surviving
shot without score or an object explosion. Water and COLBK do not block it.
STE terrain inks 1/2 block, as do road inks 4/5/6, which recolour COLPF0 rather
than change its original collision meaning. The check reads the clean buffer,
not a page containing sprites. A two-pixel word mask sweeps the old/new shot
rows, including its previous tail, with vertical clipping and ring duplication.

`src/shot_terrain.s` is a 118-byte 68000 word-wise kernel, with no per-pixel
loop or extra collision bitmap. The release with this fix is 33476 bytes.
Optional checks (all output stays under `build/`):

```sh
python3 scripts/verify_shot_terrain.py          # requires machine68k
python3 scripts/verify_hatari.py --terrain-shots
make verify-hatari                            # also checks natural terrain
```

The CPU test covers 5493 cases, including all shifts, clean terrain inks,
word-boundary spill, row clipping, >64 KiB offsets, read-only background and ABI.
The dedicated Hatari test injects controlled terrain/actors at real combat
boundaries to verify object priority. The normal gameplay regression also
shoots at the unmodified generated land, brown bank and water on STE 512 KiB.

## SFX Inventory

Native YM2149 register sequences, updated once per VBL. Durations below assume
PAL 50 Hz and uninterrupted playback; these are not PCM samples or bit-exact
POKEY emulation. Sources: `src/audio.c`, `src/audio_psg.c`, `scripts/build_psg.py`.

| Sequence ID | Effect | Physical YM channel | Frames / nominal duration | Trigger |
|---|---|---|---|---|
| 0-11 | Engine, 12 throttle-dependent noise pitches | C | One-frame loop | Flying, except during refuelling/full-fuel cue |
| 12 | Refuelling | B | 8-frame loop / 160 ms per cycle | Overlapping an intact FUEL depot |
| 13 | Full-fuel cue | B | 8 / 160 ms | Refuelling crosses fuel value 65503 |
| 14 | Shot | A | 15 / 300 ms | Player fires, or an armed helicopter launches its shot |
| 15 | Object hit/explosion | C | 23 / 460 ms | Destroyed aircraft, boat, balloon, FUEL or bridge |
| 16 | Player crash | C | 31 / 620 ms | Player dies with fuel remaining |
| 17 | Out-of-fuel death | C | 34 / 680 ms | Player dies with fuel equal to zero |
| 18 | Extra life | A | 64 / 1280 ms | Each 10000-point boundary, unless already at nine lives |
| 19 | Low-fuel warning | B | 62-frame loop / 1240 ms | Flying, fuel below 0x4000, not refuelling |
| 20 | Tank shot | C | 56 / 1120 ms | Bank or bridge tank launches a shell |

Channel A carries shots/extra lives, B fuel cues, and C the engine or an
explosion/tank shot. A fourth *logical* voice borrows physical C temporarily;
the engine resumes automatically afterward. Engine noise is disabled during
refuelling/full-fuel cues. The YM has one shared noise generator; the effect
borrowing C owns it. Shots and fuel tones can still play alongside it.

Within one game update, C-event priority is EMPTY > CRASH > HIT > ENEMY.
This is not a persistent priority lock: a new C event on a later update can
restart/replace the currently playing effect. On A, LIFE overrides SHOT and
blocks further shot sounds until its sequence finishes (not projectiles). Multiple same-type events
within one update coalesce into one sound.

The low-fuel warning has 35 audible frames followed by 27 silent frames.
The tank-shot sequence contains 16 tonal frames followed by 40 noise frames.
Terrain impacts do not trigger an additional hit/explosion sound or points.
There are no separate menu, pause, restart, bridge-crossing or game-over
sequences; pause, restart, exit confirmation and respawn reset/silence audio.

## Editable PNG Assets And Title

The build reads `assets/sprites.png` and `assets/scenery.png` directly, with
frame rectangles in the matching JSON manifests. Pillow is required. Keep
the indexed 16-colour STE palette in its original order when editing in Aseprite.
Sprite cells are 80x32 (frame origin 8,2); scenery cells are 88x40 (origin 4,2).
Animation phases sit next to one another; directions have separate rows.
Only pixels inside each manifest rectangle are exported. Preserve frame sizes.
Sprite transparency is index 0; opaque black is index 13. Scenery transparency
is index 1; opaque black is index 0. Scenery pixels must remain equal in horizontal
pairs, and narrow sprite rows must retain their single opaque colour.
Masks and preshifts are generated from these PNGs, not the old PMG ROM graphics.
The initial PNG migration reproduced the native assets and packed PRG exactly;
see `build/png_migration_report.json` (before adding title code).

`assets/title.png` is the editable 320x200 indexed 16-colour STE title, with its
own palette. The build writes native ST4, PI1 and compressed STL4 copies under
`build/`; only compressed image data is embedded in the executable. It decodes
directly into the existing screen buffer, without allocating another bitmap.
The original `title.gif` is reference material, not a build input.

After the last death animation the game freezes for 75 PAL VBLs (1.5 seconds),
then fades out and returns to the full-screen title. An ordinary death still respawns on the
checkpoint bridge with no additional delay. Release FIRE and press it again
to start a new run from the title. ESC retains its exit confirmation.
`python3 -B scripts/verify_hatari.py --title-pause` measures the pause on 512 KB,
checks the decompressed image byte for byte, and exercises held/fresh FIRE.

Palette transitions use 16 brightness steps on consecutive VBLs (about 0.34 s
including the final presented frame), decoding all four STE DAC bits per channel.
Startup, title decompression and initial terrain/HUD/sprite preparation happen
with all 16 palette entries black. The title fades in only after its page has
been displayed; FIRE fades it out, prepares the full game under black, and fades
in the game palette. The game-over path fades out before decoding the title.
`python3 -B scripts/verify_hatari.py --title-fades` checks intermediate brightness,
black preparation stages, final palettes and the last-life pause on 512 KB.

## Runtime Raster Audit

Terrain, sprites, shots, background restore and HUD cells already use native
68000/blitter kernels. Fuel-marker writes, the 8px exit-confirmation font and
HUD/background fills now also use `src/raster_cpu.s`: longword masks or copies,
with no per-pixel drawing loop in production C. C prepares glyphs/coordinates,
controls dirty HUD cells and restores them through the existing ASM renderer.

Legacy packed-row conversions and the per-pixel planar preview are available
only with `RR_MODEE_REFERENCE`; normal builds no longer link these unused
preview/conversion routines. The initial LUT setup stays in C: compact terrain
literal spans still need its byte-to-plane lookup. `RR_HUD_REFERENCE` and `RR_LEGACY_ROM_ASSETS`
remain diagnostic alternatives, not release settings. Palette/DAC calculations,
collision masks and LZSS token processing are not per-pixel raster drawing.

`make verify-raster` requires the optional Python `machine68k` package. It runs
640 cases using the production 68000 code against a pixel-wise reference,
including all marker shifts, all glyph byte values and both glyph half-words,
all 16 fill inks, unchanged neighbours/guards and preserved ABI registers.
It also checks four selector mappings through the production LUT initializer,
including the byte-to-plane lookup required by compact terrain literal spans.
Reports and kernel binaries stay under `build/raster_cpu_test/`.
