/* PicoDiver -- rebuilt from scratch, in testable stages, after the
 * original one-shot port (see docs/reference/main_v1_reference.c.txt)
 * accumulated enough complexity and stack-margin fragility that small,
 * individually-validated changes kept causing unexplained regressions
 * late in its life (docs/picodiver-notes.md "Round 4/5"). Per the user's
 * explicit direction: start with just the player and movement (known
 * fast, proven by games/demo's ball), then add the background, then one
 * enemy, then collision -- testing speed and correctness after each
 * stage, not building it all at once.
 *
 * STAGE 1 (done, confirmed smooth): player (the cabin diver) + movement
 * only -- real sprite assets, real movement feel, native 50Hz, 2px/tick.
 *
 * STAGE 2, attempt 1 (software pico_map(), redrawn every frame): user-
 * confirmed "unbearably slow" -- matches this whole session's repeated
 * finding that software-blitting a full tile grid through Layer 2 every
 * frame is expensive regardless of CPU clock (docs/picodiver-notes.md).
 *
 * STAGE 2 (done, confirmed smooth): the ocean background via the Next's
 * HARDWARE tilemap (lib/pico_tilemap.c) instead of pico_map() -- an
 * earlier software-blit attempt was user-confirmed "unbearably slow"
 * (matching this whole session's repeated finding that redrawing a full
 * tile grid through Layer 2 every frame is expensive regardless of CPU
 * clock, docs/picodiver-notes.md); the hardware tilemap composites for
 * free during scanout instead, with Layer 2 (the player sprite) sitting
 * on top via the global-transparency-colour mechanism. "What a real Next
 * game would do" for a static background, per the user's own framing --
 * not a Pico-8-faithful primitive, a deliberate departure from it where
 * the software-faithful approach measurably couldn't keep up. Two real
 * bugs found and fixed via direct measurement, not guessing: the
 * tilemap's own coordinate space needed a further (4,4)-tile offset
 * beyond the logical-canvas centring alone (confirmed by screenshotting
 * and measuring against the player sprite's known-correct position);
 * and the cabin sprite's "background" pixels are Pico-8 colour 1, not
 * 0 (confirmed by reading the actual sprite data), a colour-matching
 * trick the original art uses instead of real transparency -- fixed by
 * matching the tilemap's colour 1 to Layer 2's own, not by trying to
 * make Layer 2 transparent there.
 *
 * STAGE 3 (done, confirmed smooth): + one enemy (a fish, real sprite
 * indices from the original cart) swimming across the screen -- still no
 * collision.
 *
 * STAGE 4 (current): + collision between the player and the fish, via a
 * new library primitive (pico_spr_collide(), lib/pico_sprite.c) --
 * dw817's pixel-perfect method, the same algorithm pico_sget() was
 * originally added for and the one the real cart/PicoJS port both use
 * (not invented for this port). Deliberately minimal response for this
 * stage, matching the "test one thing at a time" approach: a visible,
 * on-screen hit counter (no score/life/oxygen/HUD system yet -- those
 * come later, once collision itself is confirmed correct and fast) and
 * the fish respawning off the left edge, same as its ordinary wrap. The
 * original cart's own cab hitbox (get_cab_sprite_coords()/
 * pixel_collider_obj(), docs/reference) reads a 16px-wide region starting
 * at the body sprite's sheet position -- which, for body frames 5-7,
 * actually spills into the NEXT animation frame's pixels in the sheet,
 * not the separately-drawn fin sprite (index 4); almost certainly an
 * original-cart quirk, not a deliberate hitbox shape. Not reproduced here
 * -- this uses just the body sprite's own 8x8 pixels, the cleaner, more
 * obviously-correct choice, and not something the original author would
 * have been relying on as intentional game feel.
 *
 * POST-STAGE-3 correction (user-flagged porting-fidelity issue): Stage 1's
 * speed fix had doubled cab_x/cab_y's own movement literals from 1 to a
 * bare 2, with no indication of why -- fine for this one file, but a bad
 * convention for a library meant to make porting other Pico-8 carts easy:
 * someone porting another cart's `obj.x += 1` would get it silently
 * running at half the intended speed if they didn't know to double every
 * such literal themselves. Fixed by keeping every object's own movement
 * delta as its original Lua literal (1) and applying the "run faster than
 * literal Pico-8" decision as one separate, named PICO_SPEED_SCALE
 * constant instead -- same net on-screen speed, but the "why" now lives
 * in one place and a freshly-ported object's "1" reads correctly as-is.
 *
 * STAGE 5 (current): + oxygen/life, the original's own survival mechanic
 * (docs/reference's gameupdate(): cab.oxy drains by 1 every 30 ticks
 * while underwater (cab.y > 32), refills by 1 every 20 ticks at the
 * surface (cab.y == 32), and hitting 0 oxygen costs a life and refills to
 * bubblelimit). frame%20/frame%30 kept as their original literal tick
 * counts, same reasoning as PICO_SPEED_SCALE above: this engine's own
 * native-50Hz tick rate (vs. the original's ~30fps) already makes these
 * drain/refill faster in real time without needing the thresholds
 * themselves touched. bubblelimit fixed at 10 (the original's own
 * level-0/no-level default, docs/reference) -- level-based scaling isn't
 * built yet. Score (+1 per fish, matching the original's own
 * obj[n].f[3]<=66 "collectible" branch -- regular fish are never a
 * damage source there, only the harder creature types past f[3]>=67
 * are) replaces Stage 4's placeholder "HITS" counter, now that there's a
 * real mechanic for it to mean something in. No life-loss-from-touching-
 * a-fish in this stage, matching the original exactly -- only oxygen
 * can cost a life until an actual hazard creature exists.
 *
 * Still deliberately minimal: a text HUD (score/oxy/life as numbers),
 * not the original's icon-row HUD (gamedraw_hud(), sprite 51 bubbles/
 * sprite 32 hearts) -- that's a visual-polish pass for later, once this
 * stage confirms the underlying mechanic itself is correct and doesn't
 * cost anything noticeable. On life reaching 0, this just resets cab's
 * position/oxy/life/score back to a fresh start -- the original's real
 * title/death-screen mode system (mode_is_static(), MODE_DEATH, docs/
 * reference) isn't built yet either. cab_y's own upward clamp also moves
 * from Stage 1's arbitrary 8 back to the original's real 32 (the
 * surface) here -- it has to, for "cab.y == 32" (surface, oxygen
 * refills) to mean anything; before oxygen existed there was no reason
 * that specific value mattered.
 *
 * STAGE 6 (current): + the real creature variety and spawn system
 * (docs/reference: creat[], newobj()/obj[], the octopus), specifically
 * to performance-test more on-screen pixel-perfect collision checks at
 * once -- this is the stress test the earlier stages' "one hardcoded
 * fish" scope was deliberately deferring. Stage 4's single hardcoded
 * fish is gone, replaced with the original's real object pool (MAX_OBJS
 * 16, matching the original's own sizing -- docs/reference's own comment
 * on why 16 is comfortable headroom) spawning real fish (every 45 ticks)
 * plus a random harder creature -- eel/dolphin-shark/piranha/big-fish --
 * every 60 ticks, same cadence as the original's gameupdate(). Also adds
 * the octopus, a separate fixed single entity (not pool-spawned) that
 * continuously drains score while touching it, matching
 * pixel_collider_oct()'s own semantics exactly (distinct from a fish's
 * one-shot pickup-and-remove). The original's own octopus hitbox
 * (pixel_collider_oct()) reads a single 16-pixel-tall region starting at
 * sprite 11's sheet position, which would actually read into whatever
 * sheet cell sits below it, NOT the real second sprite (oct_t, drawn
 * separately and animated) -- the same kind of adjacent-cell quirk
 * Stage 4 already found and chose not to reproduce for cab's own hitbox.
 * This does two separate, correct collision checks instead (head sprite
 * 11 + the real animated oct_t sprite below it).
 *
 * HUD upgraded from Stage 5's placeholder text to the original's real
 * icon row -- oxygen bubbles (sprite 51) growing from the left, life
 * hearts (sprite 32) growing from the right, same positions as
 * gamedraw_hud() -- since a text/icon mix would look inconsistent and
 * the original always paired them. Score stays text (not asked to
 * change, and the original's own score display is a separate ocean-row
 * label, not part of this icon row at all).
 *
 * STAGE 7 (current): redraw-on-change, replacing the full-canvas
 * pico_cls(0)-every-frame approach every prior stage used. Direct ZRCP
 * frame-counter measurement (reading the `frame` global at timed
 * intervals, docs/picodiver-notes.md) found the real cost precisely:
 * player+background alone already ran at ~16-17 ticks/sec, not native
 * 50Hz as earlier stages assumed from indirect (screenshot/feel-based)
 * timing -- a real correction, not just a Stage 6 regression -- and the
 * full creature/octopus/icon-HUD system roughly halved that again,
 * matching the user's own hands-on comparison against the real Pico-8
 * cart ("half the speed"). Two targeted fixes (removing redundant
 * per-row MMU page writes in pico_cls_rect(), and nestable DI/EI batch
 * sessions, lib/pico_video.c) measured ZERO improvement when tested in
 * isolation -- ruled out, not guessed around. The real, confirmed cost
 * is pico_cls(0) clearing all 16384 pixels of the logical canvas every
 * single frame, then redrawing a few hundred pixels' worth of actual
 * sprite content back on top of it -- almost all of that work is pure
 * waste.
 *
 * This is the original cart's own proven fix too (docs/reference's
 * DirtyRect/track_rect/erase_rect/back(), added there after the exact
 * same profiling finding -- "the single largest remaining per-frame
 * cost"), adapted to this port's own architecture: the original erases
 * by redrawing the underlying pico_map() tiles (its background is
 * software-drawn); this port's ocean is the HARDWARE tilemap (Stage 2),
 * which is always there underneath for free, so "erasing" a sprite's
 * previous position is just pico_cls_rect() over that one small
 * rectangle -- no tile redraw needed at all, simpler than the original's
 * own version of this fix. Per-entity dirty rects (cab, each pool
 * object, the octopus), tracked per PHYSICAL BUFFER (buf_idx, 0/1) since
 * double-buffering means each buffer is only touched every other frame
 * and still shows whatever was drawn into it two flips ago -- same
 * `[2]`-indexed pattern games/demo/main.c's own ball_x[2]/o_lit[2] and
 * the original's own prev_cab[2]/prev_obj[2][MAX_OBJS]/prev_oct[2] use,
 * for the same reason. The icon HUD and score text also move from
 * "redraw every frame regardless" to "redraw only when the value
 * actually changed since this buffer last showed it" -- the original's
 * own prev_oxy[2]/prev_life[2] mechanism, which its own comment calls
 * out as "the single largest remaining per-frame cost" when done
 * naively; safe here for the same reason it's safe there: nothing else
 * ever draws into the sky strip (y<32) during gameplay, so a stale icon
 * row can never need reasserting the way the original's own ocean-row
 * score text sometimes does (a fish can swim through *that* row --
 * ours sits above where any creature's y ever reaches, cab's own
 * CAB_Y_SURFACE clamp included, so no such exception is needed here).
 *
 * STAGE 8 (current): the title screen -- a real mode system (MODE_TITLE/
 * MODE_GAME, docs/reference's own mode global, though just these two
 * values; the original's hard-mode-select/win/death screens aren't
 * built, a separate, later ask) and the original's scrolling ship
 * banner (shipshow()/sx, docs/reference), which only ever belonged on
 * the title/menu screens there -- never gameplay (mode_is_static()
 * gates it), so this doesn't touch anything about how gameplay itself
 * looks. Ship tiles come straight from the original cart's own map data
 * (picodiver_map_data rows 0-3, the same rows this port's gameplay sky
 * strip overwrites with a plain placeholder tile -- confirmed by reading
 * the actual values: 9 distinct tiles forming a 3-times-repeated hull/
 * mast pattern, not guessed).
 *
 * Software-drawn via pico_spr(), not the hardware tilemap -- deliberately
 * different from the ocean's own Stage 2 treatment. The ocean is static
 * for the tilemap's whole lifetime; the ship scrolls continuously, and
 * the tilemap has exactly one GLOBAL horizontal scroll register shared
 * by the entire layer (lib/pico_tilemap.c) -- using it for the ship
 * would scroll the ocean along with it, which is wrong once this same
 * tilemap is also carrying gameplay's ocean content. Cheap anyway: the
 * ship only needs to redraw when `sx` actually changes (every 30 ticks,
 * matching the original's own cadence), using this file's own Stage-7
 * change-detection pattern (prev_sx_drawn[2]) -- not the "redraw every
 * frame" approach Stage 2 already proved too slow for backgrounds.
 *
 * Per-tile wraparound (screen_x = (col*8 + sx) % 128) replaces the
 * original's own two-pass approach (two pico_map() calls, one at sx and
 * one at sx-128, docs/reference's shipshow()) -- equivalent result, but
 * only possible because this draws tile-by-tile already (unlike
 * pico_map(), which blits a whole rectangular block per call and has no
 * cheaper way to wrap it).
 *
 * Entering gameplay from the title (X pressed) needs one real full
 * pico_cls(0) per buffer, the ONLY one left anywhere in this file --
 * Stage 7 removed pico_cls()-every-gameplay-frame, but the title
 * screen's own text/ship pixels are real Layer 2 content nothing in the
 * gameplay dirty-rect system knows to erase; game_entered[2] tracks
 * which buffers have had this one-time wipe, same `[2]`-per-buffer
 * reasoning as everything else here. Title text itself is drawn once
 * per buffer too (last_mode_drawn[2]), not every frame -- it never
 * changes while the title is showing, same "static screen, nothing to
 * do once drawn" reasoning the original's own back() uses for its
 * menu-style screens.
 *
 * STAGE 9 (current): the death and win screens (MODE_DEATH/MODE_WIN),
 * matching deathdraw_text()/windraw() (docs/reference) minus their
 * hard-mode-only branches (level number, the 1000-point/200-fish
 * message) -- hard mode itself still isn't built, a separate, later
 * ask. Both share Stage 8's own static-screen machinery (the ship
 * banner, "draw text once per buffer per mode" via last_mode_drawn)
 * rather than needing anything new -- generalised from "!= MODE_TITLE"
 * to "!= mode" so it correctly redraws on ANY mode change for a given
 * buffer, not just entry into the title specifically.
 *
 * Neither screen resets game state itself -- same as the original,
 * where "press x to restart" on both calls titleinit() and THAT does
 * the real reset. The one new piece: game_entered[2] (Stage 8, the
 * title->game full-clear tracker) now also gets cleared when gameplay
 * ends (life hits 0, or score hits 20), so the NEXT gameplay session
 * -- after the player goes through the death/win screen and the title
 * again -- still gets its own one-time full clear, instead of staying
 * at game_entered=1 from the session that just ended and leaving that
 * session's stale sprites on screen under the new one's dirty-rect-only
 * erasing.
 *
 * STAGE 10 (current): full-resolution mode -- the logical canvas grows
 * from the original cart's native 128x128 to this port's full physical
 * 256x192 (pico_video_init(256,192), SCREEN_W/SCREEN_H below), at native
 * 1:1 pixel scale -- NOT a zoomed/stretched view of the old 128x128
 * content (that was considered and explicitly declined: Layer 2 has no
 * hardware scale register, a software one would need real per-pixel
 * resampling throughout the whole pipeline, cost real speed, and still
 * couldn't reach the hardware tilemap background at all, since that
 * layer is composited independently of Layer 2 and has no scale concept
 * of its own either). This is a genuinely bigger play area instead --
 * more ocean, more sky, more room to move -- built entirely from
 * content the original cart's own map/sprite data already has, no new
 * art.
 *
 * The ocean tile pattern (3 distinct tiles, Stage 2) is still only
 * 16 columns x 12 rows of actual source data (picodiver_map_data's own
 * real content, confirmed by reading it -- the original cart itself
 * never needed more, being a 128x128 game). Tiled out to cover the
 * full new ocean area (32 columns x 20 rows) by wrapping the source
 * index (col%16, row%12) -- seamless because 10 of those 12 source rows
 * are themselves one uniform solid-colour tile already (Stage 2's own
 * reading of the actual data), so the repeat boundary is invisible
 * except at the one wave-edge/sand transition row, which just repeats
 * naturally every 12 rows like a real tileable texture. Same wrapping
 * idea extends ship_show()'s own banner from 16 to 32 tile columns.
 *
 * Everything with a canvas-size-shaped constant got touched: movement
 * bounds (cab_x/cab_y's own max clamps), creature spawn Y range, the
 * off-screen removal/wrap thresholds (objs_update()'s 128, octopus's
 * own 128 wrap), the dirty-rect clamp bounds (track_rect()'s 127/127),
 * the icon HUD's heart row (anchored to the right EDGE, 128->256, or
 * they'd now sit in the middle of the screen instead of the corner),
 * centre_print()'s own true-centre column (64->128), and TILE_ORIGIN_COL/
 * ROW itself (the tilemap's fixed (4,4)-tile hardware-addressing quirk,
 * Stage 2, still applies the same way, but the logical-canvas-centring
 * term it used to be added to is now (0,0) -- origin_x/origin_y are both
 * 0 once the logical canvas IS the full physical screen -- so the net
 * tilemap origin is just (4,4) now, not (12,8); reconfirmed empirically
 * the same way Stage 2 originally found it, not assumed to carry over
 * unchanged. */

#include <intrinsic.h>
#include <string.h>
#include "pico_video.h"
#include "pico_gfx.h"
#include "pico_sprite.h"
#include "pico_tilemap.h"
#include "pico_input.h"
#include "pico_math.h"
#include "pico_bank.h"
#include "picodiver_data.h"

/* Original Pico-8 Lua cart's own per-tick movement values, kept as their
 * own literal "1" constants -- not folded into a bigger number -- so
 * anyone porting another cart can diff their movement code against the
 * source and find the exact same value they expect, instead of an
 * unexplained "2" that looks like a typo or a hack.
 *
 * PICO_SPEED_SCALE is this engine's own, separate, explicit decision to
 * run faster than a literal Lua port would otherwise feel: this project
 * runs its tick loop at the Next's native 50Hz (see intrinsic_halt()
 * below), not Pico-8's own ~30fps, so a straight port of "move 1px/tick"
 * would already end up somewhat faster just from the tick rate alone --
 * but not as fast as the user confirmed felt right (docs/picodiver-notes.md:
 * "needs to move at about twice the speed" at the halved-25Hz baseline,
 * then re-confirmed again at native 50Hz as "the baseline speed"). Rather
 * than silently bake that into each object's own movement literal (which
 * is exactly what happened before this comment -- cab_x/cab_y moved by a
 * bare "2" with no indication why, and fish_x stayed at a bare "1",
 * inconsistent with it), this single named multiplier is applied
 * uniformly to every moving object's original-Lua delta, so the "why are
 * we faster than Pico-8" answer lives in one place, and a newly-ported
 * object's movement code can read "1" (matching its source) without
 * ending up unintentionally half-speed. */
/* 4, not the original 2 -- Stage 10 doubled the canvas width (128->256),
 * and this constant was never revisited for that. Two separate, both
 * real, effects compound here: (1) the player felt half as fast as
 * intended (confirmed by the user's own play) because the same
 * 1px/tick*2 delta now crosses a canvas twice as wide, taking twice as
 * many ticks edge-to-edge; (2) creatures/the octopus, moving at that
 * same now-too-slow pace, stay "in flight" for twice as many ticks
 * before reaching x>=SCREEN_W and despawning -- confirmed directly via
 * ZRCP (obj_count climbing from 3 to 6 over 20 seconds while ticks/sec
 * fell from ~4.5 to ~1.2, a real, measured, compounding feedback loop:
 * more objects in flight -> more per-tick collision/draw/erase work ->
 * fewer ticks/sec -> objects linger even longer in real time -> more
 * still accumulate). Doubling this constant again restores the
 * original's own "ticks to cross the canvas" ratio in both directions
 * at once -- same fix for both symptoms, since they share the same
 * root cause. */
#define PICO_SPEED_SCALE 4

/* Stage 10's full-resolution canvas -- the Next's full physical Layer 2
 * surface, not the original cart's native 128x128 (see this file's own
 * STAGE 10 comment). */
#define SCREEN_W 256
#define SCREEN_H 192

/* Mirrors the original cart's cabin-diver fields (docs/reference) --
 * only what Stage 1 actually needs: position, facing, and the walk
 * animation frame. */
static int16_t cab_x = 48, cab_y = 48;
static uint8_t cab_dr = 1;       /* 1 = facing right, 0 = facing left */
static uint8_t cab_img = 5;      /* current body sprite -- cycles ini..ini+ani while moving */
static const uint8_t cab_sp = 4; /* fin/paddle sprite, fixed */
static const uint8_t cab_ini = 5, cab_ani = 2;

/* The original's own survival state (docs/reference: cab.score/life/oxy,
 * titleinit()'s 0/3/10 initial values) plus a plain tick counter
 * (frame%N gates the oxygen drain/refill cadence below, same as the
 * original's own `frame` global). BUBBLELIMIT is the original's
 * level-0/no-level `bubblelimit` default -- level-based scaling (lower
 * limit, faster octopus, at higher levels) isn't built yet. */
#define BUBBLELIMIT 10
#define CAB_Y_SURFACE 32
static uint16_t score = 0;
static int16_t cab_oxy = 10;
static int16_t cab_life = 3;
static uint16_t frame = 1; /* starts at 1, not 0 -- avoids double-spawning a fish on the very first tick alongside the explicit initial obj_spawn(1) below, since frame%FISHNUM==0 would otherwise also fire at frame 0 */

static int16_t hud_last_oxy = -1, hud_last_life = -1, hud_last_score = -1;
static uint8_t hud_redraw_left = 0, score_redraw_left = 0;

/* Stage 7's own dirty-rect erase tracking -- pixel-granularity (not the
 * original's tile-granularity DirtyRect, which existed only because its
 * own erase redraws pico_map() tiles; this port's erase is just
 * pico_cls_rect() over the rectangle, so pixels are the natural unit).
 * `[2]`-indexed by buf_idx -- see this file's own STAGE 7 comment for
 * why. */
typedef struct { uint8_t x, y, w, h, valid; } DirtyRect;
static uint8_t buf_idx;

static void track_rect(DirtyRect *r, int16_t x, int16_t y, uint8_t w, uint8_t h) {
    int16_t x0 = x, y0 = y;
    int16_t x1 = (int16_t)(x + w - 1), y1 = (int16_t)(y + h - 1);
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > SCREEN_W - 1) x1 = SCREEN_W - 1;
    if (y1 > SCREEN_H - 1) y1 = SCREEN_H - 1;
    if (x0 > x1 || y0 > y1) { r->valid = 0; return; }
    r->x = (uint8_t)x0;
    r->y = (uint8_t)y0;
    r->w = (uint8_t)(x1 - x0 + 1);
    r->h = (uint8_t)(y1 - y0 + 1);
    r->valid = 1;
}

static void erase_rect(DirtyRect *r) {
    if (!r->valid) return;
    r->valid = 0;
    pico_cls_rect(r->x, r->y, (uint8_t)(r->x + r->w - 1), (uint8_t)(r->y + r->h - 1), 0);
}

static DirtyRect prev_cab[2];

static void cab_move(void) {
    cab_img++;
    if (cab_img > cab_ini + cab_ani) cab_img = cab_ini;
}

/* Tracks ONE combined rect covering both the body sprite and the +-8px
 * fin sprite regardless of facing -- matches the original's own
 * cabshow() exactly (docs/reference: "covers both... so next visit to
 * this buffer erases the right area whether cab.dr changed since or
 * not"), 24px wide (x-8..x+16) x 8px tall. */
static void cab_show(void) {
    pico_spr(cab_img, cab_x, cab_y, 1, 1, cab_dr, 0);
    if (cab_dr) pico_spr(cab_sp, (int16_t)(cab_x + 8), cab_y, 1, 1, cab_dr, 0);
    else        pico_spr(cab_sp, (int16_t)(cab_x - 8), cab_y, 1, 1, cab_dr, 0);
    track_rect(&prev_cab[buf_idx], (int16_t)(cab_x - 8), cab_y, 24, 8);
}

/* The real creature pool, replacing Stage 4's single hardcoded fish --
 * see this file's own STAGE 6 comment above. */
typedef struct {
    int16_t x, y;
    uint8_t sprstr, sprcur, sprend;
    uint8_t wid, hi, face;
} Obj;

#define MAX_OBJS 16
static Obj objs[MAX_OBJS];
static uint8_t obj_count;
static DirtyRect prev_objs[2][MAX_OBJS];

/* sprstr, sprcur(initial), sprend, wid, hi, face -- the original cart's
 * own creat[] table (docs/reference), verbatim, minus two columns: the
 * template's x/y fields (always the same placeholder values there,
 * never actually used -- obj_spawn() below sets real spawn position
 * directly, matching the original's own newobj()) and "ani" (never read
 * anywhere in gameupdate() -- the original's own animation-step update
 * reuses the WIDTH field instead, see objs_update()'s own comment). */
static const uint8_t creat[6][6] = {
    {   0,   0,   0, 0, 0, 0 }, /* 0: unused */
    {  65,  65,  66, 1, 1, 0 }, /* 1: fish */
    {  80,  80,  85, 2, 1, 0 }, /* 2: eel */
    {  96,  96, 104, 3, 2, 0 }, /* 3: dolphin/shark */
    { 105, 105, 108, 2, 2, 1 }, /* 4: piranha */
    {  73,  73,  76, 2, 2, 0 }, /* 5: big fish */
};
#define FISHNUM 45 /* ticks/fish spawn -- original's own level-0 default */

static void obj_spawn(uint8_t type) {
    Obj *o;
    if (obj_count >= MAX_OBJS) return;
    o = &objs[obj_count++];
    o->x = 1;
    /* 40..168 -- NOT 40..175 (the previous bound, confirmed wrong by the
     * user's own real-play testing: a 175-spawned, 2-cell/16px-tall
     * creature -- dolphin/shark, piranha, big fish -- reaches y=190,
     * visibly clipping ~8px into the sand floor tile, which starts at
     * y=184, i.e. OCEAN_ROWS-1 rows down from the ocean's own top at
     * y=32: 32 + (20-1)*8 = 184). 168 is the real safe ceiling: even the
     * tallest (16px) creature spawned there stops at y=184, exactly
     * meeting the sand's own top edge, never inside it. Scaled up from
     * the original's own 48+rnd(56) (48..103) for the 128-tall canvas
     * that range was sized for. */
    o->y = (int16_t)(40 + pico_rnd(129));
    o->sprstr = creat[type][0];
    o->sprcur = creat[type][1];
    o->sprend = creat[type][2];
    o->wid    = creat[type][3];
    o->hi     = creat[type][4];
    o->face   = creat[type][5];
}

static void obj_remove(uint8_t i) {
    objs[i] = objs[--obj_count];
}

/* Movement, animation and collision/removal for every active object in
 * the pool -- one pass each, matching the original's own gameupdate()
 * shape (docs/reference). References cab_img/cab_x/cab_y directly, same
 * as cab_move()/cab_show() above -- this file's own single-TU style. */
static void objs_update(void) {
    uint8_t i;

    for (i = 0; i < obj_count; i++) {
        objs[i].x = (int16_t)(objs[i].x + 1 * PICO_SPEED_SCALE);
    }

    /* Every 10 ticks, step sprcur by WID (not the unused "ani" field) and
     * loop back to sprstr past sprend -- preserved verbatim from the
     * original (docs/reference's own "what does this do?" comment on
     * this exact line). For wid>1 creatures this skips sprite indices
     * rather than animating through every frame -- looks like an
     * authoring quirk, not this port's place to "fix" it. */
    if (frame % 10 == 0) {
        for (i = 0; i < obj_count; i++) {
            objs[i].sprcur = (uint8_t)(objs[i].sprcur + objs[i].wid);
            if (objs[i].sprcur > objs[i].sprend) objs[i].sprcur = objs[i].sprstr;
        }
    }

    /* Collision + off-screen removal in one pass -- obj_remove() swaps
     * the last slot into i, so a removal re-checks the same index next
     * iteration instead of advancing past it (same pattern the original
     * uses here, docs/reference). sprstr<=66 is the original's own
     * "collectible" threshold (fish); >=67 creatures drain score instead
     * -- no life loss here, matching the original's own default (hard
     * mode, which alone makes certain creatures cost a life, isn't
     * implemented). */
    i = 0;
    while (i < obj_count) {
        uint8_t removed = 0;
        if (pico_spr_collide(cab_img, cab_x, cab_y, 1, 1,
                              objs[i].sprcur, objs[i].x, objs[i].y, objs[i].wid, objs[i].hi)) {
            if (objs[i].sprstr <= 66) score++;
            else if (score > 0) score--;
            obj_remove(i);
            removed = 1;
        } else if (objs[i].x >= SCREEN_W) {
            obj_remove(i);
            removed = 1;
        }
        if (!removed) i++;
    }
}

/* Erases every slot this buffer has EVER drawn into (0..MAX_OBJS-1, not
 * just 0..obj_count-1) before anything new is drawn -- obj_remove()'s
 * swap-with-last means a slot's logical object can change between
 * frames, so matching "slot N's old rect" against "slot N's current
 * object" would be wrong; erasing every previously-valid slot first,
 * unconditionally, sidesteps that entirely (same reasoning the
 * original's own back() uses, docs/reference: "Slots >= obj_count
 * intentionally untouched: back()'s erase pass already erased any that
 * were valid from a previous, larger obj_count in THIS buffer"). */
static void objs_erase(void) {
    uint8_t i;
    for (i = 0; i < MAX_OBJS; i++) erase_rect(&prev_objs[buf_idx][i]);
}

static void objs_show(void) {
    uint8_t i;
    for (i = 0; i < obj_count; i++) {
        pico_spr(objs[i].sprcur, objs[i].x, objs[i].y, objs[i].wid, objs[i].hi, objs[i].face, 0);
        track_rect(&prev_objs[buf_idx][i], objs[i].x, objs[i].y,
                   (uint8_t)(objs[i].wid * 8), (uint8_t)(objs[i].hi * 8));
    }
}

/* The octopus -- a separate fixed singleton (not pool-spawned), per the
 * original's own oct_x/oct_t globals (docs/reference). Drawn as two
 * sprites: a fixed head (11) and an animated body/tentacle sprite below
 * it (oct_t, cycling 12-14), both moving together. */
#define OCT_SPEED 5 /* ticks/move-and-animate step -- original's own level-0 default */
static int16_t oct_x = 1;
static uint8_t oct_t = 11;
static DirtyRect prev_oct_head[2];
static DirtyRect prev_oct_body[2];

static void oct_update(void) {
    if (frame % OCT_SPEED != 0) return;
    oct_x = (int16_t)(oct_x < SCREEN_W ? oct_x + 1 : 1);
    oct_t = (oct_t == 14) ? 12 : (uint8_t)(oct_t + 1);

    /* Continuous score drain while touching, every 20 ticks -- matches
     * pixel_collider_oct()'s own semantics exactly (distinct from a
     * fish's one-shot pickup-and-remove): the octopus is never removed,
     * it just keeps draining score for as long as contact lasts. Two
     * separate checks (head + the real animated body sprite) rather
     * than the original's own single 16px-tall read from sprite 11's
     * position alone -- see this file's STAGE 6 comment for why that's
     * not reproduced here. */
    if (frame % 20 == 0 && score > 0 &&
        (pico_spr_collide(cab_img, cab_x, cab_y, 1, 1, 11, oct_x, 32, 1, 1) ||
         pico_spr_collide(cab_img, cab_x, cab_y, 1, 1, oct_t, oct_x, 40, 1, 1))) {
        score--;
    }
}

static void oct_show(void) {
    pico_spr(11, oct_x, 32, 1, 1, 0, 0);
    pico_spr(oct_t, oct_x, 40, 1, 1, 0, 0);
    /* Two SEPARATE dirty rects (head 8x8 at y=32, tentacle 8x8 at y=40),
     * not one combined 8x16 -- the combined version (matching the
     * original's own track_rect(&prev_oct[buf_idx], oct_x, 32, 8, 16))
     * left a visible trail specifically on the tentacle while the head
     * stayed clean, confirmed via real play (not this project's own
     * automated ZRCP testing, which couldn't reproduce it reliably --
     * real hardware/human testing remains the authority here, same as
     * every other "feel"/visual-correctness check throughout this
     * project). Splitting them is a real behavioural change, not just a
     * bookkeeping one: each sprite's own erase now runs independently. */
    track_rect(&prev_oct_head[buf_idx], oct_x, 32, 8, 8);
    track_rect(&prev_oct_body[buf_idx], oct_x, 40, 8, 8);
}

/* Stage 10's full-resolution canvas IS the full physical screen
 * (pico_video_init(SCREEN_W,SCREEN_H) below), so pico_video.c's own
 * logical-canvas centring offset is (0,0) -- no border to account for
 * any more. The hardware tilemap still has its own fixed (4,4)-tile
 * addressing quirk on top of that regardless (confirmed empirically
 * back in Stage 2, when the logical canvas WAS 128x128 and this same
 * quirk pushed the naive (8,4) centring offset to (12,8) -- re-confirmed
 * here the same way, by screenshotting and measuring against the
 * player sprite's known-correct Layer 2 position, not assumed to carry
 * over unchanged just because the underlying quirk is the same one). */
#define TILE_ORIGIN_COL 4
#define TILE_ORIGIN_ROW 4

/* --- title screen (Stage 8) ----------------------------------------- */

#define MODE_TITLE 0
#define MODE_GAME 1
#define MODE_DEATH 2
#define MODE_WIN 3
static uint8_t mode = MODE_TITLE;

static int16_t sx = 1; /* ship scroll position -- original's own `sx` */
static int16_t prev_sx_drawn[2] = { -999, -999 };
static uint8_t last_mode_drawn[2] = { 0xFF, 0xFF };
static uint8_t game_entered[2] = { 0, 0 };

static void centre_print(const char *s, uint8_t y, uint8_t color) {
    uint8_t len = (uint8_t)strlen(s);
    /* SCREEN_W/2 -- PICO_FONT_CELL_W=4, so len*4/2 = len*2. */
    uint8_t x = (uint8_t)((SCREEN_W / 2) - (int16_t)(len * 2));
    pico_print(s, x, y, color);
}

/* Redraws the whole 16x4-tile ship banner at its current scroll offset
 * -- see this file's own STAGE 8 comment for why this is software-drawn
 * (not the hardware tilemap) and only called when `sx` has actually
 * changed, not every frame.
 *
 * Tile 9 (confirmed by reading its actual pixel data, not assumed) is a
 * flat, solid fill of Pico-8 colour 2 -- the sky behind the ships, not
 * a ship piece -- and makes up 42 of the 64 cells in this banner. Fills
 * the whole area directly with that colour instead of blitting it 42
 * times over, then only draws the real, distinct ship-piece tiles (the
 * other 22 cells) on top -- a real, measured fix: this redraw, run every
 * 30 ticks, was dragging the title screen's own tick rate down enough
 * to make the ship visibly scroll slower than intended (direct ZRCP
 * frame-counter measurement, not a guess -- the title screen, with
 * nothing else running, still only managed ~17 ticks/sec average, not
 * native 50Hz). */
static void ship_show(void) {
    uint8_t r, c;
    uint8_t prev_page;
    pico_cls_rect(0, 0, SCREEN_W - 1, 31, 2);
    /* 32 tile columns now (SCREEN_W/8), not the original 16 -- source
     * data only has 16 real columns (Stage 8's own reading of
     * picodiver_map_data), so c%16 wraps it to repeat twice across the
     * wider banner, same "it's a tileable pattern" reasoning as the
     * ocean's own extension below. picodiver_map_data is banked
     * (picodiver_data.h's own comment) -- paged in for this whole loop,
     * not per-cell, since every read in it is from the same page; the
     * interleaved pico_spr() calls are unaffected (picodiver_sprite_data
     * stays resident, a completely different address range from the
     * banked window). */
    prev_page = pico_bank_page_in(PICODIVER_MAP_PHYSICAL_PAGE);
    for (r = 0; r < 4; r++) {
        for (c = 0; c < SCREEN_W / 8; c++) {
            uint8_t tile = picodiver_map_data[(uint16_t)r * PICODIVER_MAP_W + (c % 16)];
            uint8_t screen_x = (uint8_t)(((uint16_t)c * 8 + (uint16_t)sx) % SCREEN_W);
            if (tile != 0 && tile != 9) pico_spr(tile, screen_x, (int16_t)(r * 8), 1, 1, 0, 0);
        }
    }
    pico_bank_page_out(prev_page);
}

/* Resets every piece of gameplay state back to a fresh start and drops
 * into the title screen -- matches the original's own titleinit()
 * (docs/reference), called once at startup here (our only entry point
 * into MODE_TITLE -- death doesn't return here yet, this stage's own
 * scope boundary, see Stage 5's comment on the same point). */
static void titleinit(void) {
    cab_x = 48; cab_y = 48; cab_dr = 1; cab_img = cab_ini;
    score = 0; cab_oxy = BUBBLELIMIT; cab_life = 3;
    obj_count = 0; obj_spawn(1); /* matches the original's own "start with a fish", unused until gameplay begins */
    oct_x = 1; oct_t = 11;
    sx = 1;
    frame = 1;
    mode = MODE_TITLE;
    prev_sx_drawn[0] = prev_sx_drawn[1] = -999;
    last_mode_drawn[0] = last_mode_drawn[1] = 0xFF;
    game_entered[0] = game_entered[1] = 0;
}

int main(void) {
    uint8_t col, row, i;
    char numbuf[8], linebuf[24];

    intrinsic_ei();
    pico_video_init(SCREEN_W, SCREEN_H);
    pico_spr_init(picodiver_sprite_data, PICODIVER_SHEET_W, PICODIVER_SHEET_H);
    /* Real Pico-8 colours for Layer 2 (the cabin/fish sprites), not the
     * Next's own default boot palette -- see pico_video_load_pico8_palette()'s
     * own comment: nothing in this project loaded Layer 2's actual RGB
     * values before this. Needed alongside the tilemap's own real-palette
     * load below so both layers agree on what Pico-8 colour 1 looks like --
     * the cabin/fish "background colour 1" blend trick (docs/picodiver-notes.md)
     * only works if they match. */
    pico_video_load_pico8_palette();

    pico_tilemap_init();
    /* The ocean background, built directly from the original cart's own
     * map data/sprite sheet -- not a hand-authored placeholder tile
     * anymore. Region (cel_x=0, cel_y=4, 16x12 cells) matches the
     * original cart's own back()'s pico_map(0, 4, 0, 32, 16, 12) call
     * exactly (docs/reference) -- same 192 tiles, just placed via the
     * hardware tilemap's own coordinate space (TILE_ORIGIN_COL/ROW)
     * instead of a Layer 2 screen-pixel destination. pico_tilemap_from_map()
     * handles the sheet's 8bpp-to-tilemap's-4bpp conversion, the real
     * Pico-8 palette load, and only defining the (3, for this cart's own
     * ocean) distinct tiles actually used -- see pico_tilemap.h's own
     * comment for the full contract.
     *
     * picodiver_map_data is banked (picodiver_data.h's own comment) --
     * paged in once for this call AND the ocean-extension loop right
     * after it (both read it, contiguous init-time code, one page-in/
     * out pair covers both). picodiver_sprite_data (also read here) is
     * resident, not banked, so it needs no page-in of its own. */
    {
    uint8_t map_prev_page = pico_bank_page_in(PICODIVER_MAP_PHYSICAL_PAGE);
    pico_tilemap_from_map(picodiver_sprite_data, PICODIVER_SHEET_W,
                           picodiver_map_data, PICODIVER_MAP_W, PICODIVER_MAP_H,
                           0, 4, 16, 12,
                           TILE_ORIGIN_COL, (uint8_t)(TILE_ORIGIN_ROW + 4));
    /* Stage 10: extend that same real ocean pattern to cover the full-
     * resolution canvas's own, much bigger ocean area (SCREEN_W/8
     * columns x OCEAN_ROWS rows) -- NOT a naive wrap of all 12 source
     * rows (row%12), which would put the sand/seafloor row (15) partway
     * down, with more "open water" appearing BELOW the seafloor -- never
     * makes sense visually. The source art's own structure (confirmed by
     * reading it, Stage 2) is really just two fixed transition rows (14:
     * the wave-edge, 15: the sand) sitting under an otherwise-uniform
     * deep-water fill (row 4, repeated 10x in the original's own 128-tall
     * data) -- so here the two transition rows stay pinned to the new,
     * much-lower true bottom, and the uniform deep-water tile simply
     * fills however many extra rows the taller canvas needs above them,
     * which is exactly what "the same ocean, just deeper" should look
     * like with no new art. */
    {
        uint8_t ocean_rows = (SCREEN_H - 32) / 8;
        for (row = 0; row < ocean_rows; row++) {
            uint8_t src_row;
            if (row == ocean_rows - 1) src_row = 15;      /* sand -- always the true bottom row */
            else if (row == ocean_rows - 2) src_row = 14; /* wave-edge -- just above the sand */
            else src_row = 4;                              /* deep water -- fills everything else */
            for (col = 0; col < SCREEN_W / 8; col++) {
                uint8_t tile = picodiver_map_data[(uint16_t)src_row * PICODIVER_MAP_W + (col % 16)];
                pico_tilemap_set((uint8_t)(TILE_ORIGIN_COL + col), (uint8_t)(TILE_ORIGIN_ROW + 4 + row), tile, 0);
            }
        }
    }
    pico_bank_page_out(map_prev_page);
    }
    /* Rows 0-3 (above the ocean -- the title/death/win screens' own
     * ship banner lives here, software-drawn over the top, Stage 8)
     * -- filled with the ocean's own plain deep-water tile (sprite 50,
     * already defined above) rather than left as undefined tilemap
     * memory, purely so gameplay's sky strip has no visibly broken/
     * garbage content behind its icon HUD. Spans the full SCREEN_W/8
     * columns now, not just the original 16. */
    for (row = 0; row < 4; row++)
        for (col = 0; col < SCREEN_W / 8; col++)
            pico_tilemap_set((uint8_t)(TILE_ORIGIN_COL + col), (uint8_t)(TILE_ORIGIN_ROW + row), 50, 0);

    titleinit();

    for (;;) {
        intrinsic_halt(); /* native 50Hz -- confirmed smooth at Stage 1, see docs/picodiver-notes.md */
        pico_input_update();

        if (mode != MODE_GAME) {
            /* All three static/menu screens (TITLE, DEATH, WIN) share
             * the same ship banner and "draw once per buffer per mode,
             * not every frame" shape -- matches the original's own
             * mode_is_static() grouping (docs/reference), just without
             * its hard-mode-select screen (not built, a separate ask). */
            if (frame % 30 == 1) { sx++; if (sx >= 128) sx = 1; }
            frame++;

            /* Static screen: drawn once per buffer, not every frame --
             * nothing else ever touches this buffer's text while a menu
             * screen is showing (same reasoning as the icon HUD/score
             * redraw-on-change below). last_mode_drawn[buf_idx] != mode
             * (not just "!= MODE_TITLE") also correctly forces a fresh
             * draw whenever the mode itself changes for this buffer --
             * e.g. DEATH showing right after GAME, or TITLE right after
             * DEATH. */
            if (last_mode_drawn[buf_idx] != mode) {
                pico_cls(0);
                /* All text kept below y=32 (the ship's own redraw zone,
                 * ship_show()) -- the original puts its title at y=2,
                 * INSIDE that zone, which works there only because it
                 * reprints all title text unconditionally every single
                 * frame (titledraw(), docs/reference), so the ship
                 * redrawing on top of it is never left showing a gap.
                 * This text draws once per buffer, not every frame
                 * (cheaper, matching Stage 7's own redraw-on-change
                 * philosophy) -- so it can't overlap anything the ship's
                 * own change-triggered redraw would blow away, or it'd
                 * vanish the first time sx changes after this draw,
                 * exactly what happened before this fix (confirmed via
                 * screenshot, not guessed). */
                if (mode == MODE_TITLE) {
                    centre_print("PICO-DIVER", 34, 7);
                    centre_print("CATCH FISH, AVOID THE OCTOPUS", 46, 10);
                    centre_print("DON'T RUN OUT OF AIR", 58, 10);
                    centre_print("PRESS X TO BEGIN", 84, 8);
                } else if (mode == MODE_DEATH) {
                    /* Matches deathdraw_text() (docs/reference) minus
                     * its hard-mode-only "you reached level N" line --
                     * no levels exist here. */
                    centre_print("GAME OVER", 34, 7);
                    pico_tostr_int((int16_t)(score * 5), numbuf);
                    strcpy(linebuf, "YOUR SCORE WAS ");
                    strcat(linebuf, numbuf);
                    centre_print(linebuf, 50, 10);
                    pico_tostr_int((int16_t)score, numbuf);
                    strcpy(linebuf, "YOU COLLECTED ");
                    strcat(linebuf, numbuf);
                    strcat(linebuf, " FISH");
                    centre_print(linebuf, 62, 10);
                    centre_print("PRESS X TO RESTART", 84, 8);
                } else { /* MODE_WIN */
                    /* Matches windraw() (docs/reference) minus its
                     * hard-mode branch (level==11, "1000 points, 200
                     * fish") -- always the non-hard "100 points, 20
                     * fish" message here. */
                    centre_print("YOU WIN", 34, 7);
                    centre_print("A PERFECT SCORE!", 50, 10);
                    centre_print("100 POINTS, 20 FISH", 62, 10);
                    centre_print("PRESS X TO RESTART", 84, 8);
                }
                last_mode_drawn[buf_idx] = mode;
                prev_sx_drawn[buf_idx] = -999; /* force a ship redraw too */
            }
            if (sx != prev_sx_drawn[buf_idx]) {
                ship_show();
                prev_sx_drawn[buf_idx] = sx;
            }

            if (pico_btnp(PICO_BTN_X)) {
                /* TITLE: no further reset needed -- titleinit() already
                 * set cab/score/oxy/life/the obj pool/the octopus to a
                 * fresh state, and nothing during a static screen
                 * touches any of it, so it's still correct now (matches
                 * the original's own gameinit(), which likewise just
                 * flips mode and nothing else). DEATH/WIN: titleinit()
                 * itself does the reset, same as the original's own
                 * "press x to restart" -> titleinit() on both screens. */
                if (mode == MODE_TITLE) mode = MODE_GAME;
                else titleinit();
            }
        } else { /* MODE_GAME */
            /* Max bounds scaled up for the full-resolution canvas (Stage
             * 10) -- same 8px margin convention as the original's own
             * [8,108] (in its 128-wide canvas), just sized for SCREEN_W/
             * SCREEN_H instead: 232 keeps the fin sprite (cab_x+16 at
             * its furthest) a few pixels clear of the right edge, 176
             * leaves cab's own 8px sprite height clear of the bottom. */
            if (pico_btn(PICO_BTN_LEFT))  { cab_dr = 0; cab_x = (int16_t)(cab_x - 1 * PICO_SPEED_SCALE); if (cab_x < 8) cab_x = 8; cab_move(); }
            if (pico_btn(PICO_BTN_RIGHT)) { cab_dr = 1; cab_x = (int16_t)(cab_x + 1 * PICO_SPEED_SCALE); if (cab_x > 232) cab_x = 232; cab_move(); }
            if (pico_btn(PICO_BTN_UP))    { cab_y = (int16_t)(cab_y - 1 * PICO_SPEED_SCALE); if (cab_y < CAB_Y_SURFACE) cab_y = CAB_Y_SURFACE; cab_move(); }
            if (pico_btn(PICO_BTN_DOWN))  { cab_y = (int16_t)(cab_y + 1 * PICO_SPEED_SCALE); if (cab_y > 176) cab_y = 176; cab_move(); }

            objs_update();
            oct_update();
            if (frame % FISHNUM == 0) obj_spawn(1);
            if (frame % 60 == 0) obj_spawn((uint8_t)(2 + pico_rnd(4)));

            /* Oxygen/life -- ported verbatim from gameupdate() (docs/
             * reference), "easy mode" (which disables this entirely) not
             * implemented, so this always applies, matching the original's
             * own default (easy=0). */
            if (cab_y == CAB_Y_SURFACE) {
                if (cab_oxy < BUBBLELIMIT && frame % 20 == 1) cab_oxy++;
            } else {
                if (frame % 30 == 1) cab_oxy--;
                if (cab_oxy == 0 && cab_life > 0) {
                    cab_oxy = BUBBLELIMIT;
                    cab_life--;
                }
            }
            frame++;

            /* Death/win -- matches gameupdate()'s own "if (cab.life==0)
             * mode=MODE_DEATH" / "if (cab.score==20) mode=MODE_WIN"
             * (docs/reference), minus its hard-mode branch (level
             * advancement instead of an immediate win) -- not built.
             * No state reset here -- the death/win screen's own "press
             * X to restart" calls titleinit(), which does the real
             * reset, same as the original. game_entered is cleared so
             * the NEXT time gameplay starts (after title again), it
             * gets its one-time full clear (above) to wipe THIS
             * session's leftover sprites, not stale game_entered=1 from
             * this one. */
            if (cab_life == 0) {
                mode = MODE_DEATH;
                game_entered[0] = game_entered[1] = 0;
            } else if (score == 20) {
                mode = MODE_WIN;
                game_entered[0] = game_entered[1] = 0;
            }

            /* One real pico_cls(0) per buffer, the first time gameplay
             * draws into it -- wipes the title screen's own leftover
             * Layer 2 content (text/ship), which nothing in the
             * dirty-rect system below knows about. After that, no more
             * full clears -- see this file's STAGE 7 comment. */
            if (!game_entered[buf_idx]) {
                pico_cls(0);
                game_entered[buf_idx] = 1;
            }
            erase_rect(&prev_cab[buf_idx]);
            erase_rect(&prev_oct_head[buf_idx]);
            erase_rect(&prev_oct_body[buf_idx]);
            objs_erase();

            /* Icon HUD/score -- redrawn for exactly the 2 frames right
             * after oxy/life/score actually changes (hud_redraw_left),
             * enough to hit both physical buffers once each (buf_idx
             * alternates every frame), then stops until the next real
             * change. */
            if (cab_oxy != hud_last_oxy || cab_life != hud_last_life) {
                hud_last_oxy = cab_oxy;
                hud_last_life = cab_life;
                hud_redraw_left = 2;
            }
            if (hud_redraw_left > 0) {
                pico_cls_rect(0, 0, SCREEN_W - 1, 7, 0);
                for (i = 1; i <= cab_oxy; i++) pico_spr(51, (int16_t)((i * 8) - 8), 0, 1, 1, 0, 0);
                /* Anchored to the right EDGE (SCREEN_W), not the old
                 * canvas's own width (128) -- that would now sit in the
                 * middle of the wider screen instead of the corner. */
                for (i = 1; i <= cab_life; i++) pico_spr(32, (int16_t)(SCREEN_W - (i * 8)), 0, 1, 1, 0, 0);
                hud_redraw_left--;
            }
            if ((int16_t)score != hud_last_score) {
                hud_last_score = (int16_t)score;
                score_redraw_left = 2;
            }
            if (score_redraw_left > 0) {
                pico_cls_rect(2, 10, 70, 17, 0); /* fixed, generous max-width erase for "SCORE: NNNN" */
                pico_print("SCORE:", 2, 10, 7);
                pico_tostr_int((int16_t)score, numbuf);
                pico_print(numbuf, 36, 10, 7);
                score_redraw_left--;
            }

            cab_show();
            objs_show();
            oct_show();
        }

        pico_flip();
        buf_idx ^= 1;
    }
}
