#ifndef PICO_MUSIC_H
#define PICO_MUSIC_H

#include <stdint.h>

/* music(n) / sfx(n) -- thin wrapper around z88dk's own bundled Arkos
 * Tracker 2 player (<psg/arkos.h>, target/zx/psg/asm_arkos_zx_smc.asm) --
 * confirmed linking cleanly under THIS project's exact build target
 * (-clib=classic +zxn, docs/music-notes.md), the same "z88dk already has
 * this, don't duplicate it" discovery dark-room-next's own <psg.h> spike
 * made for plain PSG tones.
 *
 * Deliberate deviation from Pico-8's own music()/sfx(): Pico-8's audio is
 * wholly SYNTHESIZED from a cart's own __sfx__/__music__ data at runtime.
 * This instead plays back pre-composed Arkos Tracker 2 (.aks) song/
 * sound-effect data (exported to C via z88dk's own
 * support/arkos/arkos-song-to-asm.pl), not an automated byte-for-byte
 * conversion of a cart's own Pico-8 audio data -- see docs/music-notes.md
 * for why (the AY chip's waveform/effect ceiling makes faithful
 * synthesis-level conversion a materially bigger, separate undertaking;
 * a real shipped Pico-8-to-Next port, CelesteNext, took this same
 * recompose-in-Arkos path rather than converting). A ported cart's own
 * music(n)/sfx(n) call sites become pico_music_init()/pico_sfx_play() at
 * the same points, fed by hand-authored Arkos data, not a converter's
 * output.
 *
 * Credit: the Arkos Tracker 2 player itself is (c) Julien "Targhan" Nevo
 * (https://www.julien-nevo.com/arkostracker/) -- z88dk bundles and
 * maintains the Z80/sccz80 port of it (support/arkos/, credited there to
 * ZXjogv). Nothing from the Arkos player's own source is vendored here;
 * this header just wraps the C API z88dk already ships
 * (<psg/arkos.h>). */

/* Call once, after loading a song (an exported Arkos .asm's own data
 * symbol, e.g. `extern uint8_t my_song[];`) -- subsong selects one of
 * several songs/variations authored together in Arkos Tracker 2 (0 if
 * the song has only one). */
void pico_music_init(const void *song, uint16_t subsong);

/* Advances music playback by one tick -- call once per frame (same spot
 * pico_input_update()/pico_tick() already get called), NOT inside your
 * own pico_bank_page_in()/page_out() pair if the song/player data is
 * banked -- see docs/music-notes.md's banking note (CelesteNext's own
 * sfx_update() pages in its PAGE_54_MUSIC around this exact call). */
void pico_music_play_frame(void);

/* Silences music playback (does not reset playback position -- call
 * pico_music_init() again to restart from the beginning). */
void pico_music_stop(void);

/* Registers a table of sound-effect data pointers (an exported Arkos
 * .asm's own effects-table symbol) -- call once, before any
 * pico_sfx_play(). */
void pico_sfx_init(void **effects_table);

/* Plays sound effect `effect` (1-based, matching Arkos Tracker 2's own
 * numbering) on AY channel `channel` (0-2), overriding whatever music is
 * currently playing on that channel for the effect's own duration.
 * inv_volume is INVERTED (0 = full volume, 15 = silent) -- Arkos
 * Tracker 2's own convention, not Pico-8's. */
void pico_sfx_play(uint16_t effect, uint16_t channel, uint16_t inv_volume);

#endif
