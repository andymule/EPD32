#!/usr/bin/env python3
"""Convert a binary PGM (P5) to a scaled grayscale PNG with a viewing frame.
Dependency-free (stdlib zlib only). Usage: pgm2png.py in.pgm out.png"""
import sys, zlib, struct


def read_pgm(path):
    with open(path, "rb") as f:
        data = f.read()
    assert data[:2] == b"P5", "not a P5 PGM"
    # parse header: P5 <w> <h> <maxval>\n then raw bytes
    idx = 2
    fields = []
    while len(fields) < 3:
        while idx < len(data) and data[idx] in b" \t\r\n":
            idx += 1
        start = idx
        while idx < len(data) and data[idx] not in b" \t\r\n":
            idx += 1
        fields.append(int(data[start:idx]))
    idx += 1  # single whitespace after maxval
    w, h, _ = fields
    pix = data[idx:idx + w * h]
    return w, h, pix


def scale_and_frame(w, h, pix, scale, margin, frame=120, bg=210):
    W = w * scale + 2 * margin
    H = h * scale + 2 * margin
    out = bytearray([bg]) * (W * H)
    # frame rectangle just outside the panel
    for x in range(margin - 1, margin + w * scale + 1):
        out[(margin - 1) * W + x] = frame
        out[(margin + h * scale) * W + x] = frame
    for y in range(margin - 1, margin + h * scale + 1):
        out[y * W + (margin - 1)] = frame
        out[y * W + (margin + w * scale)] = frame
    for yy in range(h):
        for xx in range(w):
            val = pix[yy * w + xx]
            for sy in range(scale):
                row = (margin + yy * scale + sy) * W
                base = margin + xx * scale
                for sx in range(scale):
                    out[row + base + sx] = val
    return W, H, bytes(out)


def write_png(path, w, h, gray):
    def chunk(typ, data):
        c = typ + data
        return struct.pack(">I", len(data)) + c + struct.pack(">I", zlib.crc32(c) & 0xFFFFFFFF)

    raw = bytearray()
    for y in range(h):
        raw.append(0)  # filter type 0
        raw.extend(gray[y * w:(y + 1) * w])
    png = b"\x89PNG\r\n\x1a\n"
    png += chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 0, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(bytes(raw), 9))
    png += chunk(b"IEND", b"")
    with open(path, "wb") as f:
        f.write(png)


def main():
    inp, outp = sys.argv[1], sys.argv[2]
    w, h, pix = read_pgm(inp)
    maxdim = max(w, h)
    scale = max(1, round(680 / maxdim))
    W, H, gray = scale_and_frame(w, h, pix, scale, margin=16)
    write_png(outp, W, H, gray)
    print(f"{outp}: {w}x{h} -> {W}x{H} (scale {scale})")


if __name__ == "__main__":
    main()
