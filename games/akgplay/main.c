/* A minimal, dedicated Arkos (.aks) song player -- no gameplay, no
 * sprites, nothing in the per-frame loop but the real banked Arkos
 * player (lib/pico_music_banked.c/.h, the same path PicoDiver's own
 * background music uses) and a visual tick counter. Built so a song
 * can be previewed at its true tempo without any game's own gameplay
 * loop as a confound -- useful both for checking a new composition
 * sounds right before wiring it into a real game, and for isolating
 * "is the player/pipeline itself correct" from "does THIS game's own
 * per-frame budget leave enough headroom," two genuinely different
 * questions this project's own PicoDiver work needed to tell apart
 * more than once (docs/music-notes.md).
 *
 * The on-screen counters are the whole point: `frame` ticks once per
 * intrinsic_halt() (one real video frame), and `seconds` increments
 * once every 50 of those -- if the Next is genuinely running at its
 * intended speed, `seconds` should visibly advance once per real
 * wall-clock second (watch it against a real clock/stopwatch). If it
 * visibly drifts slower than real seconds, that's a real frame-rate
 * problem worth chasing (same technique that caught v0.10.2's own
 * CPU-speed regression) -- not something to assume from how the music
 * sounds alone.
 *
 * To preview a DIFFERENT song: convert it with
 * tools/arkos_song_to_asm.py (see docs/music-notes.md's own workflow),
 * point the Makefile's `BANKED_MUSIC_SONG` (this game's own `ifeq`
 * block) at the new .asm, and update AKGPLAY_SONG_SYMBOL below to
 * match the `--symbol` used. Defaults to the same committed
 * sailing-by.aks content PicoDiver uses (games/picodiver/
 * sailing_by_song.asm, reused directly, not copied) -- the only real
 * Arkos content currently committed to this project. */

#include <intrinsic.h>
#include <stdint.h>
#include "pico_video.h"
#include "pico_gfx.h"
#include "pico_math.h"
#include "pico_music_banked.h"

#define AKGPLAY_MUSIC_PAGE 14
extern uint8_t sailing_by_Start[]; /* AKGPLAY_SONG_SYMBOL -- see top comment to swap */

static uint16_t frame = 0;
static uint16_t seconds = 0;

static void draw(void) {
    char buf[8];
    pico_cls(0);
    pico_print("AKG PLAYER", 2, 2, 7);
    pico_print("frame", 2, 24, 6);
    pico_tostr_int((int16_t)frame, buf);
    pico_print(buf, 60, 24, 6);
    pico_print("seconds", 2, 36, 6);
    pico_tostr_int((int16_t)seconds, buf);
    pico_print(buf, 60, 36, 6);
    pico_print("(watch seconds vs a", 2, 56, 5);
    pico_print("real clock)", 2, 64, 5);
}

/* Redrawing the UI (pico_cls() + print()s) is NOT free -- measured at
 * ~32ms/call for pico_cls() alone even after picodiver's own batching
 * fixes (docs/picodiver-notes.md), a real, already-documented sccz80
 * codegen cost, not a bug. Doing that every single halt was this tool's
 * OWN bottleneck, not the music player's: an earlier investigation here
 * mistook the resulting ~6-9fps for a real Arkos playback slowdown on a
 * demanding song, when isolating play_frame() alone (no UI redraw) measured
 * a genuine ~50-60fps -- the player was always fine. Since `seconds` only
 * changes once every 50 frames anyway, the UI only needs to repaint that
 * often; play_frame() still runs every single halt, at full real tempo. */
#define AKGPLAY_UI_REDRAW_INTERVAL 50

int main(void) {
    intrinsic_ei();
    pico_video_init(128, 128);
    pico_music_banked_init(AKGPLAY_MUSIC_PAGE, sailing_by_Start, 0);

    for (;;) {
        intrinsic_halt();
        pico_music_banked_play_frame(AKGPLAY_MUSIC_PAGE);
        frame++;
        if (frame % AKGPLAY_UI_REDRAW_INTERVAL == 0) {
            seconds++;
            draw();
            pico_flip();
        }
    }
}
