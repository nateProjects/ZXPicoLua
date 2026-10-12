# Pico-8 Lua vs ZXPicoLua C -- side-by-side reference

A lookup table for porting a cart: for each Pico-8 built-in, what the
ZXPicoLua equivalent is called, how its signature differs, and a real call
site (mostly from `games/picodiver/main.c`, the only reference port with
enough call sites to be a representative example) showing the Lua and the
ported C side by side. Not a tutorial -- each library header (`lib/pico_*.h`)
has the full reasoning behind every deviation; this is the quick "what do I
type instead of X" document to have open while translating a cart line by
line.

Three differences apply almost everywhere below, so stated once instead of
repeated per row:

- **No optional arguments.** Every Pico-8 call that can omit a trailing
  argument (`spr(n,x,y)` defaulting `w,h` to 1; `print(s)` relying on a
  cursor) has an explicit-argument-only C form -- pass the default value
  yourself, or use the cursor-based sibling function named for that case
  (`pico_print_at_cursor`).
- **No return-by-value strings.** Anything Lua returns as a new string
  (`sub`, `tostr`, `chr`) instead writes into a caller-provided buffer.
- **No `nil`.** Where Pico-8 returns `nil` for an out-of-range/missing
  result (`ord`, a map read past the edge), the C side returns a sentinel
  (usually `0`) that may also be a legitimate real value -- check the
  input range yourself if that distinction matters at a call site.

## Core loop

| Pico-8 | ZXPicoLua | Notes |
|---|---|---|
| `function _init()` | your `main()`'s setup code, before the loop | no callback split -- one real `main()` |
| `function _update()` / `_update60()` | top of your `main()`'s `while` loop body | single fixed-rate loop, no 30/60fps switch |
| `function _draw()` | bottom of the same loop body, ending in `pico_flip()` | update/draw aren't separate callbacks; order is whatever you write |
| (implicit halt-and-repeat) | `intrinsic_halt()` then `pico_input_update()` | explicit each frame -- see `lib/pico_input.h` |

```c
/* games/picodiver/main.c -- the actual shape every game's loop takes */
for (;;) {
    intrinsic_halt();
    pico_input_update();
    pico_tick();
    /* ...update state from pico_btn()/pico_btnp()... */
    /* ...draw via pico_cls_rect()/pico_spr()/pico_print()... */
    pico_flip();
}
```

## Input

| Pico-8 | ZXPicoLua | Notes |
|---|---|---|
| `btn(i)` | `pico_btn(i)` | same meaning (currently held) |
| `btnp(i)` | `pico_btnp(i)` | edge-triggered only -- no Pico-8 auto-repeat-after-hold |
| `i` button constant | `PICO_BTN_LEFT/RIGHT/UP/DOWN/O/X` | `O`/`X` = Pico-8's default Z/X keys |
| (4-player `btn(i,p)`) | -- | single local player only, `lib/pico_input.h` |

```lua
-- islander-4.p8 style
if (btn(0)) x -= 1
```
```c
/* games/picodiver/main.c:862 */
if (pico_btn(PICO_BTN_LEFT)) { cab_dr = 0; cab_x = (int16_t)(cab_x - 1 * PICO_SPEED_SCALE); ... }
```

## Drawing: screen, colour, shapes

| Pico-8 | ZXPicoLua | Notes |
|---|---|---|
| `cls(c)` | `pico_cls(c)` | whole physical surface, border included |
| (no partial-clear builtin) | `pico_cls_rect(x0,y0,x1,y1,c)` | added because tile/sprite ports redraw only a dirty rect per frame |
| `pset(x,y,c)` | `pico_pset(x,y,c)` | unchecked, like real Pico-8 |
| `pget(x,y)` | `pico_pget(x,y)` | reads the back buffer -- see `lib/pico_video.h`'s double-buffering note |
| `color(c)` | `pico_color(c)` | sets state consumed by `PICO_CURRENT_COLOR` (below), not a silent global default like Lua's own omitted-colour-argument forms |
| `rect(x0,y0,x1,y1,[c])` | `pico_rect(x0,y0,x1,y1,c)` | `c` or `PICO_CURRENT_COLOR` |
| `rectfill(x0,y0,x1,y1,[c])` | `pico_rectfill(x0,y0,x1,y1,c)` | explicit `c` only -- see dither note below |
| `line(x0,y0,x1,y1,[c])` | `pico_line(x0,y0,x1,y1,c)` | `c` or `PICO_CURRENT_COLOR` |
| `circ`/`circfill` | `pico_circ`/`pico_circfill` | explicit `c` only |
| `oval`/`ovalfill` | `pico_oval`/`pico_ovalfill` | explicit `c` only; args are the bounding box, same as Pico-8 |
| `fillp(pattern)` | `pico_fillp(pattern, transparent)` | the fractional/transparent trick (`fillp(p + 0x0.8)`) is its own explicit bool argument instead |
| `camera(x,y)` | `pico_camera(x,y)` | offsets `pico_gfx.c`'s shapes/text only -- not sprites, not raw pset/cls (see `lib/pico_gfx.h`) |
| `cursor(x,y)` | `pico_cursor(x,y)` | |
| `print(s,[x,y,c])` | `pico_print(s,x,y,c)` **or** `pico_print_at_cursor(s,c)` | Lua's argument-count overloading splits into two named functions |

`PICO_CURRENT_COLOR` (0xFF) is the stand-in for Pico-8's own "colour
argument omitted, use whatever `color()` last set" behaviour -- but it only
works on `pico_rect`/`pico_line`/`pico_print`/`pico_print_at_cursor`.
`pico_rectfill`/`pico_circ(fill)`/`pico_oval(fill)` already spend the full
0-255 range on dither encoding (`color>15` = two-colour fill pattern), so
they always need a real explicit colour.

```lua
-- color(11) set earlier, in a different function
function centre_print(s,y)
 print(s,64-#s*2,y)
end
```
```c
/* color() set earlier via pico_color(11); this function never
 * needed to know or re-pass that colour -- the whole reason
 * PICO_CURRENT_COLOR exists (lib/pico_gfx.h) */
void centre_print(const char *s, uint8_t y) {
    pico_print(s, (uint8_t)(64 - strlen(s) * 2), y, PICO_CURRENT_COLOR);
}
```

## Sprites (software, Layer 2)

| Pico-8 | ZXPicoLua | Notes |
|---|---|---|
| `spr(n,x,y,[w,h,flip_x,flip_y])` | `pico_spr(n,x,y,w,h,flip_x,flip_y)` | every arg explicit |
| `sspr(sx,sy,sw,sh,dx,dy,[dw,dh,flip_x,flip_y])` | `pico_sspr(sx,sy,sw,sh,dx,dy,dw,dh,flip_x,flip_y)` | |
| `sget(x,y)` | `pico_sget(x,y)` | reads the sheet, not the screen |
| `sset(x,y,[c])` | -- | not implemented, no reference cart writes to its own sheet at runtime |
| `fget(n,[f])` / `fset(n,f,[v])` | `pico_fget(n,f)`/`pico_fset(n,f,v)` (one flag) or `pico_fget_all`/`pico_fset_all` (whole byte) | split like the colour-argument functions, for the same reason |
| `palt(c,[t])` | `pico_palt(c,t)` | multi-colour transparency, matches real Pico-8 |
| `pal(c0,c1,[p])` | `pico_pal(c0,c1)` / `pico_pal_reset()` | no `p=1` "screen palette" mode |
| (no built-in pixel collision) | `pico_spr_collide(n1,x1,y1,w1,h1,n2,x2,y2,w2,h2)` | dw817's method -- the same algorithm most carts hand-roll themselves |

```lua
-- picodiver.js style
spr(cab_img,cab_x,cab_y,1,1,cab_dr)
```
```c
/* games/picodiver/main.c:412 */
pico_spr(cab_img, cab_x, cab_y, 1, 1, cab_dr, 0);
```

## Map, tiles, and the hardware tilemap (no Pico-8 equivalent)

| Pico-8 | ZXPicoLua | Notes |
|---|---|---|
| `mget(x,y)` / `mset(x,y,v)` | `pico_mget(x,y)` / `pico_mset(x,y,v)` | out-of-range reads/writes are no-ops, like Pico-8 |
| `map(cx,cy,sx,sy,cw,ch,[layer])` | `pico_map(...)` / `pico_map_layer(...,layer_mask)` | software blit, one `pico_spr()` per cell, every frame |
| -- | `pico_tilemap_from_map(...)` + `pico_tilemap_init()` | **not a Lua function at all** -- a hardware tilemap background with zero per-frame redraw cost, for a static map too big/expensive to `map()` every frame (picodiver's ocean background; Harvester's whole world). Converts the same sheet/map data `pico_map()` would use, once, into tile patterns. |

The tilemap module exists because `map()`'s software redraw (one `spr()`
call per visible cell, every single frame) is fine for a small HUD strip
but not for a full-screen scrolling world -- see `lib/pico_tilemap.h` and
`docs/tilemap-viewport-notes.md`. There is deliberately no 1:1 Lua
equivalent to port from; this is a case where the native platform offers
something better than the thing being ported, not just a renamed function.

## Hardware sprites (no Pico-8 equivalent)

Pico-8 has exactly one sprite mechanism (`spr`/`sspr`, software, on its one
screen layer). The Next has a second, separate compositing layer -- real
hardware sprites, `lib/pico_hwsprite.h` -- that a port can use INSTEAD of
`pico_spr()` for moving objects, to get animation/movement with no
erase/redraw bookkeeping at all (no `DirtyRect`, no tearing, because the
hardware recomposites it, not your draw loop). There's no Lua call to list
here; the translation is architectural, not line-by-line: the original
cart's `spr(player_img,x,y,...)` call becomes a `pico_hwsprite_set(slot,
pattern,x,y,visible,flip_x)` call, and the heavy lifting (defining what each
`pattern` slot looks like) happens once at init via
`pico_hwsprite_set_pattern()`, not every frame. See Harvester's own port for
a worked example, and `docs/hwsprite-notes.md` for the mechanism.

## Math, strings, RNG

| Pico-8 | ZXPicoLua | Notes |
|---|---|---|
| `mid/min/max/abs` | `pico_mid/pico_min/pico_max/pico_abs` | |
| `sgn(x)` | `pico_sgn(x)` | `sgn(0)==1`, Pico-8's own quirk, preserved |
| `band/bor/bxor/bnot/shl/shr/lshr` | `pico_band/pico_bor/pico_bxor/pico_bnot/pico_shl/pico_shr/pico_lshr` | only useful porting call sites verbatim -- native code should just use `&|^~<<>>` |
| `srand(seed)` / `rnd(n)` | `pico_srand(seed)` / `pico_rnd(n)` | same reverse-engineered algorithm, same sequence for the same seed; `pico_rnd` returns the integer part only (`rnd(n)+flr()` collapsed into one call) |
| `flr(x)` | `pico_flr(x)` | operates on a `pico_fix_t` (Q16.16), not a plain number -- see Fixed-point below |
| `ceil(x)` | `pico_ceil(x)` | same |
| `tostr(val)` | `pico_tostr_int(val, buf)` | decimal integers only, no hex/flags forms |
| `sub(s,pos0,[pos1])` | `pico_sub(s,pos0,pos1,out)` | pass `INT16_MAX` for an omitted `pos1` |
| `chr(code)` | `pico_chr(code, out)` | single-code only, no `chr(97,98,99)` multi-arg form |
| `ord(s,[index])` | `pico_ord(s, index)` | `index` defaults to 1, not omittable -- pass `1` explicitly |
| `time()` / `t()` | `pico_tick()` (call once/frame) + `pico_time()` (read) | a raw tick counter, not fractional seconds -- re-tune any `% n` divisor against the Next's real tick rate |
| `sin/cos/atan2` | -- | not implemented; no reference cart has needed trig yet (would need a real fixed-point lookup table) |
| (Lua tables: `add/del/deli/all/foreach/pairs`) | -- | **not a library gap** -- this is a porting-methodology decision, not a missing function: translate a Lua object pool into a fixed-size C array (`objs[MAX_OBJS]`) with swap-with-last removal, per pool, the way `games/picodiver/main.c`'s own `obj_remove()` does it. See the project plan's own note on this. |

```lua
islander-4.p8 style
function sort_foragry()
 for i=1,#foragry do
  ...
 end
end
```
```c
/* No add()/del()/all() equivalent to call -- the pool itself is a
 * fixed array with its own count, ported once per object type: */
#define MAX_OBJS 16
static Obj objs[MAX_OBJS];
static uint8_t obj_count;
/* games/picodiver/main.c's own obj_remove(): swap the removed slot
 * with the last live one, decrement obj_count -- no shifting needed. */
```

## Fixed-point math (no Pico-8 equivalent, needed because of one)

Pico-8 numbers ARE 16.16 fixed-point already -- Lua carts never see this,
they just write `0.1` or `x/2`. A C port that needs the same fractional
accumulation (velocity build-up, `lerp()`, percentages) needs an explicit
type for it, since plain `int16_t` can't represent `0.1`:

| Pico-8 (implicit) | ZXPicoLua | Notes |
|---|---|---|
| a plain fractional number, e.g. `0.1` | `pico_fix_t` (`PICO_FIX_INT(n)` / `PICO_FIX_FRAC(num,den)`) | Q16.16, matches Pico-8's own native format exactly |
| `a*b` | `pico_fix_mul(a,b)` | needs a 64-bit intermediate; plain `*` overflows |
| `lerp(pos,tar,perc)` | `pico_fix_lerp(pos,tar,perc)` | direct port of islander-4.p8's own `lerp()`, in fixed-point |
| (an int, read from a fixed value) | `PICO_FIX_TO_INT(f)` / `pico_flr(f)` / `pico_ceil(f)` | |

```lua
-- islander-4.p8:492ish, update_player()
function lerp(pos,tar,perc) return pos+((tar-pos)*perc) end
...
hs=lerp(hs,0,fric)
```
```c
/* Harvester's own port */
static const pico_fix_t fric = PICO_FIX_FRAC(1, 10); /* 0.1 */
hs = pico_fix_lerp(hs, 0, fric);
```

## Persistent data

| Pico-8 | ZXPicoLua | Notes |
|---|---|---|
| `cartdata(id)` | `pico_cartdata(id)` | file-backed via esxDOS, not browser storage |
| `dget(i)` / `dset(i,v)` | `pico_dget(i)` / `pico_dset(i,v)` | RAM only -- does NOT write through to disk immediately |
| (implicit autosave) | `pico_cartdata_save()` | call explicitly (pause menu, level complete, ...) -- see `lib/pico_data.h` for why immediate-write-on-`dset` was deliberately dropped |

## Things with no Lua equivalent at all, specific to this platform

| ZXPicoLua | Why it exists |
|---|---|
| `pico_video_init(w,h)` | Pico-8 is always 128x128; a native port can declare the Next's full 256x192 (or any size) as its own logical canvas |
| `pico_video_load_pico8_palette()` / `pico_tilemap_load_pico8_palette()` | the Next's own default boot palette isn't Pico-8's colours -- opt in explicitly |
| `pico_flip()` | Pico-8's own display is always double-buffered by the engine; here it's an explicit per-frame call (and skipping it is a real, if subtle, way to get a tearing bug) |
| `pico_video_batch_begin/pset/end`, `pico_video_batch_row()` | per-pixel call overhead that doesn't exist in Pico-8's own native (not emulated-on-Z80) runtime -- exists purely because the generic `pico_pset()` path profiled too slow for a tile/sprite-heavy frame; `lib/pico_sprite.c`'s `sspr()` is the main internal user, not something a ported cart's own logic should normally call directly |
| `pico_bank_page_in/page_out` | z88dk classic mode's 64K resident-code ceiling has no Pico-8 equivalent (a cart's own storage limit is cartridge size, not RAM) -- large const tables (sprite sheets, maps) get paged in on demand instead of living resident the whole time |
| `pico_mul_z80n.asm` (not a called function) | a transparent hardware-multiply override, zero call-site changes anywhere -- listed here only so it isn't mistaken for a missing port of something |

## See also

- `lib/pico_*.h` -- the authoritative, fully-reasoned version of every row above (why each deviation exists, what was tried and rejected)
- `docs/*-notes.md` -- the bug-by-bug history behind the trickier modules (tearing fixes, banking, the -O3 ternary miscompile)
- [[zxpicolua_status]] -- current milestone status and the gap-analysis history that grew this API (picodiver, then the Bootyful Demake cart, then the full manual)
