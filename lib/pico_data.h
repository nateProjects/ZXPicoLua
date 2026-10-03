#ifndef PICO_DATA_H
#define PICO_DATA_H

#include <stdint.h>

#define PICO_DATA_SLOTS 64

/* cartdata(id) -- declares this cart's persistent-data file and loads any
 * existing save from it into the 64 dget()/dset() slots (zeroed if no
 * save exists yet). id becomes the save file's name (at most 8
 * characters used, esxDOS 8.3-filename convention -- ".DAT" is appended
 * automatically). Must be called before dget()/dset(). Returns 1 if an
 * existing save was loaded, 0 if starting fresh (not an error either way
 * -- matches dark-room-next's save_read_gamestate()'s own "0 means
 * caller falls back to a fresh state" convention, src/save.c). */
uint8_t pico_cartdata(const char *id);

/* dget(index)/dset(index, value) -- read/write one of 64 persistent
 * slots (0-63), in RAM only. Deliberate deviation from real Pico-8, where
 * dset() writes through to disk immediately: hammering esxDOS/SD-card
 * I/O on every dset() call (plausibly every frame, for a live score
 * counter) is both slow and a real failure-mode risk -- see
 * pico_cartdata_save() below, which a game calls explicitly instead (on
 * a pause/menu, a level-complete event, or similar), matching how
 * dark-room-next's own save system works (explicit autosave() calls, not
 * a write on every state change). Out-of-range index reads/writes are
 * ignored, matching pico_mget()/pico_mset()'s same "naturally bounds-
 * prone" reasoning (pico_map.h). */
int16_t pico_dget(uint8_t index);
void pico_dset(uint8_t index, int16_t value);

/* Writes all 64 slots to the file named by the last pico_cartdata() call.
 * Returns 1 on success, 0 if the file couldn't be opened for writing
 * (matches dark-room-next's save_write_gamestate(), src/save.c). */
uint8_t pico_cartdata_save(void);

#endif
