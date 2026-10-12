# __map__ / __gff__ converter -- findings

`tools/p8_map_to_c.py` closes a real, confirmed gap: CelesteNext
(`/Users/nat/Code/Games/CelesteNext-main`) -- a real shipped Pico-8-to-Next
port -- never actually built an automated level converter of its own (its
`zxnext/scripts/all_convert.py` has `convert_levels`/`convert_layer2`
commented out; room data was exported by hand/externally instead, per its
own leftover comments). Pico-8's own `__map__` format turned out not to
need one either, though -- it's not a bit-packed format like `__gfx__`
(4bpp) is, just plain 2-hex-digit tile indices, row-major, one byte per
cell (0-255, same range as a sprite index) -- confirmed directly against
two real carts (`/Users/nat/Code/Pico-8/celeste.p8`,
`/Users/nat/Code/Pico-8/Bootyful-Demake/booty-demake.p8`), not assumed
from the format's reputation: both decode to a clean 128x32 grid, and the
first few bytes of `celeste.p8`'s own output were hand-checked against the
cart's raw hex (`23 31 25` -> `35, 49, 37` decimal) before trusting the
rest.

## `__gff__` (sprite flags) comes along for free, and feeds directly into pico_colbox_at()

Pico-8's `__gff__` section is the same shape as `__map__` -- one hex-pair
byte per sprite index, up to 256 bytes, which is exactly `pico_fget_all()`/
`pico_fset_all()`'s own whole-byte convention (`pico_map.h`). The
converter emits both from one pass over a cart, so a ported game's own
startup can do `for (i...) pico_fset_all(i, flags[i]);` once, then feed
the result straight into `pico_map_build_colmask()` -- the collision-mask
work lifted from CelesteNext's own technique (`docs/pico8-vs-zxpicolua-
reference.md`'s "Map, tiles" section) and this converter were two separate
asks that turned out to compose directly, not a coincidence worth missing.

## Known limitation, not hit by either test cart

Pre-0.2.0 Pico-8 let a cart's map extend into the bottom half of the
`__gfx__` section's own memory (rows 32-63) when the sprite sheet didn't
need its full 128x128 area -- a legacy space-saving trick. **Not handled
here** -- the converter only reads the dedicated `__map__` section's own
rows. Neither test cart needed it (both are clean 128x32 `__map__`-only
maps); worth adding if a cart that actually uses the shared-memory trick
ever comes up, not built speculatively against a case neither reference
cart exercises.

## Output format

Emits a single `.c` file: `<symbol>_w`/`<symbol>_h` (map dimensions, in
cells), `<symbol>_data[]` (row-major tile indices, ready for
`pico_map_init()`), and `<symbol>_flags[]` (only if `__gff__` was
present -- NOT directly linkable into `pico_map.c`'s own internal flag
table, which is deliberately `static`; loop `pico_fset_all()` once at
startup instead, see `tools/p8_map_to_c.py`'s own top comment).
