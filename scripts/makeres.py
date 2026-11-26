"""Generate a COFF object holding RT_MANIFEST + RT_VERSION without rc.exe.

`zig rc` is unusable in some locked-down environments (it re-executes the zig
binary), and lld-link rejects the hand written .res dialect, so this script
emits an ordinary i386 COFF object with a `.rsrc` section instead. Both
lld-link and link.exe accept such objects, which keeps the MSVC and the zig
builds on the same code path.

Usage: python makeres.py <output.obj> [--manifest app.manifest] [--version 2.0.0.0]
"""
import argparse
import struct

RT_ICON = 3
RT_GROUP_ICON = 14
RT_VERSION = 16
RT_MANIFEST = 24
LANG_EN_US = 0x0409
CODE_PAGE = 0

IMAGE_SCN_CNT_INITIALIZED_DATA = 0x00000040
IMAGE_SCN_MEM_READ = 0x40000000
IMAGE_REL_I386_DIR32NB = 0x0007  # RVA, no image base added


def pad(data: bytes, alignment: int = 4) -> bytes:
    return data + b"\0" * (-len(data) % alignment)


def resource_tree(resources):
    """Build a .rsrc section image.

    `resources` is a list of (type_id, [(name_id, [(lang_id, payload)])]) and the
    result is (section_bytes, [offset_of_each_data_entry_offset_field]).
    """
    root_entries = 16 + 8 * len(resources)
    type_dirs, name_dirs, data_entries = [], [], []
    cursor = root_entries
    for _type_id, names in resources:
        type_dirs.append(cursor)
        cursor += 16 + 8 * len(names)
    for _type_id, names in resources:
        for _name_id, langs in names:
            name_dirs.append(cursor)
            cursor += 16 + 8 * len(langs)
    for _type_id, names in resources:
        for _name_id, langs in names:
            data_entries.append(cursor)
            cursor += 16 * len(langs)
    payload_start = cursor

    body = bytearray()
    # Directory tables first, in the same order the offsets were assigned.
    def directory(entries):
        out = struct.pack("<IIHHHH", 0, 0, 0, 0, 0, len(entries))
        for name, target, is_dir in entries:
            out += struct.pack("<II", name, (0x80000000 | target) if is_dir else target)
        return out

    body += directory([(t, type_dirs[i], True) for i, (t, _n) in enumerate(resources)])

    index = 0
    for _type_id, names in resources:
        body += directory([(n, name_dirs[index + i], True) for i, (n, _l) in enumerate(names)])
        index += len(names)
    index = 0
    for _type_id, names in resources:
        for _name_id, langs in names:
            body += directory([(lang, data_entries[index], False) for lang, _p in langs])
            index += 1

    data_entry_slots = []
    payloads = []
    payload_offsets = []
    offset = payload_start
    for _type_id, names in resources:
        for _name_id, langs in names:
            for lang, payload in langs:
                payload_offsets.append(offset)
                payloads.append(pad(payload, 8))
                data_entry_slots.append(len(body))
                body += struct.pack("<IIII", offset, len(payload), CODE_PAGE, 0)
                offset += len(payloads[-1])
    for payload in payloads:
        body += payload
    return bytes(body), data_entry_slots


def version_block(key: str, value: bytes, wtype: int, children: bytes = b"") -> bytes:
    """One VS_VERSIONINFO node: wLength, wValueLength, wType, szKey, value, children."""
    key_bytes = key.encode("utf-16-le") + b"\0\0"
    head = struct.pack("<HHH", 0, 0, wtype) + key_bytes
    head += b"\0" * (-len(head) % 4)
    value_length = (len(value) // 2) if wtype == 1 else len(value)
    node = head + value + children
    # wLength excludes trailing padding, but must cover every byte before it.
    unpadded = len(head) + len(value) + len(children)
    node = struct.pack("<HHH", unpadded, value_length, wtype) + node[6:]
    return pad(node)


def version_info(file_version=(2, 0, 0, 0), strings=None) -> bytes:
    strings = strings or {}
    fixed = struct.pack(
        "<13I",
        0xFEEF04BD,  # dwSignature
        0x00010000,  # dwStrucVersion
        (file_version[0] << 16) | file_version[1],
        (file_version[2] << 16) | file_version[3],
        (file_version[0] << 16) | file_version[1],
        (file_version[2] << 16) | file_version[3],
        0x3F,        # dwFileFlagsMask
        0,           # dwFileFlags
        0x00040004,  # dwFileOS = VOS_NT_WINDOWS32
        1,           # dwFileType = VFT_APP
        0,           # dwFileSubtype
        0,           # dwFileDateMS
        0,           # dwFileDateLS
    )
    table = b""
    for key, value in strings.items():
        table += version_block(key, value.encode("utf-16-le") + b"\0\0", 1)
    string_file_info = version_block("StringFileInfo", b"", 1, version_block("040904B0", b"", 1, table))
    translation = version_block(
        "VarFileInfo", b"", 1, version_block("Translation", struct.pack("<HH", LANG_EN_US, 1200), 0)
    )
    return version_block("VS_VERSION_INFO", fixed, 0, string_file_info + translation)


def coff_object(section: bytes, reloc_offsets, name=b".rsrc") -> bytes:
    """Serialise a one-section i386 COFF object that relocates DIR32 slots."""
    section_header_size = 40
    header_size = 20
    raw_ptr = header_size + section_header_size
    reloc_ptr = raw_ptr + len(section)
    reloc_bytes = b"".join(
        struct.pack("<IIH", offset, 0, IMAGE_REL_I386_DIR32NB) for offset in reloc_offsets
    )
    symtab_ptr = reloc_ptr + len(reloc_bytes)
    symbols = struct.pack("<8sIhHBB", name.ljust(8, b"\0"), 0, 1, 0, 3, 0)
    strings = struct.pack("<I", 4)  # empty string table
    header = struct.pack("<HHIIIHH", 0x014C, 1, 0, symtab_ptr, 1, 0, 0)
    section_header = struct.pack(
        "<8sIIIIIIHHI",
        name.ljust(8, b"\0"), 0, 0, len(section), raw_ptr, reloc_ptr, 0,
        len(reloc_offsets), 0,
        IMAGE_SCN_CNT_INITIALIZED_DATA | IMAGE_SCN_MEM_READ,
    )
    return header + section_header + section + reloc_bytes + symbols + strings


def icon_resources(path):
    """Split a .ico into RT_ICON images plus the RT_GROUP_ICON directory."""
    with open(path, "rb") as handle:
        data = handle.read()
    _reserved, _kind, count = struct.unpack_from("<HHH", data, 0)
    icons, entries = [], []
    cursor = 6
    for index in range(count):
        width, height, colors, _pad, planes, bits, size, offset = struct.unpack_from("<BBBBHHII", data, cursor)
        cursor += 16
        icons.append((index + 1, [(LANG_EN_US, data[offset:offset + size])]))
        entries.append((width, height, colors, 0, planes, bits, size, index + 1))
    group = struct.pack("<HHH", 0, 1, count) + b"".join(struct.pack("<BBBBHHIH", *entry) for entry in entries)
    return icons, group


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("output")
    parser.add_argument("--manifest")
    parser.add_argument("--icon")
    parser.add_argument("--version", default="2.0.2.0")
    parser.add_argument("--product", default="ClassRollCall")
    parser.add_argument("--description", default="ClassRollCall classroom roll call")
    parser.add_argument("--copyright", default="Copyright (c) 2026 enderLuoxh")
    args = parser.parse_args()

    parts = [int(p) for p in args.version.split(".")]
    parts += [0] * (4 - len(parts))
    resources = []
    if args.icon:
        icons, group = icon_resources(args.icon)
        resources.append((RT_ICON, icons))
        resources.append((RT_GROUP_ICON, [(1, [(LANG_EN_US, group)])]))
    if args.manifest:
        with open(args.manifest, "rb") as handle:
            resources.append((RT_MANIFEST, [(1, [(LANG_EN_US, handle.read())])]))
    resources.append((RT_VERSION, [(1, [(LANG_EN_US, version_info(tuple(parts), {
        "FileDescription": args.description,
        "FileVersion": args.version,
        "LegalCopyright": args.copyright,
        "OriginalFilename": "ClassRollCall.exe",
        "ProductName": args.product,
        "ProductVersion": args.version,
    }))])]))

    # Windows looks up numeric resource types with a binary search. Each type
    # must occur once, and all directory entries must be in ascending order.
    resources.sort(key=lambda resource: resource[0])
    section, slots = resource_tree(resources)
    with open(args.output, "wb") as handle:
        handle.write(coff_object(section, slots))
    print("wrote %s (%d byte section, %d relocations)" % (args.output, len(section), len(slots)))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
