# Banked code calls -- findings

`lib/pico_bank.h`'s page-in/page-out window had only ever been proven for
banked **data** (large const tables -- picodiver's own map data,
Harvester/Islander's sprite/map data). A real shipped Pico-8-to-Next port
(Celeste Classic, `/Users/nat/Code/Games/CelesteNext-main`) uses the same
kind of banking for **code** too, via a `*.wrap.c` idiom (page in, call the
real paged-out function, page out) -- genuinely useful for z88dk classic's
64K resident ceiling when banked data alone isn't enough headroom, but not
something this project had confirmed actually works for code, not just
data, before now.

## Confirmed: no new mechanism needed, the existing one already covers code

The Makefile's own `PAGE_<n>_<name>` section-rename trick (the pattern rule
next to `CBANK_*`) blindly substitutes `code_compiler`/`rodata_compiler`/
`data_compiler`/`bss_compiler` for the target section name -- it was never
actually data-specific, just never exercised on a file whose only content
was a function before. `games/bankcalltest/` (`GAME=bankcalltest`) proves
this directly: `banked_fn.c`'s `banked_multiply()` compiles and links
exactly like a banked data module -- confirmed via the `.map` file,
`_banked_multiply = $2000 ; addr, public, ..., PAGE_8_banked_fn`, same
address a banked data symbol would land at.

**Calling convention needs no new C-level helper either.** Because the
banked function is linked at a real, resident-while-paged-in address, an
ordinary forward declaration (`banked_fn.h`) plus a normal C call works --
there is no special "banked call" ABI to build. The whole pattern is just:

```c
uint8_t prev = pico_bank_page_in(SOME_PHYSICAL_PAGE);
result = some_banked_function(args...);   /* an ordinary call */
pico_bank_page_out(prev);
```

matching CelesteNext's own `*.wrap.c` shape exactly (store/page-in/call/
page-out/restore), just inline instead of factored into a separate wrapper
file -- factor it into one if a banked function has several call sites, the
same judgement call as any other repeated code.

## Verified, not just linked

A build succeeding and an address landing where expected isn't proof the
*call* actually executes correctly through a live page-in -- confirmed via
ZRCP screenshot instead: `games/bankcalltest/main.c` computes `37*53`
twice, once resident (`1961`, the known-correct baseline) and once via
`pico_bank_page_in(8)` + `banked_multiply(37,53)` + `pico_bank_page_out()`,
and prints both plus a PASS/FAIL comparison. Screenshot showed
`37*53 banked = 1961` and `PASS` -- the banked call genuinely executed
(not a stale/garbage return) and genuinely computed the right answer
through the paged-in window, not by coincidence.

## Caveats worth remembering before reusing this for something bigger

- **One physical page per banked module, same 8K ceiling as banked data**
  (`pico_bank.h`'s own existing caveat) -- a banked function that needs to
  call *another* banked function on a *different* page needs its own
  explicit page-in/out around that inner call too; nesting isn't automatic
  just because the outer call is already inside a page-in/out pair.
- **A banked function can't safely call back into resident code that
  itself assumes a *different* page is active in the same MMU window** --
  not an issue here (this project's banked data readers are always leaf
  calls), but worth checking before banking something that calls back out
  into library code.
- **Every const table in a banked file must be at FILE scope, not
  function-local.** A function-local `static const` array gets emitted
  INLINE in the function's own code by sccz80 -- the Makefile's section-
  rename script maps every section of the file into one, so that data
  sits in the middle of the banked code, and falling through it at
  runtime (or a branch landing past the function's real end) executes
  the data bytes as instructions instead. A real incident in a sibling
  project (Harvester, not caught by `bankcalltest`'s own trivially simple
  test function): a crash that only reproduced through the one code path
  that happened to reach a function-local const table, traced via
  resident-global progress markers read over ZRCP, not from any build
  error. String literals in a banked function are fine (sccz80 emits
  those at the file's end, not inline) -- only `static const` arrays/
  structs declared inside a function body are the hazard. Keep any const
  table a banked function needs at file scope.
- Reused physical page 8 (already proven reliable on this exact
  z88dk/ZEsarUX setup for data) for `bankcalltest`'s own build -- a
  different `.nex` entirely, so no conflict with picodiver's own page-8
  usage; still worth a fresh ZRCP check (not just reuse-and-assume) the
  first time a *new* page number is tried for code, same discipline as any
  new page number for data (`z88dk_banking_reference`).
- **Avoid physical pages 18-29 for any banked module, code or data.**
  Root-caused (2026-10-13, via a sibling project, Harvester/Islander,
  then independently reconfirmed directly against THIS project's own
  `pico_video.c`): `pico_video_init()` leaves Layer 2's main bank at its
  cold-boot default (bank 9 = pages 18-23) and allocates the shadow bank
  3 16K-banks further (bank 12 = pages 24-29), then clears both at
  startup -- a module banked onto any page in that range links fine,
  produces a byte-correct `.nex`, and then reads back all-zero at
  runtime, because Layer 2's own init silently clears it moments after
  boot. This is exactly the "page 18/20/22 mysteriously fail" story from
  `z88dk_banking_reference` -- the root cause wasn't known at the time,
  only found later. See `lib/pico_bank.h`'s own comment for the proven-
  safe alternative pages (8/12/14/16/17/30).
