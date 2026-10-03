/* Uses the low-level esxdos_f_* calls (<arch/zx/esxdos.h>), not stdio's
 * fopen/fread/fwrite/fclose -- dark-room-next's src/save.c already found
 * plain stdio pulls in the entire generic buffered-FILE* layer as
 * resident code and can blow a tight resident-code budget outright (see
 * that file's own header for the measured overflow); the low-level calls
 * map close to the raw esxDOS RST syscalls with none of that machinery.
 * No banking here, unlike save.c -- this project's codebase is small
 * enough that everything is still resident; worth revisiting only if
 * that stops being true. */

#include <arch/zx/esxdos.h>
#include "pico_data.h"

#define ESXDOS_HANDLE_ERROR 0xFF

static int16_t slots[PICO_DATA_SLOTS];
static char filename[13]; /* up to 8 id chars + ".DAT" + NUL */

uint8_t pico_cartdata(const char *id) {
    uint8_t i, h;
    int n;

    for (i = 0; i < 8 && id[i] != 0; i++) filename[i] = id[i];
    filename[i++] = '.';
    filename[i++] = 'D';
    filename[i++] = 'A';
    filename[i++] = 'T';
    filename[i] = 0;

    for (i = 0; i < PICO_DATA_SLOTS; i++) slots[i] = 0;

    h = esxdos_f_open(filename, ESXDOS_MODE_R | ESXDOS_MODE_OE);
    if (h == ESXDOS_HANDLE_ERROR) return 0;
    n = esxdos_f_read(h, slots, sizeof(slots));
    esxdos_f_close(h);

    return (n == (int)sizeof(slots)) ? 1 : 0;
}

int16_t pico_dget(uint8_t index) {
    if (index >= PICO_DATA_SLOTS) return 0;
    return slots[index];
}

void pico_dset(uint8_t index, int16_t value) {
    if (index >= PICO_DATA_SLOTS) return;
    slots[index] = value;
}

uint8_t pico_cartdata_save(void) {
    uint8_t h;

    h = esxdos_f_open(filename, ESXDOS_MODE_W | ESXDOS_MODE_CT);
    if (h == ESXDOS_HANDLE_ERROR) return 0;
    esxdos_f_write(h, slots, sizeof(slots));
    esxdos_f_close(h);
    return 1;
}
