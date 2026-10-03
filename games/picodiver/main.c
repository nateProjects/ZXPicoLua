/* PicoDiver -- ported from PicoJS/games/picodiver/picodiver.js, itself a
 * port of the original Pico-8 cart picodiver2.p8
 * (https://github.com/nateProjects/PicoDiver). M4's real integration
 * test: the first full game built against this library, not just a demo
 * scene.
 *
 * Deliberately NOT ported (matching picodiver.js's own documented scope):
 *   * sfx(n) -- no audio API in this library yet, all call sites skipped.
 *   * the JS port's "canvas tainted" sget() warning -- a workaround for a
 *     browser-only failure mode (opening the page via file://) that has
 *     no equivalent here; pico_sget() always works.
 *
 * Deliberate adaptations (porting JS to this library's C API, not Pico-8
 * itself to something different):
 *   * No implicit "current colour" state (picoJS.color()/color()) --
 *     this library's primitives always take an explicit colour argument
 *     (pico_gfx.h's own documented policy). `curcol` below is this file's
 *     own tracking variable, not a library feature, updated at each point
 *     the original calls color(X) and threaded explicitly into every
 *     following draw call until the next change -- mechanical, not a
 *     design choice of its own.
 *   * `obj` (the on-screen fish/creatures) is a fixed-size array with
 *     swap-and-shrink removal, not JS's dynamic add()/del()/all() (this
 *     library doesn't have those -- pico_gfx.h's own note on why: no
 *     sensible direct C analogue, use plain arrays/structs). The
 *     original's own "remove overlapping objects" block (`del(obj.thing)`,
 *     a dot not a comma, confirmed in picodiver.js's own comment to be a
 *     no-op in the original too) is dropped rather than ported as dead
 *     code.
 *   * Runs at a halved 25fps (two intrinsic_halt() waits per logical
 *     frame) rather than the Next's native 50Hz, approximating the
 *     original cart's 30fps _update() pacing (not _update60()) -- frame-
 *     counter-based timers throughout this game (oxygen depletion, enemy
 *     spawn rate) are tuned against that assumption, and running them at
 *     full 50Hz would make the game noticeably faster/harder than
 *     intended, not just a cosmetic frame-rate difference. */

#include <intrinsic.h>
#include <string.h>
#include "pico_video.h"
#include "pico_gfx.h"
#include "pico_sprite.h"
#include "pico_map.h"
#include "pico_math.h"
#include "pico_input.h"
#include "picodiver_data.h"

/* --- game state ---------------------------------------------------------
 * Field layout/comments mirror picodiver.js's own 1-based "thing" arrays
 * verbatim (index 0 unused) -- see that file's own "Table-index
 * convention" header comment for why: every field-position comment here
 * matches the original Lua cart's source, not just the JS port. */

typedef struct {
    int16_t f[10]; /* [1]=x [2]=y [3]=sprstr [4]=sprcur [5]=sprend [6]=ani [7]=wid [8]=hi [9]=face */
} Thing;

#define MAX_OBJS 24
static Thing obj[MAX_OBJS];
static uint8_t obj_count;

/* x, y, sprstr, sprcur, sprend, ani, wid, hi, face -- index 0 unused. */
static const int16_t creat[6][10] = {
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 1, 48, 65, 65, 66, 2, 1, 1, 0 },   /* 1: fish */
    { 0, 1, 48, 80, 80, 85, 3, 2, 1, 0 },   /* 2: eel */
    { 0, 1, 48, 96, 96, 104, 3, 3, 2, 0 },  /* 3: dolphin/shark */
    { 0, 1, 48, 105, 105, 108, 2, 2, 2, 1 }, /* 4: piranha */
    { 0, 1, 48, 73, 73, 76, 2, 2, 2, 0 }    /* 5: big fish */
};

typedef struct {
    int16_t sp, ini, img, ani;
    int16_t x, y;
    int16_t h;
    uint8_t dr;   /* 1 = facing right, 0 = facing left */
    uint8_t mov;
    int16_t score;
    int16_t life;
    int16_t oxy;
} Cabin;

static Cabin cab;
static int16_t oct_x, oct_t; /* octopus: oct.y is always a fixed 32, used as a literal below */
static uint8_t backx; /* unused beyond initial assignment in the original -- kept for fidelity */
static int16_t sx;    /* ship background scroll position */
static uint8_t hard, easy;
static uint8_t mode;
static uint16_t frame;
static int16_t level;
static int16_t octspeed, bubblelimit, fishnum;

static uint8_t curcol; /* this file's own "current colour" tracking -- see top comment */

/* --- redraw-on-change tracking (performance fix, post-human-testing) ---
 *
 * The original port called back() -- a full pico_cls() + a 192-tile
 * ocean map() + a 2x128-tile scrolling ship -- unconditionally every
 * single frame, mirroring real Pico-8's own "just redraw everything in
 * _draw()" idiom. That's fine on Pico-8's own highly optimised engine;
 * on this library's software pixel-pushing it meant redrawing a
 * completely static 320-tile background from scratch 50+ times a second,
 * which human testing confirmed made even the title screen's ship --
 * which only actually moves once every 30 frames -- sluggish and the
 * whole game barely responsive, the same "redraw on change, not every
 * frame" lesson games/demo/main.c already learned the hard way in M2.5
 * (see docs/input-notes.md), just not yet applied here.
 *
 * Because the back buffer alternates between two physical banks
 * (pico_flip(), pico_video.h), each one lags the other by one flip --
 * tracked per buffer (`[2]`), same pattern as games/demo/main.c's own
 * ball_x[2]/o_lit[2]. buf_idx itself is set once per frame in main(). */
static uint8_t buf_idx;
static uint8_t last_mode[2] = { 0xFF, 0xFF };   /* 0xFF: force a full redraw the first time each buffer is used */
static int16_t last_sx_drawn[2] = { -999, -999 };

#define MODE_TITLE 0
#define MODE_HARD_SELECT 3
#define MODE_WIN 6
#define MODE_DEATH 9

/* The four menu-style screens, with no moving sprites of their own
 * (besides the ship, handled separately in shipshow()) -- see back()'s
 * own comment for why only these get the "skip the ocean redraw when
 * nothing changed" optimization; gameplay (mode 1) always needs a full
 * redraw until its own moving sprites get proper erase tracking. */
static uint8_t mode_is_static(uint8_t m) {
    return m == MODE_TITLE || m == MODE_HARD_SELECT || m == MODE_WIN || m == MODE_DEATH;
}

static void centre_print(const char *s, uint8_t y) {
    uint8_t len = (uint8_t)strlen(s);
    uint8_t x = (uint8_t)(64 - (int16_t)(len * 2));
    pico_print(s, x, y, curcol);
}

static void newobj(uint8_t type) {
    Thing t;
    uint8_t l;
    if (obj_count >= MAX_OBJS) return;
    t.f[1] = 1;
    t.f[2] = (int16_t)(48 + pico_rnd(56));
    for (l = 3; l <= 9; l++) t.f[l] = creat[type][l];
    obj[obj_count++] = t;
}

static void obj_remove(uint8_t i) {
    obj[i] = obj[obj_count - 1];
    obj_count--;
}

static void cabmove(void) {
    cab.mov = 1;
    cab.img += 1;
    if (cab.img > cab.ini + cab.ani) cab.img = cab.ini;
}

static void cabshow(void) {
    pico_spr((uint8_t)cab.img, cab.x, cab.y, 1, (uint8_t)cab.h, cab.dr, 0);
    if (cab.dr) {
        pico_spr((uint8_t)cab.sp, (int16_t)(cab.x + 8), cab.y, 1, (uint8_t)cab.h, cab.dr, 0);
    } else {
        pico_spr((uint8_t)cab.sp, (int16_t)(cab.x - 8), cab.y, 1, (uint8_t)cab.h, cab.dr, 0);
    }
}

static void objmove(uint8_t n) {
    obj[n].f[1] += 1;
}

static void fishshow(void) {
    uint8_t n;
    for (n = 0; n < obj_count; n++) {
        objmove(n);
        pico_spr((uint8_t)obj[n].f[4], obj[n].f[1], obj[n].f[2],
                  (uint8_t)obj[n].f[7], (uint8_t)obj[n].f[8], (uint8_t)obj[n].f[9], 0);
    }
}

static void octshow(void) {
    pico_spr(11, oct_x, 32, 1, 1, 0, 0);
    pico_spr((uint8_t)oct_t, oct_x, 40, 1, 1, 0, 0);
}

static void octmove(void) {
    if (oct_x < 128) oct_x++; else oct_x = 1;
    if (oct_t == 14) oct_t = 12; else oct_t++;
}

static void shipshow(void) {
    /* Advance the scroll position unconditionally (state, not drawing --
     * cheap, always correct to do every frame regardless of mode). Only
     * the two map() calls (256 tiles between them) are gated on this
     * buffer's own ship position actually being stale. */
    if (frame % 30 == 1) sx++;
    if (sx >= 128) sx = 1;

    if (sx != last_sx_drawn[buf_idx]) {
        pico_pal(2, 12); /* "don't ask -)" -- preserved verbatim from the original */
        pico_map(0, 0, sx, 0, 16, 4);
        pico_map(0, 0, (int16_t)(sx - 128), 0, 16, 4);
        last_sx_drawn[buf_idx] = sx;
    }
}

static void back(void) {
    /* The ocean (192 tiles) never changes -- only redraw it (and clear
     * the screen) when this buffer is showing a different mode than last
     * time, not every frame. Forces a fresh ship redraw too, since a
     * mode change just wiped it.
     *
     * Gameplay (mode 1) is the one exception: unlike the static menu
     * screens, cab/creatures/octopus move every frame on top of the
     * ocean, and used to rely on the OLD unconditional full redraw to
     * implicitly erase their previous position -- skipping that redraw
     * here (as this fix originally did unconditionally) left visible
     * ghost trails, caught by a human playtest, not predicted in advance.
     * Always force a full redraw during gameplay until each moving
     * element gets its own proper per-buffer erase/redraw tracking
     * (the games/demo/main.c ball_x[2]-style pattern) -- a real further
     * optimization, just not done yet. The menu screens (title/hard-
     * select/win/death) have no moving sprites of their own (only the
     * ship, already handled below), so they get the full benefit now. */
    if (mode != last_mode[buf_idx] || !mode_is_static(mode)) {
        pico_cls(0);
        pico_map(0, 4, 0, 32, 16, 12);
        last_mode[buf_idx] = mode;
        last_sx_drawn[buf_idx] = -999;
    }
    shipshow();
}

/* --- pixel collision (dw817's method, ported from picodiver.js) -------- */

static void get_cab_sprite_coords(int16_t *sx_out, int16_t *sy_out) {
    int16_t n = cab.img;
    *sx_out = (int16_t)((n % 16) * 8);
    *sy_out = (int16_t)((n / 16) * 8);
}

static uint8_t pixel_collide(int16_t sh1, int16_t sv1, int16_t sxx1, int16_t syy1, int16_t ph1, int16_t pv1,
                              int16_t sh2, int16_t sv2, int16_t sxx2, int16_t syy2, int16_t ph2, int16_t pv2) {
    int16_t i, j, x, y, t;

    if ((int32_t)sxx1 * syy1 > (int32_t)sxx2 * syy2) {
        t = sh1; sh1 = sh2; sh2 = t;
        t = sv1; sv1 = sv2; sv2 = t;
        t = sxx1; sxx1 = sxx2; sxx2 = t;
        t = syy1; syy1 = syy2; syy2 = t;
        t = ph1; ph1 = ph2; ph2 = t;
        t = pv1; pv1 = pv2; pv2 = t;
    }

    for (i = 0; i <= syy1 - 1; i++) {
        for (j = 0; j <= sxx1 - 1; j++) {
            x = (int16_t)(j + ph1 - ph2);
            y = (int16_t)(i + pv1 - pv2);
            if (x >= 0 && x < sxx2 && y >= 0 && y < syy2 &&
                pico_sget((uint8_t)(sh1 + j), (uint8_t)(sv1 + i)) > 0 &&
                pico_sget((uint8_t)(sh2 + x), (uint8_t)(sv2 + y)) > 0) {
                return 1;
            }
        }
    }
    return 0;
}

static uint8_t pixel_collider_oct(void) {
    int16_t cab_sx, cab_sy;
    int16_t oct_sx = (11 % 16) * 8, oct_sy = (11 / 16) * 8;
    get_cab_sprite_coords(&cab_sx, &cab_sy);
    return pixel_collide(cab_sx, cab_sy, 16, 8, cab.x, cab.y,
                          oct_sx, oct_sy, 8, 16, oct_x, 32);
}

static uint8_t pixel_collider_obj(uint8_t n) {
    int16_t cab_sx, cab_sy;
    int16_t obj_sn = obj[n].f[4];
    int16_t obj_sx = (int16_t)((obj_sn % 16) * 8);
    int16_t obj_sy = (int16_t)((obj_sn / 16) * 8);
    int16_t obj_w = (int16_t)(obj[n].f[7] * 8);
    int16_t obj_h = (int16_t)(obj[n].f[8] * 8);
    get_cab_sprite_coords(&cab_sx, &cab_sy);
    return pixel_collide(cab_sx, cab_sy, 16, 8, cab.x, cab.y,
                          obj_sx, obj_sy, obj_w, obj_h, obj[n].f[1], obj[n].f[2]);
}

/* --- init ---------------------------------------------------------------- */

static void gameinit(void) {
    mode = 1;
}

static void titleinit(void) {
    cab.sp = 4; cab.ini = 5; cab.img = 5; cab.ani = 2;
    cab.x = 48; cab.y = 48; cab.h = 1;
    cab.dr = 1; cab.mov = 0;
    cab.score = 0; cab.life = 3; cab.oxy = 10;

    obj_count = 0;
    newobj(1); /* start with a fish */

    oct_x = 1; oct_t = 11;
    backx = 1;
    sx = 1;
    hard = 0;
    mode = MODE_TITLE;
    frame = 1;
    level = 1;
    easy = 0;
    octspeed = 5;
    bubblelimit = 10;
    fishnum = 45;
}

/* --- draw ------------------------------------------------------------- */

static void titledraw(void) {
    back();
    curcol = 7;  centre_print("pico-diver", 2);
    curcol = 10; centre_print("catch 20 goldfish to win", 36);
                 centre_print("but don't run out of air", 52);
                 centre_print("or collide with the big fish", 68);
    curcol = 8;  centre_print("press x for easy mode", 84);
    if (pico_btn(PICO_BTN_X)) gameinit();
                 centre_print("press z for arcade mode", 92);
    if (pico_btnp(PICO_BTN_O)) mode = MODE_HARD_SELECT;
    curcol = 7;  centre_print("nct 2018 / jfc 1984", 108);
}

static void harddraw(void) {
    back();
    curcol = 7;  centre_print("arcade mode", 2);
    curcol = 10; centre_print("10 levels - each one faster", 36);
                 centre_print("grey sharks kill", 52);
                 centre_print("oxygen is reduced over time", 68);
                 centre_print("try for a perfect score", 84);
    curcol = 8;  centre_print("press x to begin", 108);
    curcol = 7;
    hard = 1;
    if (pico_btnp(PICO_BTN_X)) gameinit();
}

static void gamedraw(void) {
    char numbuf[8], linebuf[16];
    uint8_t l;

    back();

    curcol = 0;
    strcpy(linebuf, "fish ");
    pico_tostr_int(cab.score, numbuf);
    strcat(linebuf, numbuf);
    pico_print(linebuf, 1, 121, curcol);

    if (hard) {
        strcpy(linebuf, "level ");
        pico_tostr_int(level, numbuf);
        strcat(linebuf, numbuf);
        pico_print(linebuf, (uint8_t)(level < 10 ? 100 : 96), 121, curcol);
    }

    for (l = 1; l <= cab.oxy; l++) pico_spr(51, (int16_t)((l * 8) - 8), 0, 1, 1, 0, 0);
    for (l = 1; l <= cab.life; l++) pico_spr(32, (int16_t)(128 - (l * 8)), 0, 1, 1, 0, 0);

    octshow();
    fishshow();
    cabshow();

    if (frame % fishnum == 0) newobj(1);
    if (frame % 60 == 0) newobj((uint8_t)(2 + pico_rnd(4)));

    if (hard) pico_pal(12, 6); /* see titleinit()'s note: never reset, matches the original */
}

static void windraw(void) {
    back();
    curcol = 7;  centre_print("you win", 2);
    curcol = 10; centre_print("a perfect score!", 40);
    if (level == 11) centre_print("1000 points, 200 fish", 60);
    else centre_print("100 points, 20 fish", 60);
    curcol = 8;  centre_print("press x to restart", 88);
    if (pico_btnp(PICO_BTN_X)) titleinit();
                 centre_print("press z for hard mode", 96);
    if (pico_btnp(PICO_BTN_O)) mode = MODE_HARD_SELECT;
    curcol = 7;
    hard = 0;
}

static void deathdraw(void) {
    char numbuf[8], linebuf[24];

    back();
    curcol = 7;  centre_print("game over", 2);
    curcol = 10;

    pico_tostr_int((int16_t)(cab.score * 5), numbuf);
    strcpy(linebuf, "your score was "); strcat(linebuf, numbuf);
    centre_print(linebuf, 36);

    pico_tostr_int(cab.score, numbuf);
    strcpy(linebuf, "you collected "); strcat(linebuf, numbuf); strcat(linebuf, " fish");
    centre_print(linebuf, 52);

    pico_tostr_int((int16_t)(3 - cab.life), numbuf);
    strcpy(linebuf, "you lost "); strcat(linebuf, numbuf); strcat(linebuf, " lives");
    centre_print(linebuf, 68);

    if (hard) {
        pico_tostr_int(level, numbuf);
        strcpy(linebuf, "you reached level "); strcat(linebuf, numbuf);
        centre_print(linebuf, 84);
    }
    curcol = 8;  centre_print("press x to begin", 92);
    if (pico_btnp(PICO_BTN_X)) titleinit();
                 centre_print("press z for hard mode", 104);
    if (pico_btnp(PICO_BTN_O)) mode = MODE_HARD_SELECT;
    curcol = 7;
    hard = 0;
}

/* --- update -------------------------------------------------------------- */

static void gameupdate(void) {
    uint8_t n;

    cab.mov = 0;
    if (pico_btn(PICO_BTN_LEFT))  { cab.dr = 0; cab.x -= 1; if (cab.x < 8) cab.x = 8; cabmove(); }
    if (pico_btn(PICO_BTN_RIGHT)) { cab.dr = 1; cab.x += 1; if (cab.x > 108) cab.x = 108; cabmove(); }
    if (pico_btn(PICO_BTN_UP))    { cab.y -= 1; if (cab.y < 32) cab.y = 32; cabmove(); }
    if (pico_btn(PICO_BTN_DOWN))  { cab.y += 1; if (cab.y > 108) cab.y = 108; cabmove(); }

    if (pixel_collider_oct()) {
        if (frame % 20 == 0) {
            if (cab.score > 0 && !easy) cab.score -= 1;
        }
    }

    n = 0;
    while (n < obj_count) {
        uint8_t removed = 0;

        if (pixel_collider_obj(n)) {
            if (obj[n].f[3] <= 66) {
                cab.score += 1;
            }
            if (obj[n].f[3] >= 67) {
                if (cab.score >= 1 && !easy) cab.score -= 1;
                if (hard && (obj[n].f[3] < 105 && obj[n].f[3] > 95) && !easy) cab.life -= 1;
            }
            obj_remove(n);
            removed = 1;
        } else if (obj[n].f[1] >= 128) {
            obj_remove(n);
            removed = 1;
        }

        if (!removed) n++;
        /* removing swaps the last element into slot n -- re-check the
         * same index next iteration instead of advancing past it. */
    }

    if (frame % 10 == 0) {
        for (n = 0; n < obj_count; n++) {
            obj[n].f[4] += obj[n].f[7]; /* preserved verbatim from the original -- see picodiver.js's own "what does this do?" comment on this exact line */
            if (obj[n].f[4] > obj[n].f[5]) obj[n].f[4] = obj[n].f[3];
        }
    }

    if (frame % octspeed == 0) octmove();

    if (level >= 9)      { octspeed = 1; bubblelimit = 6;  fishnum = 65; }
    else if (level >= 7) { octspeed = 2; bubblelimit = 7;  fishnum = 60; }
    else if (level >= 5) { octspeed = 3; bubblelimit = 8;  fishnum = 55; }
    else if (level >= 3) { octspeed = 4; bubblelimit = 9;  fishnum = 50; }
    else if (level >= 1) { octspeed = 5; bubblelimit = 10; fishnum = 45; }

    if (cab.life == 0) mode = MODE_DEATH;

    if (cab.score == 20) {
        if (!hard) mode = MODE_WIN;
        if (hard) {
            if (level < 11) { level += 1; cab.score = 0; }
            if (level == 11) mode = MODE_WIN;
        }
    }

    if (cab.y == 32) {
        if (cab.oxy < bubblelimit) {
            if (frame % 20 == 1) cab.oxy += 1;
        }
    }
    if (cab.y > 32) {
        if (frame % 30 == 1) cab.oxy -= 1;
        if (cab.oxy == 0 && cab.life > 0 && !easy) {
            cab.oxy = (int16_t)bubblelimit;
            cab.life -= 1;
        }
    }

    frame += 1;
}

/* --- entry point ----------------------------------------------------- */

int main(void) {
    intrinsic_ei();

    pico_video_init(128, 128);
    pico_spr_init(picodiver_sprite_data, PICODIVER_SHEET_W, PICODIVER_SHEET_H);
    pico_map_init((uint8_t *)picodiver_map_data, PICODIVER_MAP_W, PICODIVER_MAP_H);

    titleinit();
    buf_idx = 0;

    for (;;) {
        intrinsic_halt();
        intrinsic_halt(); /* halved to ~25fps -- see this file's top comment */
        pico_input_update();

        if (mode == MODE_TITLE) { /* nothing to update */ }
        else if (mode == MODE_DEATH) { /* nothing to update */ }
        else if (mode == MODE_WIN) { /* nothing to update */ }
        else if (mode == MODE_HARD_SELECT) { /* nothing to update */ }
        else gameupdate();

        if (mode == MODE_TITLE) titledraw();
        else if (mode == MODE_DEATH) deathdraw();
        else if (mode == MODE_WIN) windraw();
        else if (mode == MODE_HARD_SELECT) harddraw();
        else gamedraw();

        pico_flip();
        buf_idx ^= 1;
    }
}
