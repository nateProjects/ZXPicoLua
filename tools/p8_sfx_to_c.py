#!/usr/bin/env python3
"""Converts Pico-8 __sfx__ data to AY-3-8912 register data for
lib/pico_sfx_synth.h, and emits C source.

Text-format layout per sfx (168 hex digits = 84 bytes), confirmed against
the Pico-8 wiki's own Memory page and cross-checked byte-for-byte against
dansanderson/picotool's pico8/sfx/sfx.py (a trusted independent cart
parser) and by hand against a real cart (celeste.p8):
    byte 0:    editor mode + filter switches (ignored -- no AY equivalent)
    byte 1:    note duration, in 1/128ths of a second (one value, shared
               by all 32 notes in this sfx)
    byte 2:    loop range start (note index 0-63, but only 0-31 meaningful)
    byte 3:    loop range end
    bytes 4-83: 32 notes, 5 hex digits each:
        digits 0-1: pitch (0-63)
        digit 2:    waveform (0-7 built-in, 8-F = custom instrument from
                    sfx slot 0-7)
        digit 3:    volume (0-7)
        digit 4:    effect (0-7)

Pitch-to-frequency formula (NOT in the Pico-8 wiki's own text -- confirmed
against zepto8's own source, a byte-accurate C++ reimplementation,
src/pico8/sfx.cpp: `key_to_freq(key) = 440 * 2^((key - 33) / 12)`,
standard 12-tone equal temperament with pitch 33 = A4 = 440Hz):

    freq(pitch) = 440 * 2^((pitch - 33) / 12)

AY tone period (same formula z88dk's own <psg.h> psgT() macro uses for
__SPECTRUM__/__ZXN__ -- the classic 1773400Hz AY clock divided by 16):

    period = round(110837.5 / freq)

KNOWN, DELIBERATE FIDELITY LOSS (see docs/sfx-convert-notes.md for the
full reasoning): the AY-3-8912 only generates true square waves on its
tone channel, plus one shared noise generator -- there is no hardware
equivalent for Pico-8's sine/triangle/sawtooth/ringing/phaser waveforms.
Every built-in waveform except 6 (noise) collapses to "play a tone";
waveform 6 maps to the AY's noise generator instead. Custom waveforms
(8-F) are NOT synthesized (that would need real wavetable playback, a
separate, bigger undertaking) -- they fall back to a plain tone at the
note's own pitch, and the converter prints a warning listing which
sfx/notes this affected so it's not a silent downgrade.

Also NOT applied in this first pass: the per-note `effect` field (slide,
vibrato, drop, fade_in, fade_out, arp fast/slow) is parsed and discarded,
not synthesized -- see docs/sfx-convert-notes.md's own "Known limitations"
section. Every affected note still plays (as a plain static tone/noise at
its own pitch/volume), just without the effect's own pitch/volume motion.

Usage:
    python3 tools/p8_sfx_to_c.py cart.p8 --sfx 0 1 2 --out sfx_data.c --symbol my_sfx
    python3 tools/p8_sfx_to_c.py cart.p8 --all --out sfx_data.c --symbol my_sfx
"""
import argparse
import math
import sys

AY_CLOCK_OVER_16 = 110837.5  # matches <psg.h>'s own __SPECTRUM__ psgT(hz) macro
TICK_HZ = 50.0  # this project's own fixed per-frame tick rate (pico_tick())


def parse_cart_section(text, section_name):
    lines = text.splitlines()
    start = None
    for i, line in enumerate(lines):
        if line.strip() == "__%s__" % section_name:
            start = i + 1
            break
    if start is None:
        return []
    out = []
    for line in lines[start:]:
        stripped = line.strip()
        if stripped.startswith("__") and stripped.endswith("__") and len(stripped) > 4:
            break
        out.append(line.rstrip("\n"))
    return out


def pitch_to_ay_period(pitch):
    freq = 440.0 * (2.0 ** ((pitch - 33) / 12.0))
    period = int(round(AY_CLOCK_OVER_16 / freq))
    return max(1, min(4095, period))  # AY tone period is a 12-bit register


class Note(object):
    __slots__ = ("pitch", "waveform", "volume", "effect")

    def __init__(self, pitch, waveform, volume, effect):
        self.pitch = pitch
        self.waveform = waveform
        self.volume = volume
        self.effect = effect


class Sfx(object):
    __slots__ = ("id", "note_duration", "loop_start", "loop_end", "notes")


def parse_sfx_line(line, sfx_id):
    if len(line) < 168:
        return None
    note_duration = int(line[2:4], 16)
    loop_start = int(line[4:6], 16)
    loop_end = int(line[6:8], 16)
    notes = []
    for i in range(8, 168, 5):
        pitch = int(line[i:i + 2], 16)
        waveform = int(line[i + 2:i + 3], 16)
        volume = int(line[i + 3:i + 4], 16)
        effect = int(line[i + 4:i + 5], 16)
        notes.append(Note(pitch, waveform, volume, effect))
    sfx = Sfx()
    sfx.id = sfx_id
    sfx.note_duration = note_duration
    sfx.loop_start = loop_start
    sfx.loop_end = loop_end
    sfx.notes = notes
    return sfx


def duration_to_ticks(note_duration_128ths):
    ticks = int(round(note_duration_128ths * (TICK_HZ / 128.0)))
    return max(1, ticks)


def emit_sfx(f, symbol, sfx, warnings):
    ticks = duration_to_ticks(sfx.note_duration)
    f.write("const pico_sfx_note_t %s_%d_notes[32] = {\n" % (symbol, sfx.id))
    for i, note in enumerate(sfx.notes):
        if note.volume == 0:
            f.write("    {0, 0, 0}, /* rest */\n")
            continue
        is_noise = 1 if note.waveform == 6 else 0
        if note.waveform >= 8:
            warnings.append("sfx %d note %d: custom waveform %d (sfx slot %d) not synthesized, falling back to plain tone" % (sfx.id, i, note.waveform, note.waveform - 8))
            is_noise = 0
        if note.effect != 0:
            warnings.append("sfx %d note %d: effect %d not applied (static tone only)" % (sfx.id, i, note.effect))
        period = pitch_to_ay_period(note.pitch)
        ay_volume = int(round(note.volume * 15.0 / 7.0))
        f.write("    {%d, %d, %d},\n" % (period, ay_volume, is_noise))
    f.write("};\n")
    f.write("const pico_sfx_t %s_%d = { %s_%d_notes, 32, %d, %d, %d };\n\n" % (
        symbol, sfx.id, symbol, sfx.id, ticks, sfx.loop_start, sfx.loop_end))


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("cart", help=".p8 cart file")
    ap.add_argument("--out", required=True, help="output .c file")
    ap.add_argument("--symbol", default="p8_sfx", help="base name for emitted C symbols (default: p8_sfx)")
    group = ap.add_mutually_exclusive_group(required=True)
    group.add_argument("--sfx", type=int, nargs="+", help="sfx IDs to convert (0-63)")
    group.add_argument("--all", action="store_true", help="convert all 64 sfx slots")
    args = ap.parse_args()

    text = open(args.cart, "r", encoding="utf-8", errors="replace").read()
    lines = parse_cart_section(text, "sfx")
    if not lines:
        sys.exit("no __sfx__ section found in %s" % args.cart)

    ids = range(64) if args.all else args.sfx
    warnings = []
    sfxs = []
    for sfx_id in ids:
        if sfx_id < 0 or sfx_id >= len(lines):
            sys.exit("sfx %d out of range (cart has %d lines)" % (sfx_id, len(lines)))
        sfx = parse_sfx_line(lines[sfx_id], sfx_id)
        if sfx is None:
            sys.exit("sfx %d: line too short, not a valid sfx line" % sfx_id)
        sfxs.append(sfx)

    with open(args.out, "w") as f:
        f.write("/* Auto-generated by tools/p8_sfx_to_c.py from %s -- do not hand-edit. */\n" % args.cart)
        f.write("#include <stdint.h>\n")
        f.write('#include "pico_sfx_synth.h"\n\n')
        for sfx in sfxs:
            emit_sfx(f, args.symbol, sfx, warnings)

    print("wrote %s: %d sfx converted" % (args.out, len(sfxs)))
    for w in warnings:
        print("  warning: %s" % w, file=sys.stderr)


if __name__ == "__main__":
    main()
