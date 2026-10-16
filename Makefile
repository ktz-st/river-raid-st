.DEFAULT_GOAL := all
.DELETE_ON_ERROR:
CC = m68k-atari-mintelf-gcc
LD = m68k-atari-mintelf-gcc
AS = vasmm68k_mot
STRIP = m68k-atari-mintelf-strip
UPX ?= upx
PYTHON ?= python3
export PYTHONDONTWRITEBYTECODE = 1

GODLIB_DIR ?= ../godlib
CMINI_DIR ?= ../libcmini
ROM = assets/RIVER_A8.BIN
GEN = build/generated
PATHS = scripts/build_paths.py
PALETTE = assets/st_palette.py

CFLAGS = -Wall -Os -mshort -mfastcall -m68000 -fpack-struct=2 \
 -Isrc -Iassets -I$(GEN) -I$(CMINI_DIR)/include -I.. \
 -fno-jump-tables -fno-tree-switch-conversion -fomit-frame-pointer \
 -fno-unwind-tables -fno-asynchronous-unwind-tables -fmerge-constants -fno-ident \
 -fno-inline-functions -fno-inline-small-functions -DdGODLIB_FADE -MMD -MP
LDFLAGS = -nostdlib -nostartfiles -nodefaultlibs -static -mshort -mfastcall -m68000 \
 -Wl,--gc-sections -Wl,--relax -L$(CMINI_DIR)/lib/mshort/mfastcall -L$(GODLIB_DIR)
LIBS = -Wl,--start-group -lgod -lcmini -lgcc -Wl,--end-group
CRT = $(CMINI_DIR)/lib/crt0.o

# Keep the established link order for predictable layout and profiling.
C_NAMES = modee split player enemies bank gen_direct cold_init native_assets \
 audio audio_psg audio_psg_data hud_masks ste_graphic ste_platform title title_assets palette
S_NAMES = split_s gen_sparse_m68k expand_line planar_line copyrow checkcopy \
 restore_cpu bank_cpu shot_cpu shot_terrain hud_cpu sprite_cpu road_cpu mask_overlap psg_lock
OBJECTS = $(addprefix build/,$(addsuffix .o,$(C_NAMES) $(S_NAMES))) \
 build/rr_512.o build/stack_512.o
HEADERS = $(wildcard src/*.h) assets/hud_assets.inc $(GEN)/native_assets.h $(GEN)/title_assets.h
GENERATED_C = native_assets audio_psg_data hud_masks title_assets
GENERATED_S = gen_sparse_m68k sprite_cpu road_cpu

.PHONY: all debug clean run verify verify-hatari smoke-hatari
all: release/RR.PRG
debug: build/RR.PRG

$(GEN)/native_assets.c: scripts/build_native_assets.py scripts/png_assets.py scripts/rom_data_layout.py $(PATHS) $(PALETTE) $(ROM) assets/sprites.png assets/sprites.json assets/scenery.png assets/scenery.json
	$(PYTHON) scripts/build_native_assets.py
$(GEN)/native_assets.h: $(GEN)/native_assets.c
	@test -f $@ || $(PYTHON) scripts/build_native_assets.py
$(GEN)/title_assets.c: assets/title.png scripts/build_title.py $(PATHS)
	$(PYTHON) scripts/build_title.py
$(GEN)/title_assets.h: $(GEN)/title_assets.c
	@test -f $@ || $(PYTHON) scripts/build_title.py
$(GEN)/audio_psg_data.c: scripts/build_psg.py $(PATHS) $(ROM)
	$(PYTHON) scripts/build_psg.py
$(GEN)/hud_masks.c: scripts/build_hud.py $(PATHS) assets/hud_assets.inc
	$(PYTHON) scripts/build_hud.py
$(GEN)/gen_sparse_m68k.s: scripts/build_generator_m68k.py scripts/lift_generator.py scripts/rom_data_layout.py $(PATHS) $(ROM)
	$(PYTHON) scripts/build_generator_m68k.py --compact --sparse
$(GEN)/sprite_cpu.s: scripts/build_sprite_cpu.py $(PATHS)
	$(PYTHON) scripts/build_sprite_cpu.py
$(GEN)/road_cpu.s: scripts/build_road_cpu.py $(PATHS)
	$(PYTHON) scripts/build_road_cpu.py

build/%.o: src/%.c $(HEADERS)
	@mkdir -p build
	$(CC) $(CFLAGS) -c $< -o $@
$(addprefix build/,$(addsuffix .o,$(GENERATED_C))): build/%.o: $(GEN)/%.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@
build/rr_512.o: src/rr.c $(HEADERS)
	$(CC) $(CFLAGS) -DRR_LOW_MEMORY -c $< -o $@
build/%.o: src/%.s
	@mkdir -p build
	$(AS) -nosym -devpac -Felf $< -o $@
$(addprefix build/,$(addsuffix .o,$(GENERATED_S))): build/%.o: $(GEN)/%.s
	$(AS) -nosym -devpac -Felf $< -o $@

build/godlib/linked.o: $(OBJECTS) scripts/build_godlib_subset.py $(GODLIB_DIR)/libgod.a Makefile
	$(PYTHON) scripts/build_godlib_subset.py --godlib $(GODLIB_DIR) --cmini $(CMINI_DIR) $(OBJECTS)
build/RR.RAW: build/godlib/linked.o Makefile
	$(LD) -s $(LDFLAGS) -Wl,-Map=build/RR.map -o $@ $<
	$(STRIP) --strip-unneeded $@
release/RR.PRG: build/RR.RAW
	@mkdir -p release
	$(UPX) -9 --force-overwrite -o $@ $<
	$(UPX) -t $@
build/RR.PRG: build/godlib/linked.o Makefile
	$(LD) $(LDFLAGS) -Wl,-Map=build/RR.debug.map -o $@ $<

verify: release/RR.PRG build/RR.PRG
	$(PYTHON) scripts/verify_migration.py
verify-hatari: release/RR.PRG build/RR.PRG
	$(PYTHON) scripts/verify_hatari.py
smoke-hatari: release/RR.PRG build/RR.PRG
	$(PYTHON) scripts/verify_hatari.py --quit-only
run: release/RR.PRG
	hatari --machine ste --memsize 0 --monitor rgb --confirm-quit false $<
clean:
	rm -rf build
	rm -f release/RR.PRG

-include $(wildcard build/*.d)
