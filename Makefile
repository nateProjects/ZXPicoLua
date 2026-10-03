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

SRCS := $(wildcard $(LIB_DIR)/*.c) $(wildcard $(GAME_DIR)/*.c)
HEADERS := $(wildcard $(LIB_DIR)/*.h) $(wildcard $(GAME_DIR)/*.h)

# ZEsarUX changes its cwd to inside its own app bundle at launch, so paths
# passed to it must be absolute, not relative (see dark-room-next's
# Makefile/docs for the same gotcha).
ABS_BUILD_DIR := $(abspath $(BUILD_DIR))
ABS_SDCARD_DIR := $(abspath $(SDCARD_DIR))
ZESARUX := /Applications/ZEsarUX.app/Contents/MacOS/zesarux

.PHONY: all clean clean-all run

all: $(TARGET).nex

# -lesxdos: M3's pico_data.c (dget/dset/cartdata) needs the low-level
# esxdos_f_* calls -- same reasoning as dark-room-next's save.c, see
# pico_data.c's own header comment.
$(TARGET).nex: $(SRCS) $(HEADERS) | $(BUILD_DIR)
	$(ZCC) +zxn -clib=classic -subtype=nex \
		-I$(LIB_DIR) -I$(GAME_DIR) \
		$(SRCS) \
		-lesxdos \
		-o $(TARGET) -create-app -m

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# --nex-no-automount-esxdos + --esxdos-root-dir: needed for any game that
# touches the SD card (M3's pico_data.c) -- see dark-room-next's own
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
