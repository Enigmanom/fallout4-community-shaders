"""Translate the pinned Unified Water tile's stream, preserving geometry and UVs."""

from pathlib import Path
import struct


def convert(source: Path, destination: Path):
    data = source.read_bytes()
    offset = data.index(b"\n") + 1
    prefix = data[:offset]

    def read(fmt):
        nonlocal offset
        values = struct.unpack_from("<" + fmt, data, offset)
        offset += struct.calcsize("<" + fmt)
        return values[0] if len(values) == 1 else values

    def string(short=False):
        nonlocal offset
        length = read("B" if short else "I")
        value = data[offset:offset + length]
        offset += length
        return value

    version, endian, user, count, bs_version = read("IBIII")
    assert (version, endian, user, count, bs_version) == (0x14020007, 1, 12, 6, 100)
    exports = [string(True) for _ in range(3)]
    types = [string() for _ in range(read("H"))]
    indices = [read("H") for _ in range(count)]
    sizes = [read("I") for _ in range(count)]
    num_strings, maximum = read("II")
    strings = [string() for _ in range(num_strings)]
    assert read("I") == 0
    blocks = []
    for size in sizes:
        blocks.append(bytearray(data[offset:offset + size]))
        offset += size
    tail = data[offset:]
    assert [types[index] for index in indices] == [
        b"BSFadeNode", b"BSMultiBoundNode", b"BSTriShape",
        b"BSWaterShaderProperty", b"BSMultiBound", b"BSMultiBoundAABB",
    ]
    for index in [0, 1, 2]:
        # FO4: AV flags omit Skyrim's 0x80000 bit.
        flags = struct.unpack_from("<I", blocks[index], 12)[0]
        struct.pack_into("<I", blocks[index], 12, flags & ~0x80000)
    # FO4: NiNode no longer serializes its dynamic-effect array count.
    del blocks[0][80:84]
    del blocks[1][80:84]
    shape = blocks[2]
    descriptor = struct.unpack_from("<Q", shape, 100)[0]
    triangles, vertices = struct.unpack_from("<HH", shape, 108)
    assert (triangles, vertices) == (2, 4)
    vertex_bytes = bytes(shape[116:228])
    triangle_bytes = bytes(shape[228:240])
    assert struct.unpack("<6H", triangle_bytes) == (0, 1, 2, 2, 3, 0)
    assert [struct.unpack_from("<3f", vertex_bytes, i * 28) for i in range(4)] == [
        (-2048.0, -2048.0, 0.0), (2048.0, -2048.0, 0.0),
        (2048.0, 2048.0, 0.0), (-2048.0, 2048.0, 0.0),
    ]
    # FO4: SSE's float positions require the full-precision attribute and a uint32 triangle count.
    descriptor |= 0x400 << 44
    blocks[2] = shape[:100] + struct.pack("<QIHI", descriptor, triangles, vertices, 124) + vertex_bytes + triangle_bytes
    # FO4: no SSE particle-data suffix; stream130 adds a Max Filepath export string.
    exports.append(b"\0")
    result = bytearray(prefix + struct.pack("<IBIII", version, endian, user, count, 130))
    for text in exports:
        result += struct.pack("<B", len(text)) + text
    result += struct.pack("<H", len(types))
    for text in types:
        result += struct.pack("<I", len(text)) + text
    result += struct.pack("<6H", *indices)
    result += struct.pack("<6I", *(len(block) for block in blocks))
    result += struct.pack("<II", num_strings, maximum)
    for text in strings:
        result += struct.pack("<I", len(text)) + text
    result += struct.pack("<I", 0)
    result += b"".join(blocks) + tail
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_bytes(result)
    print(f"{source} -> {destination}: four vertices, two triangles, identical vertex/UV payload")


if __name__ == "__main__":
    root = Path(__file__).resolve().parents[1]
    convert(
        root / "extern/community-shaders-shared/features/Unified Water/meshes/water/WaterMesh.nif",
        root / "package/Meshes/Water/FO4UnifiedWater.nif",
    )
