#ifndef PICO_SFX_SYNTH_H
#define PICO_SFX_SYNTH_H

#include <stdint.h>

/* A tiny software synth that plays back Pico-8 sfx() data on the real
 * AY-3-8912, via tools/p8_sfx_to_c.py's converted output -- the automated
 * alternative to lib/pico_music.c's Arkos Tracker 2 playback (which needs
 * a human recomposing by ear). See docs/sfx-convert-notes.md for the full
 * reasoning, the pitch formula's own provenance (zepto8), and the
 * DELIBERATE fidelity losses this accepts to run on real AY hardware:
 * every Pico-8 waveform except noise collapses to a plain square tone (no
 * sine/triangle/sawtooth/ringing/phaser equivalent exists on the AY), and
 * per-note effects (slide/vibrato/drop/fade/arpeggio) are parsed by the
 * converter but not yet synthesized -- an affected note still plays, as a
 * static tone/noise at its own pitch and volume, just without the
 * effect's own pitch/volume motion over the note's duration. */

/* One AY-ready note: ay_period is the 12-bit tone period (psgT()'s own
 * formula, precomputed by the converter -- 0 is never emitted, a silent
 * note instead gets ay_volume=0 and ay_period=0, used only as the "this
 * note is a rest" marker, checked via ay_volume not ay_period). is_noise
 * selects the AY's noise generator instead of its tone generator for this
 * note (Pico-8 waveform 6 only -- every other waveform, including the
 * unsynthesized custom ones, plays as tone). */
typedef struct {
    uint16_t ay_period;
    uint8_t ay_volume;   /* 0-15, AY-native range (already scaled from Pico-8's 0-7) */
    uint8_t is_noise;
} pico_sfx_note_t;

/* One converted sfx: a fixed 32-note sequence, the per-sfx note duration
 * (already converted from Pico-8's 1/128-second units to this project's
 * own 50Hz tick count, so the player never needs to know the original
 * units), and a loop range (loop_end > loop_start means "loop", matching
 * the common loop_start==loop_end==0 default for a one-shot, non-looping
 * sfx -- see tools/p8_sfx_to_c.py's own header comment). */
typedef struct {
    const pico_sfx_note_t *notes;
    uint8_t note_count;      /* always 32 for a converted Pico-8 sfx */
    uint8_t duration_ticks;  /* per-note duration, in this project's 50Hz ticks */
    uint8_t loop_start;
    uint8_t loop_end;
} pico_sfx_t;

/* Starts `sfx` playing on AY channel `channel` (0-2), matching Pico-8's
 * own sfx(n, channel) convention. Overrides whatever was previously
 * playing on that channel. */
void pico_sfx_synth_play(const pico_sfx_t *sfx, uint8_t channel);

/* Advances all 3 channels by one tick -- call once per frame (same spot
 * pico_tick()/pico_input_update() already get called). A channel that
 * reaches the end of its sfx (no loop, or loop_end<=loop_start) silences
 * itself and stops advancing until the next pico_sfx_synth_play() call on
 * that channel. */
void pico_sfx_synth_update(void);

/* Silences channel `channel` immediately, without waiting for its current
 * note to finish. */
void pico_sfx_synth_stop(uint8_t channel);

#endif
