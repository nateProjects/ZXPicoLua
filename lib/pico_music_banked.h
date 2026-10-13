#ifndef PICO_MUSIC_BANKED_H
#define PICO_MUSIC_BANKED_H

#include <stdint.h>

/* Like pico_music.h, but for when the Arkos player itself doesn't fit in
 * resident budget -- which it didn't for picodiver: the player alone
 * (<psg/arkos.h>'s prebuilt asm_arkos_zx_smc.asm, pulled in resident by
 * pico_music.c) overflowed picodiver's build by 3099 bytes with ZERO song
 * data loaded. Unlike a game's own large const data (picodiver_map_data.c,
 * banked via lib/pico_bank.h the ordinary way), the player isn't one of
 * this project's own source files -- it comes from z88dk's own prebuilt
 * classic-clib library archive the moment anything references ply_akg_*
 * via <psg/arkos.h>, so the usual "compile our own .c fresh to .asm, then
 * section-rename it" banking trick doesn't apply to it directly.
 *
 * The fix (same approach David Crespo's CelesteNext took for the same
 * problem, see docs/music-notes.md): vendor the player's own source
 * (lib/pico_music_player.asm, copied from z88dk's libsrc/target/zx/psg/
 * asm_arkos_zx_smc.asm) so it's something THIS project compiles itself,
 * then bank it -- and, since the player needs its song's data mapped
 * into memory at the same time it's running (not a sequential page-in-
 * then-call-later relationship), concatenate the vendored player AND one
 * song's generated .asm into the SAME physical page, built by the
 * Makefile's BANKED_MUSIC_SONGS mechanism (see the Makefile's own
 * comment next to it). A combined player+song page is comfortably under
 * the 8192-byte single-page ceiling lib/pico_bank.h documents (measured:
 * player ~3099 bytes, picodiver's own first song ~1834 bytes, ~4933
 * bytes combined -- about 3.2KB of headroom left in the page for a
 * second short song or sfx data later).
 *
 * Call site shape -- same pico_bank_page_in()/page_out() pattern as any
 * other banked module, wrapping the whole call (unlike pico_music.c's
 * unbanked pico_music_play_frame(), this one pages in/out internally;
 * there's no separate step the caller needs to do): */
void pico_music_banked_init(uint8_t bank_page, const void *song, uint8_t subsong);
void pico_music_banked_play_frame(uint8_t bank_page);
void pico_music_banked_stop(uint8_t bank_page);

#endif
