#include "pico_sfx_synth.h"
#include <psg.h>

typedef struct {
    const pico_sfx_t *sfx;
    uint8_t cur_note;
    uint8_t ticks_left;
    uint8_t playing;
} channel_state_t;

static channel_state_t ch_state[3];

/* psg_tone()/psg_volume() (lib psg.h) take a channel BITMASK (chan0=1,
 * chan1=2, chan2=4), not a 0-2 index -- same shift every call site below
 * needs, kept in one place. */
static uint8_t chan_bit(uint8_t channel) {
    return (uint8_t)(1u << channel);
}

/* psg_channels(tone_mask, noise_mask) sets the WHOLE mixer register at
 * once (lib psg.h) -- there's no incremental "just this channel" form, so
 * every state change recomputes the full mask from all 3 channels'
 * current state, not just the one channel that changed. */
static void update_channel_mask(void) {
    uint8_t tone_mask = 0, noise_mask = 0;
    uint8_t c;
    for (c = 0; c < 3; c++) {
        const pico_sfx_note_t *n;
        if (!ch_state[c].playing) continue;
        n = &ch_state[c].sfx->notes[ch_state[c].cur_note];
        if (n->ay_volume == 0) continue; /* a rest -- neither generator active */
        if (n->is_noise) noise_mask |= chan_bit(c);
        else tone_mask |= chan_bit(c);
    }
    psg_channels(tone_mask, noise_mask);
}

static void apply_note(uint8_t channel) {
    const pico_sfx_note_t *n = &ch_state[channel].sfx->notes[ch_state[channel].cur_note];
    if (n->ay_volume == 0) {
        psg_volume(chan_bit(channel), 0);
    } else if (n->is_noise) {
        psg_noise(n->ay_period);
        psg_volume(chan_bit(channel), n->ay_volume);
    } else {
        psg_tone(chan_bit(channel), n->ay_period);
        psg_volume(chan_bit(channel), n->ay_volume);
    }
    update_channel_mask();
}

void pico_sfx_synth_play(const pico_sfx_t *sfx, uint8_t channel) {
    if (channel > 2 || sfx->note_count == 0) return;
    ch_state[channel].sfx = sfx;
    ch_state[channel].cur_note = 0;
    ch_state[channel].ticks_left = sfx->duration_ticks;
    ch_state[channel].playing = 1;
    apply_note(channel);
}

void pico_sfx_synth_stop(uint8_t channel) {
    if (channel > 2) return;
    ch_state[channel].playing = 0;
    psg_volume(chan_bit(channel), 0);
    update_channel_mask();
}

void pico_sfx_synth_update(void) {
    uint8_t c;
    for (c = 0; c < 3; c++) {
        const pico_sfx_t *sfx;
        if (!ch_state[c].playing) continue;
        if (ch_state[c].ticks_left > 0) {
            ch_state[c].ticks_left--;
        } else {
            sfx = ch_state[c].sfx;
            if (sfx->loop_end > sfx->loop_start && ch_state[c].cur_note == sfx->loop_end) {
                ch_state[c].cur_note = sfx->loop_start;
            } else {
                ch_state[c].cur_note++;
            }
            if (ch_state[c].cur_note >= sfx->note_count) {
                ch_state[c].playing = 0;
                psg_volume(chan_bit(c), 0);
                update_channel_mask();
                continue;
            }
            ch_state[c].ticks_left = sfx->duration_ticks;
        }
        /* Reassert every tick, not just on a note change -- needed once
         * something else shares the AY chip (lib/pico_music_player.asm's
         * Arkos playback, which rewrites all 3 channels unconditionally
         * every frame): without this, a held note was only ever audible
         * for the single trigger frame before the music player's very
         * next tick silently reclaimed the channel, heard as a single
         * "click" instead of the real note duration (found the hard way
         * -- games/picodiver/, which runs both; games/sfxtest/, which
         * doesn't, never exposed this). Harmless when nothing else is
         * sharing the chip -- just a few redundant identical writes. */
        apply_note(c);
    }
}
