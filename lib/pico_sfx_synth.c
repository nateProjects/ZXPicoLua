#include "pico_sfx_synth.h"
#include <psg.h>
#include <arch/zxn.h>

typedef struct {
    const pico_sfx_t *sfx;
    uint8_t cur_note;
    uint8_t ticks_left;
    uint8_t playing;
} channel_state_t;

static channel_state_t ch_state[3];

/* Runs on the Next's SECOND AY chip (Turbosound), not the first one
 * lib/pico_music_player.asm's Arkos playback uses -- found the hard way
 * (docs/sfx-convert-notes.md) that sharing ONE chip doesn't work: both
 * this module and the Arkos player rewrite the whole mixer register
 * (AY R7) unconditionally every tick, each blind to what the other
 * wants on channels it doesn't itself control, producing audible
 * on/off gating ("a series of clicks") on whichever channels lost that
 * frame's race. A real, documented AY-sfx-vs-tracker-music conflict,
 * not unique to this project -- even Shiru's own AYFX driver (for
 * NextBASIC, read directly: /Users/nat/Code/Games/ayfxResources/
 * next-ayfx-main/src/ayfx.asm) has the identical limitation on a shared
 * chip, acknowledged in its own README as "can sometimes become
 * crunchy." The real fix either system needs is what AYFX's own driver
 * ALSO supports but defaults away from: a genuinely separate physical
 * AY chip, not register-level coordination (which would need
 * cooperation from the vendored, frozen-on-purpose Arkos player to do
 * safely). z88dk's own <arch/zxn.h> exposes the Next's Turbosound
 * chip-select port (IO_TURBOSOUND) -- its own symbolic values for WHICH
 * chip to select turned out wrong, see the comment by
 * TURBOSOUND_SELECT_PSG_0/1 below -- select chip 1 before this module's
 * own writes, restore chip 0 immediately after, every time. The vendored
 * Arkos
 * player itself needs no changes: it never selects a chip explicitly
 * (written before Turbosound was a consideration), so it always targets
 * whatever chip is CURRENTLY selected when it runs -- as long as this
 * module always restores chip 0 before returning, Arkos's own next
 * per-frame write still lands on chip 0 exactly as before. */
static uint8_t turbosound_ready = 0;

static void ensure_turbosound(void) {
    if (turbosound_ready) return;
    ZXN_WRITE_REG(REG_PERIPHERAL_3,
                  (uint8_t)(ZXN_READ_REG(REG_PERIPHERAL_3) | RP3_ENABLE_TURBOSOUND));
    turbosound_ready = 1;
}

/* z88dk's own <arch/zxn.h> symbolic constants for this (IT_SELECT_PSG_0/
 * 1/2 = 0x0F/0x0E/0x0D) turned out NOT to work -- confirmed the hard way,
 * not assumed: selecting with those values left sfx audibly conflicting
 * with music exactly as before (still "just a click"), meaning the
 * chip-select never actually took effect; those values read as ordinary
 * (unused, harmless) AY register-select indices 13-15, not as the
 * special chip-select command. The real protocol needs the upper nibble
 * SET, not clear (confirmed against this project's own prior research,
 * `z88dk_banking_reference`-adjacent findings noting the exact bit
 * pattern `%11111101`/`%11111110`, and cross-checked against outside
 * documentation): write $FF/$FE/$FD to select chip 0/1/2. Using these
 * literal values directly rather than z88dk's own (apparently wrong)
 * symbols. */
#define TURBOSOUND_SELECT_PSG_0 0xFF
#define TURBOSOUND_SELECT_PSG_1 0xFE

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
    ensure_turbosound();
    IO_TURBOSOUND = TURBOSOUND_SELECT_PSG_1;
    ch_state[channel].sfx = sfx;
    ch_state[channel].cur_note = 0;
    ch_state[channel].ticks_left = sfx->duration_ticks;
    ch_state[channel].playing = 1;
    apply_note(channel);
    IO_TURBOSOUND = TURBOSOUND_SELECT_PSG_0;
}

void pico_sfx_synth_stop(uint8_t channel) {
    if (channel > 2) return;
    ensure_turbosound();
    IO_TURBOSOUND = TURBOSOUND_SELECT_PSG_1;
    ch_state[channel].playing = 0;
    psg_volume(chan_bit(channel), 0);
    update_channel_mask();
    IO_TURBOSOUND = TURBOSOUND_SELECT_PSG_0;
}

void pico_sfx_synth_update(void) {
    uint8_t c;
    uint8_t any_active = 0;
    for (c = 0; c < 3; c++) {
        if (ch_state[c].playing) { any_active = 1; break; }
    }
    /* Nothing playing -- skip touching Turbosound's chip-select state
     * entirely, not just the AY registers (cheap, and avoids any
     * window, however brief, where chip 1 stays selected for no
     * reason). */
    if (!any_active) return;

    IO_TURBOSOUND = TURBOSOUND_SELECT_PSG_1;
    for (c = 0; c < 3; c++) {
        const pico_sfx_t *sfx;
        if (!ch_state[c].playing) continue;
        if (ch_state[c].ticks_left > 0) {
            ch_state[c].ticks_left--;
            continue;
        }
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
        apply_note(c);
    }
    IO_TURBOSOUND = TURBOSOUND_SELECT_PSG_0;
}
