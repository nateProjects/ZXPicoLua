/* Proves a converted Pico-8 sfx actually reaches the AY chip through
 * lib/pico_sfx_synth.c -- not just "the converter ran and produced
 * plausible-looking numbers". sfx_data.c is a small, self-authored
 * synthetic test sfx (an 8-note ascending blip) converted via
 * tools/p8_sfx_to_c.py -- the converter itself was verified separately
 * against a real cart (celeste.p8, see docs/sfx-convert-notes.md), but
 * that cart's own content isn't committed here (same reasoning as this
 * project's own removal of games/islander/ -- a ported cart's content
 * belongs to its own author, not this repo).
 *
 * Replays blip_sfx_0 every 2 seconds, forever, no video -- a pure-audio
 * throwaway, verified via `zesarux --aofile` waveform capture
 * (dark-room-next's own proven method, that project's docs/sound-notes.md),
 * same ceiling as every other AY verification in this project: confirms
 * real, controllable (non-silent during playback, silent between
 * notes/after the sfx ends) AY output, not pitch-accuracy-by-ear. See
 * docs/sfx-convert-notes.md. */

#include <intrinsic.h>
#include <stdint.h>
#include <psg.h>
#include "pico_sfx_synth.h"

extern const pico_sfx_t blip_sfx_0;

int main(void) {
    uint16_t frame = 0;

    intrinsic_ei();
    psg_init();

    for (;;) {
        intrinsic_halt();
        if (frame % 100 == 0) {
            pico_sfx_synth_play(&blip_sfx_0, 0);
        }
        pico_sfx_synth_update();
        frame++;
    }
}
