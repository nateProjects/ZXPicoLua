# ZXPicoLua -- Pico-8-style C library for the ZX Spectrum Next
# Toolchain: z88dk (local checkout), classic clib, +zxn target, .nex output
# Modeled on dark-room-next/Makefile.
#
# Builds one game at a time, selected via GAME (default: demo) --
# `make`/`make run` build/run games/demo, `make GAME=picodiver run` builds/
# runs games/picodiver. Each game is its own directory under games/,
# always linked against the full lib/ library.

Z88DK_DIR := /Users/nat/Code/Games/z88dk
export PATH := $(Z88DK_DIR)/bin:$(PATH)
export ZCCCFG := $(Z88DK_DIR)/lib/config/

ZCC := $(Z88DK_DIR)/bin/zcc

GAME       ?= demo
LIB_DIR    := lib
GAME_DIR   := games/$(GAME)
BUILD_DIR  := build/$(GAME)
SDCARD_DIR := sdcard
TARGET     := $(BUILD_DIR)/$(GAME)

LIB_SRCS := $(wildcard $(LIB_DIR)/*.c)
# pico_mul_z80n.asm (2026-10-12): hardware-mul override for classic
# clib's own generic l_mult/l_mult_u/l_mult_0 -- see that file's own
# top comment. Passed as a raw source alongside LIB_SRCS so it links
# unconditionally for every game, same as any lib/*.c module.
LIB_ASM_SRCS := $(wildcard $(LIB_DIR)/*.asm)
HEADERS := $(wildcard $(LIB_DIR)/*.h) $(wildcard $(GAME_DIR)/*.h)

# --- Banked modules (per-game, via lib/pico_bank.h) ---
# A game lists its own large/rarely-touched const-data .c files (under
# GAME_DIR, no .c suffix) in BANKED_CSRCS and gives each a CBANK_<name>
# physical page number -- see picodiver's own block below for a user.
# Mechanism ported verbatim from dark-room-next's
# Makefile (itself verified against real shipped Next games, see
# z88dk-banking-notes.md): compile the .c to .asm separately, then
# text-substitute the compiler's default section names for a
# PAGE_<n>_<name> section z88dk's own nex packager places directly at
# physical page <n> -- NOT `#pragma bank N`, which that same
# investigation found silently computes the wrong address.
BANKED_CSRCS :=
BANKED_ASMS = $(patsubst %,$(BUILD_DIR)/%.asm,$(BANKED_CSRCS))

$(BUILD_DIR)/%.asm: $(GAME_DIR)/%.c $(HEADERS) | $(BUILD_DIR)
	$(ZCC) +zxn -clib=classic -subtype=nex -I$(LIB_DIR) -I$(GAME_DIR) -a $< -o $@
	python3 -c "\
import sys; \
path, section = sys.argv[1], sys.argv[2]; \
txt = open(path).read(); \
txt = 'SECTION ' + section + chr(10) + 'ORG \$$2000' + chr(10) + txt; \
txt = txt.replace('code_compiler', section).replace('rodata_compiler', section).replace('data_compiler', section).replace('bss_compiler', section); \
open(path, 'w').write(txt)" \
		$@ PAGE_$(CBANK_$*)_$*

# Per-game opt-outs for library features a specific game doesn't use, to
# recover resident-size budget under z88dk classic's 64K limit (see
# pico_map.c's own comment on PICO_NO_MAP_FLAGS) -- confirmed by grep,
# not assumed, that picodiver never calls fget/fset/map_layer(), nor
# pico_data.c's cartdata/dget/dset (excluded below, along with the
# -lesxdos it alone needs -- both real, measured bytes this program has
# essentially none to spare of, docs/picodiver-notes.md). pico_map.c
# itself is excluded too, not just its flags -- picodiver's own
# background moved from software pico_map() to the hardware tilemap
# (lib/pico_tilemap.c) back in its own Stage 2, and pico_map()/pico_mget/
# pico_mset/pico_map_init have had zero call sites in games/picodiver/
# main.c ever since (confirmed by grep); the Stage 9 death/win screens'
# extra text pushed this build's resident size just over 64K, and this
# whole file was dead weight the linker wasn't dropping on its own.
ifeq ($(GAME),picodiver)
GAME_CFLAGS := -DPICO_NO_MAP_FLAGS
# pico_hwsprite.c excluded too (2026-10-12): zero call sites in
# games/picodiver/main.c (confirmed by grep) -- picodiver's own objects
# are all software Layer-2 blits via pico_sprite.c, never Next hardware
# sprites. Harmless dead weight when this file was small; grew a lot
# during Islander's own later work (16x16 pattern support, magnification,
# etc.) and tipped picodiver's resident build over the 64K ceiling --
# confirmed by reproducing the SAME overflow with this file still
# in and an unrelated new source (pico_mul_z80n.asm) pulled back out,
# ruling that new file out before looking here.
LIB_SRCS := $(filter-out $(LIB_DIR)/pico_data.c $(LIB_DIR)/pico_map.c $(LIB_DIR)/pico_hwsprite.c,$(LIB_SRCS))
ESXDOS_LIB :=
# picodiver_map_data.c banked (2026-10-12) -- see picodiver_data.h's own
# PICODIVER_MAP_PHYSICAL_PAGE comment. Recovers 2048 bytes, comfortably
# clearing the 669-byte overflow this build hit once pico_hwsprite.c
# alone wasn't enough.
BANKED_CSRCS := picodiver_map_data
CBANK_picodiver_map_data := 8
else
ESXDOS_LIB := -lesxdos
endif

# Banked .c files are compiled separately into BANKED_ASMS (above) and
# linked via that .asm, not as an ordinary resident source -- excluded
# here so they're not ALSO compiled/linked the normal way too.
GAME_SRCS := $(filter-out $(patsubst %,$(GAME_DIR)/%.c,$(BANKED_CSRCS)),$(wildcard $(GAME_DIR)/*.c))
SRCS := $(LIB_SRCS) $(LIB_ASM_SRCS) $(GAME_SRCS)

# ZEsarUX changes its cwd to inside its own app bundle at launch, so paths
# passed to it must be absolute, not relative (see dark-room-next's
# Makefile/docs for the same gotcha).
ABS_BUILD_DIR := $(abspath $(BUILD_DIR))
ABS_SDCARD_DIR := $(abspath $(SDCARD_DIR))
ZESARUX := /Applications/ZEsarUX.app/Contents/MacOS/zesarux

.PHONY: all clean clean-all run

all: $(TARGET).nex

# -lesxdos: pico_data.c (dget/dset/cartdata) needs the low-level
# esxdos_f_* calls -- same reasoning as dark-room-next's save.c, see
# pico_data.c's own header comment.
$(TARGET).nex: $(SRCS) $(HEADERS) $(BANKED_ASMS) | $(BUILD_DIR)
	$(ZCC) +zxn -clib=classic -subtype=nex \
		-I$(LIB_DIR) -I$(GAME_DIR) \
		-O3 $(GAME_CFLAGS) \
		$(SRCS) $(BANKED_ASMS) \
		$(ESXDOS_LIB) \
		-o $(TARGET) -create-app -m

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# --nex-no-automount-esxdos + --esxdos-root-dir: needed for any game that
# touches the SD card (pico_data.c) -- see dark-room-next's own
# Makefile/docs/layer2-notes.md for why both flags are required together
# (ZEsarUX silently auto-mounts the .nex's own directory otherwise, which
# --esxdos-root-dir alone doesn't override).
run: $(TARGET).nex
	mkdir -p $(SDCARD_DIR)
	$(ZESARUX) --noconfigfile --machine tbblue --enabletimexvideo \
		--tbblue-fast-boot-mode \
		--nex-no-automount-esxdos \
		--enable-esxdos-handler --esxdos-root-dir $(ABS_SDCARD_DIR) \
		--zoom 1 \
		$(ABS_BUILD_DIR)/$(GAME).nex

clean:
	rm -rf $(BUILD_DIR)

clean-all:
	rm -rf build
