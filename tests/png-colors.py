#!/usr/bin/env python3
"""Print a PNG's distinct colours as "count #rrggbb", commonest first.

Usage: png-colors.py FILE

Stdlib only: the suite cannot assume Pillow or ImageMagick on a build host.
"""

import struct
import sys
import zlib

CHANNELS = {0: 1, 2: 3, 3: 1, 4: 2, 6: 4}


def chunks(blob):
    off = 8
    while off < len(blob):
        (n,) = struct.unpack(">I", blob[off : off + 4])
        yield blob[off + 4 : off + 8], blob[off + 8 : off + 8 + n]
        off += 12 + n


def unfilter(raw, width, height, bpp, stride):
    """Undo the per-scanline filters; returns the concatenated raw scanlines."""
    out, prev, off = bytearray(), bytearray(stride), 0
    for _ in range(height):
        ftype, line = raw[off], bytearray(raw[off + 1 : off + 1 + stride])
        off += 1 + stride
        for i in range(stride):
            a = line[i - bpp] if i >= bpp else 0
            b = prev[i]
            c = prev[i - bpp] if i >= bpp else 0
            if ftype == 1:
                line[i] = (line[i] + a) & 0xFF
            elif ftype == 2:
                line[i] = (line[i] + b) & 0xFF
            elif ftype == 3:
                line[i] = (line[i] + (a + b) // 2) & 0xFF
            elif ftype == 4:
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                line[i] = (
                    line[i] + (a if pa <= pb and pa <= pc else b if pb <= pc else c)
                ) & 0xFF
            elif ftype != 0:
                raise SystemExit("unknown filter %d" % ftype)
        out += line
        prev = line
    return out


def samples(row, depth, count):
    """The first `count` samples of one unfiltered scanline."""
    if depth == 8:
        return list(row[:count])
    if depth == 16:
        return [row[i * 2] << 8 | row[i * 2 + 1] for i in range(count)]
    per, mask = 8 // depth, (1 << depth) - 1
    return [(row[i // per] >> (8 - depth * (i % per + 1))) & mask for i in range(count)]


def colors(path):
    blob = open(path, "rb").read()
    if blob[:8] != b"\x89PNG\r\n\x1a\n":
        raise SystemExit("%s: not a PNG" % path)
    plte, idat = b"", b""
    for kind, data in chunks(blob):
        if kind == b"IHDR":
            width, height, depth, ctype, _, _, interlace = struct.unpack(
                ">IIBBBBB", data
            )
        elif kind == b"PLTE":
            plte = data
        elif kind == b"IDAT":
            idat += data
    if interlace:
        raise SystemExit("%s: interlaced" % path)
    nchan = CHANNELS[ctype]
    stride = (width * nchan * depth + 7) // 8
    bpp = max(1, nchan * depth // 8)
    raw = unfilter(zlib.decompress(idat), width, height, bpp, stride)

    tally = {}
    for y in range(height):
        row = raw[y * stride : (y + 1) * stride]
        vals = samples(row, depth, width * nchan)
        for x in range(width):
            px = tuple(vals[x * nchan : (x + 1) * nchan])
            if ctype == 3:
                px = tuple(plte[px[0] * 3 : px[0] * 3 + 3])
            elif ctype in (0, 4):
                px = (px[0], px[0], px[0])
            tally[px[:3]] = tally.get(px[:3], 0) + 1
    return tally


def main():
    if len(sys.argv) != 2:
        raise SystemExit(__doc__)
    tally = colors(sys.argv[1])
    for px, n in sorted(tally.items(), key=lambda kv: (-kv[1], kv[0])):
        print("%d #%02x%02x%02x" % ((n,) + px))


if __name__ == "__main__":
    main()
