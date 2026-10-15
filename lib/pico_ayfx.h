#ifndef PICO_AYFX_H
#define PICO_AYFX_H

#include <stdint.h>

/* AYFX sound effects (Shiru's format), via the vendored
 * lib/pico_ayfx_player.asm -- the alternative to lib/pico_sfx_synth.c
 * for a game that also plays real Arkos music (lib/pico_music_banked.c):
 * both pico_sfx_synth and AYFX need their own AY chip to avoid fighting
 * the music player for the shared mixer register (see
 * docs/sfx-convert-notes.md's full story) -- this module runs on the
 * Next's second AY chip (Turbosound) by default, same approach
 * pico_sfx_synth.c settled on.
 *
 * Requires an effects bank, converted from a real .afb file (authored
 * in Shiru's AYFX editor, or https://zx.remysharp.com/audio/) via
 * tools/afb_to_c.py -- a flat byte array this project links directly as
 * resident data, no parsing needed here (AFXPLAY parses its own format
 * at runtime, same relationship as any other vendored player). A game
 * provides its own `pico_ayfx_bank_data[]` symbol (whatever
 * tools/afb_to_c.py's own --symbol names it) -- this header doesn't
 * declare it, since its size varies per bank. */

/* Call once, after loading/linking an effects bank -- enables
 * Turbosound if not already on, zeroes the AY, resets channel state. */
void pico_ayfx_init(void);

/* play(effect) -- starts effect number `effect` (0-based, matching
 * however many effects the linked bank actually contains) on whichever
 * AY channel is free, or the longest-sounding one if all 3 are busy
 * (AYFX's own channel-stealing policy, not configurable). Returns
 * immediately -- doesn't block or play synchronously. */
void pico_ayfx_play(uint8_t effect);

/* Advances playback by one tick -- call once per frame (same spot
 * pico_music_banked_play_frame()/pico_sfx_synth_update() already get
 * called). */
void pico_ayfx_frame(void);

#endif
