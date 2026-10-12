#include "pico_music.h"
#include <psg/arkos.h>

void pico_music_init(const void *song, uint16_t subsong) {
    ply_akg_init((void *)song, subsong);
}

void pico_music_play_frame(void) {
    ply_akg_play();
}

void pico_music_stop(void) {
    ply_akg_stop();
}

void pico_sfx_init(void **effects_table) {
    ply_akg_initsoundeffects(effects_table);
}

void pico_sfx_play(uint16_t effect, uint16_t channel, uint16_t inv_volume) {
    ply_akg_playsoundeffect(effect, channel, inv_volume);
}
