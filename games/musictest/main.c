/* Proves lib/pico_music.c itself (not just the raw <psg/arkos.h> it
 * wraps) compiles and links cleanly through this project's own build
 * pipeline, opted in via the Makefile's GAME=musictest block (pico_music.c
 * is excluded from every other game by default -- see the Makefile's own
 * LIB_SRCS comment for why). No real Arkos Tracker 2 content has been
 * authored for this project yet (see docs/music-notes.md), so this only
 * calls pico_music_stop() -- the one entry point that needs no song data
 * (it just silences the AY channels) -- rather than claiming full
 * playback is verified when it isn't. */

#include <intrinsic.h>
#include <stdint.h>
#include "pico_video.h"
#include "pico_gfx.h"
#include "pico_music.h"

static void draw_all(void) {
    pico_cls(0);
    pico_print("PICO_MUSIC.C LINKED OK", 2, 40, 7);
    pico_print("(no song content yet)", 2, 50, 6);
}

int main(void) {
    intrinsic_ei();
    pico_video_init(128, 128);

    pico_music_stop();

    draw_all();
    pico_flip();
    draw_all();

    for (;;) intrinsic_halt();
}
