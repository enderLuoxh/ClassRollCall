"""Draw the application icon and emit a multi-size .ico.

The icon is a rounded square in the Windows accent blue with a white check mark:
it reads as "attendance recorded" even at 16 px on the taskbar.
"""
import io
import struct
import sys

from PIL import Image, ImageDraw

SIZES = [16, 20, 24, 32, 40, 48, 64, 128, 256]
STEPS = 4


def render(size: int) -> Image.Image:
    canvas = size * STEPS
    image = Image.new("RGBA", (canvas, canvas), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)

    # Rounded square with a soft vertical gradient.
    radius = int(canvas * 0.225)
    top = (34, 122, 214, 255)
    bottom = (12, 84, 172, 255)
    mask = Image.new("L", (canvas, canvas), 0)
    ImageDraw.Draw(mask).rounded_rectangle([0, 0, canvas - 1, canvas - 1], radius=radius, fill=255)
    gradient = Image.new("RGBA", (canvas, canvas))
    pixels = gradient.load()
    for y in range(canvas):
        t = y / max(1, canvas - 1)
        color = tuple(int(top[i] + (bottom[i] - top[i]) * t) for i in range(3)) + (255,)
        for x in range(canvas):
            pixels[x, y] = color
    image.paste(gradient, (0, 0), mask)

    # Check mark, drawn as a thick round-capped polyline.
    points = [(0.255, 0.520), (0.425, 0.690), (0.755, 0.300)]
    width = int(canvas * 0.105)
    for index in range(len(points) - 1):
        x0, y0 = (points[index][0] * canvas, points[index][1] * canvas)
        x1, y1 = (points[index + 1][0] * canvas, points[index + 1][1] * canvas)
        draw.line([(x0, y0), (x1, y1)], fill=(255, 255, 255, 255), width=width)
    radius_dot = width // 2
    for fx, fy in (points[0], points[-1], points[1]):
        cx, cy = fx * canvas, fy * canvas
        draw.ellipse([cx - radius_dot, cy - radius_dot, cx + radius_dot, cy + radius_dot], fill=(255, 255, 255, 255))

    return image.resize((size, size), Image.LANCZOS)


def bmp_entry(image: Image.Image) -> bytes:
    """32bpp bottom-up BMP as stored inside an ICO (XOR mask + empty AND mask)."""
    width, height = image.size
    header = struct.pack("<IiiHHIIiiII", 40, width, height * 2, 1, 32, 0, width * height * 4, 0, 0, 0, 0)
    rows = []
    for y in range(height - 1, -1, -1):
        row = bytearray()
        for x in range(width):
            r, g, b, a = image.getpixel((x, y))
            row += bytes((b, g, r, a))
        rows.append(bytes(row))
    mask_row = bytes(((width + 31) // 32) * 4)
    return header + b"".join(rows) + mask_row * height


def entry_bytes(image: Image.Image) -> bytes:
    """Large sizes are stored as PNG so the executable stays small."""
    if image.size[0] < 64:
        return bmp_entry(image)
    buffer = io.BytesIO()
    image.save(buffer, format="PNG", optimize=True)
    return buffer.getvalue()


def main() -> int:
    output = sys.argv[1] if len(sys.argv) > 1 else "app.ico"
    preview = sys.argv[2] if len(sys.argv) > 2 else None
    images = [render(size) for size in SIZES]
    entries = [(image, entry_bytes(image)) for image in images]

    header = struct.pack("<HHH", 0, 1, len(entries))
    offset = 6 + 16 * len(entries)
    directory = b""
    body = b""
    for image, data in entries:
        width = 0 if image.size[0] >= 256 else image.size[0]
        directory += struct.pack("<BBBBHHII", width, width, 0, 0, 1, 32, len(data), offset)
        offset += len(data)
        body += data
    with open(output, "wb") as handle:
        handle.write(header + directory + body)
    if preview:
        render(256).save(preview)
    print("wrote %s (%d sizes, %d bytes)" % (output, len(entries), 6 + 16 * len(entries) + len(body)))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())