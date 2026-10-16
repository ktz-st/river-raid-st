# Building River Raid STE

This is a native Atari STE game: C and Motorola 68000 assembly using GodLib.
The target is a 512 KiB STE, 8 MHz, PAL 50 Hz. There is no PC build.
Read `README.md` and the root `Makefile` before changing build settings.

## Toolchain

Use the **MiNT ELF cross-toolchain**, not the host compiler, generic
`m68k-elf-gcc`, or the older `m68k-atari-mint-gcc` a.out toolchain.
The verified compiler is:

```text
m68k-atari-mintelf-gcc (MiNT ELF 20250810) 15.2.0
GNU ld (GNU Binutils for MiNT ELF 20250812) 2.45.0
```

Use this version for a known-working baseline. Other releases have not been
qualified; their version number alone does not guarantee the required ABI.
The installation must include `m68k-atari-mintelf-{gcc,ld,ar,nm,strip}` and the
`mshort/mfastcall` libgcc multilib. Put all tools on `PATH`.

Additional build tools, with versions used locally:

- `vasmm68k_mot` 2.0c, Motorola syntax, ELF output (`-devpac -Felf`).
- UPX 5.2.1 with Atari/TOS support, for the default packed release.
- GNU Make, Git, Python 3 and Pillow (verified: Python 3.14.7, Pillow 12.3.0).

The game ABI flags are `-m68000 -mshort -mfastcall -fpack-struct=2`.
Do not remove them or substitute libraries built for a different calling
convention. `-mshort` means 16-bit `int`; pointers and `long` remain 32-bit.

Check the installation before building:

```sh
m68k-atari-mintelf-gcc --version
m68k-atari-mintelf-ld --version
m68k-atari-mintelf-gcc -m68000 -mshort -mfastcall -print-multi-directory
m68k-atari-mintelf-gcc -m68000 -mshort -mfastcall -print-libgcc-file-name
upx --version
python3 -c "import PIL; print(PIL.__version__)"
```

The multilib directory should be `mshort/mfastcall`; the libgcc path must
resolve to an existing file in that multilib, not the default 32-bit-int ABI.

## Dependency Layout

Clone these forks beside the game, explicitly using the names below:

```sh
# Run in the parent directory containing river-raid-st/.
git clone https://github.com/ktz-st/godlib.elf.git godlib
git clone https://github.com/ktz-st/libcmini.elf.git libcmini
```

Expected layout:

```text
workspace/
  godlib/          # headers, C/assembly sources, libgod.a, Makefile
  libcmini/        # include/, lib/crt0.o, lib/mshort/mfastcall/libcmini.a
  river-raid-st/   # this project
```

Do not leave the GodLib clone named `godlib.elf`: includes use
`<godlib/...>` through `-I..`. `GODLIB_DIR` overrides library/source lookup,
but does not by itself fix that header layout. Prefer the layout above.
Do not overwrite or re-clone existing dependency directories with local changes.

The libcmini fork ships headers and prebuilt libraries; it has no Makefile.
Do not run `make` there. GodLib can be built from the parent directory with:

```sh
make -C godlib -j4
test -f godlib/libgod.a
test -f libcmini/lib/crt0.o
test -f libcmini/lib/mshort/mfastcall/libcmini.a
```

The game requires GodLib source files as well as `libgod.a`. Its build script
creates a private trimmed STE subset in `build/godlib/`, checking that original
sprite and VBL assembly instructions are preserved. Do not modify shared
GodLib sources just to trim the game.

## Build Inputs And Commands

Install Pillow in your Python environment. For example, from the workspace
parent, keeping the environment outside the game source tree:

```sh
python3 -m venv .venv-rr-build
. .venv-rr-build/bin/activate
python3 -m pip install Pillow==12.3.0
```

The original cartridge is a required **local** build input:
`river-raid-st/assets/RIVER_A8.BIN`, exactly 8192 bytes, SHA-256:

```text
dc55bef6fa336003772643e68b9bfade7155ff4dde1aad814e1cf56ad818805f
```

It is ignored by Git and is not shipped with the release. Ask the user to
provide their cartridge dump if absent; do not invent data or silently download
a ROM. The generator/tables and PSG conversion still require it even though
sprite and scenery graphics now come from editable PNG atlases.

From the game directory:

```sh
make -j4 verify     # release, matching debug program and static verification
make               # release only
make debug         # unstripped debug program and symbols
```

Outputs: `release/RR.PRG` is stripped and UPX-packed; `build/RR.PRG` retains
symbols; `build/RR.RAW` is the stripped, unpacked program, not a raw memory dump.
Maps are `build/RR.map` and `build/RR.debug.map`. All generated assets, reports,
objects and private library intermediates belong under `build/`.
Only `RR.PRG` is needed on the Atari; do not add AUTO copies or runtime assets.

`make verify` checks original asset hashes and current release/link/layout
invariants. Intentional graphics edits can change those historical hashes:
investigate and document the difference, never update baselines just to hide it.
Changing compiler or UPX versions can change executable bytes without changing
behaviour. Do not claim byte-identical output without comparing the files.
`make clean` deletes `build/` and `release/RR.PRG`, including local test captures.

## Target Verification

Hatari is optional for compilation. `make run` uses `hatari` on `PATH`:

```sh
hatari --machine ste --memsize 0 --monitor rgb --confirm-quit false release/RR.PRG
```

`--memsize 0` means 512 KiB. Use a 68000 at 8 MHz and the default TOS 2.06
configuration for comparisons; do not mask bugs by adding RAM or turbo speed.
Always keep `--confirm-quit false`.

The automated `make smoke-hatari`, `make verify-hatari` and
`python3 -B scripts/verify_hatari.py --title-fades` additionally require the
local hrdb-enabled Hatari and helper scripts referenced in
`scripts/verify_hatari.py`. They are not portable prerequisites of `make`.
Do not report emulator tests as passed if these helpers are unavailable.

Preserve indexed STE PNG palette order, transparency, sprite masks and preshifts.
Never regenerate editable source atlases with the one-time bootstrap/import
scripts during an ordinary build. Keep YM-only audio, existing controls,
checkpoint behaviour and the 512 KiB target unless the user requests changes.
