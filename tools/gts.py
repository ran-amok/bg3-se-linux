#!/usr/bin/env python3
"""Read a Graphine GTS virtual-texture tile set: header, levels, page-file table, FourCC strings.

    gts.py info FILE            header fields, levels, every page-file entry, top-level
                                FourCC String values; exit 1 if the file is malformed
    gts.py --selftest           calibration (below)

WHY THIS EXISTS

The extender merges the tile sets a mod ships (Mods/<Dir>/ScriptExtender/VirtualTextures.json)
into one SEMergedTileSet_N.gts in the game's Data directory (VirtualTextureMerge.inl). Every
string in the format is UTF-16 -- the 256-unit page-file name, the FourCC 'String' values --
which upstream spells wchar_t, 2 bytes on Windows and 4 here. Built that way the page-file
entry is 1048 bytes where the file has 536, so the source tables are misread and the merged
file is written with 4-byte characters the game cannot open, and a mod's virtual-textured
bodies render black. This reader is the check on both sides: a real tile set
must parse (the calibration), and a merged one written by the extender must parse the same
way. It reads the format as VirtualTextureFormat.h declares it, independently of the C++.

CALIBRATION (--selftest, two-sided)

The untouched input is a tile set the game ships, extracted from Data/VirtualTextures.pak
(Generated/Public/VirtualTextures/Albedo_Normal_Physical_6.gts; lspk.py does the extract);
it must parse with a 536-byte page-file stride, every name UTF-16 ending in '.gtp', and
FourCC strings that decode. Then two planted defects, each of which must be refused: the
page-file table rewritten the way a 4-byte wchar_t build writes it (1048-byte entries,
UTF-32 names), and a FourCC String value rewritten with 4-byte characters.
"""
import os, struct, subprocess, sys, tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
GAME_DATA = os.path.expanduser("~/.local/share/Steam/steamapps/common/Baldurs Gate 3/Data")
CAL_PAK = os.path.join(GAME_DATA, "VirtualTextures.pak")
CAL_GTS = "Generated/Public/VirtualTextures/Albedo_Normal_Physical_6.gts"

# GTSHeader, #pragma pack(1), as VirtualTextureFormat.h declares it (192 bytes)
HDR_FMT = '<III16sIQIQiiiIIQIIIQ' + 'I' * 8 + 'IQIQIQQ' + 'I' * 4
HDR_NAMES = ['Magic', 'Version', 'Unused', 'GUID', 'NumLayers', 'LayersOffset', 'NumLevels',
             'LevelsOffset', 'TileWidth', 'TileHeight', 'TileBorder', 'I2', 'NumFlatTileInfos',
             'FlatTileInfoOffset', 'I6', 'I7', 'NumPackedTileIDs', 'PackedTileIDsOffset',
             'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'PageSize', 'NumPageFiles',
             'PageFileMetadataOffset', 'FourCCListSize', 'FourCCListOffset',
             'ParameterBlockHeadersCount', 'ParameterBlockHeadersOffset', 'ThumbnailsOffset',
             'XJJ', 'XKK', 'XLL', 'XMM']
MAGIC = 0x47505247          # 'GRPG'
PAGEFILE_SIZE = 536         # char16_t[256] + u32 + Guid + u32
FMT_NODE, FMT_STRING, FMT_INT = 1, 2, 3


class Malformed(Exception):
    pass


def tag_str(t):
    return struct.pack('<I', t).decode('latin-1')     # the file stores the tag's characters in order


def parse(data):
    if len(data) < struct.calcsize(HDR_FMT):
        raise Malformed("shorter than a GTS header")
    h = dict(zip(HDR_NAMES, struct.unpack_from(HDR_FMT, data, 0)))
    if h['Magic'] != MAGIC:
        raise Malformed(f"magic {h['Magic']:#x} is not GRPG")
    if h['Version'] != 5:
        raise Malformed(f"version {h['Version']} (expected 5)")
    levels = []
    for i in range(h['NumLevels']):
        w, hh, off = struct.unpack_from('<IIQ', data, h['LevelsOffset'] + 16 * i)
        levels.append((w, hh, off))
    # page-file table: consecutive 536-byte entries, UTF-16 name, NumPages, Guid, F
    pfs = []
    off = h['PageFileMetadataOffset']
    end = off + PAGEFILE_SIZE * h['NumPageFiles']
    if end > len(data):
        raise Malformed("page-file table runs past the end of the file")
    for i in range(h['NumPageFiles']):
        e = data[off + PAGEFILE_SIZE * i: off + PAGEFILE_SIZE * (i + 1)]
        raw = e[:512]
        try:
            name = raw.decode('utf-16-le')
        except UnicodeDecodeError as ex:
            raise Malformed(f"page file {i}: name is not UTF-16 ({ex.reason} at unit {ex.start // 2})")
        name = name.split('\0', 1)[0]
        if not name or any(ord(c) < 0x20 for c in name):
            raise Malformed(f"page file {i}: name {name!r} is empty or has control characters")
        num, guid, f = struct.unpack_from('<I16sI', e, 512)
        pfs.append((name, num, guid.hex(), f))
    # the next table, if any, starts where this one ends
    later = [v for v in (h['FourCCListOffset'], h['ParameterBlockHeadersOffset'], h['ThumbnailsOffset'])
             if v > off]
    if later and min(later) < end:
        raise Malformed(f"page-file table ({h['NumPageFiles']} x {PAGEFILE_SIZE}) overlaps the next table at {min(later)}")
    strings = walk_fourcc(data, h['FourCCListOffset'], h['FourCCListSize'])
    return h, levels, pfs, strings


def walk_fourcc(data, off, size, depth=0, out=None):
    """Every (path, value) String node, recursing into Node values. Offsets in the file
    are what the game aligns to (FourCCNode::Advance aligns the pointer to 4)."""
    if out is None:
        out = []
    end = off + size
    if end > len(data):
        raise Malformed("FourCC list runs past the end of the file")
    cc = off
    while cc + 8 <= end:
        tag, fmt, ext, length = struct.unpack_from('<IBBH', data, cc)
        val = cc + 8
        if ext == 1:
            length |= struct.unpack_from('<I', data, val)[0] << 16
            val += 4
        if val + length > end:
            raise Malformed(f"FourCC {tag_str(tag)} value runs past its list")
        if fmt == FMT_NODE:
            walk_fourcc(data, val, length, depth + 1, out)
        elif fmt == FMT_STRING:
            raw = data[val:val + length]
            if length % 2:
                raise Malformed(f"FourCC {tag_str(tag)}: String of odd byte length {length}")
            try:
                s = raw.decode('utf-16-le')
            except UnicodeDecodeError as ex:
                raise Malformed(f"FourCC {tag_str(tag)}: String is not UTF-16 ({ex.reason})")
            if not s.endswith('\0'):
                raise Malformed(f"FourCC {tag_str(tag)}: String is not NUL-terminated")
            s = s[:-1]
            if any(ord(c) < 0x20 and c not in '\r\n\t' for c in s):
                raise Malformed(f"FourCC {tag_str(tag)}: String {s[:40]!r}... has control characters (4-byte wchar_t?)")
            out.append(('  ' * depth + tag_str(tag), s))
        cc = val + length
        if cc % 4:
            cc += 4 - cc % 4
    return out


def info(path):
    data = open(path, 'rb').read()
    h, levels, pfs, strings = parse(data)
    print(f"{path}: {len(data)} bytes, GUID {h['GUID'].hex()}")
    for k in ('NumLayers', 'NumLevels', 'TileWidth', 'TileHeight', 'TileBorder', 'PageSize',
              'NumFlatTileInfos', 'NumPackedTileIDs', 'ParameterBlockHeadersCount',
              'NumPageFiles', 'PageFileMetadataOffset', 'FourCCListOffset', 'FourCCListSize'):
        print(f"  {k:28} {h[k]}")
    print("  levels (tiles):", ' '.join(f"{w}x{hh}" for w, hh, _ in levels))
    print(f"  page files ({len(pfs)}, {PAGEFILE_SIZE}-byte entries):")
    for name, num, guid, f in pfs:
        print(f"    {name}  pages={num} f={f}")
    print(f"  FourCC strings ({len(strings)}):")
    for k, v in strings:
        print(f"    {k} = {v!r}")


def selftest():
    import lspk  # noqa: the pak reader beside this script
    if not os.path.exists(CAL_PAK):
        sys.exit(f"selftest: no installed game at {CAL_PAK}")
    ok = True

    def check(label, want_ok, fn):
        nonlocal ok
        try:
            fn()
            got = True
            why = "parsed"
        except Malformed as ex:
            got = False
            why = str(ex)
        mark = "PASS" if got == want_ok else "FAIL"
        ok &= got == want_ok
        print(f"{mark} {label}: {'accepted' if got else 'refused'} ({why})")

    data = subprocess.run([sys.executable, os.path.join(HERE, 'lspk.py'), 'extract', CAL_PAK, CAL_GTS],
                          capture_output=True, check=True).stdout
    h, levels, pfs, strings = parse(data)
    print(f"calibration: {CAL_GTS} {len(data)} bytes, {h['NumPageFiles']} page files, "
          f"{len(strings)} strings, levels {' '.join(f'{w}x{hh}' for w, hh, _ in levels)}")
    exact = h['PageFileMetadataOffset'] + PAGEFILE_SIZE * h['NumPageFiles']
    nxt = min(v for v in (h['FourCCListOffset'], h['ParameterBlockHeadersOffset'], h['ThumbnailsOffset'])
              if v > h['PageFileMetadataOffset'])
    print(f"  page-file table ends at {exact}, next table at {nxt}: {'exact' if exact == nxt else 'NOT exact'}")
    ok &= exact == nxt
    ok &= all(n.endswith('.gtp') for n, *_ in pfs)
    print(f"  every page-file name ends in .gtp: {all(n.endswith('.gtp') for n, *_ in pfs)}; first {pfs[0][0]!r}")
    ok &= any(k.strip() == 'NAME' for k, _ in strings)
    print(f"  FourCC NAME strings: {[v for k, v in strings if k.strip() == 'NAME'][:3]}")
    check("untouched game tile set", True, lambda: parse(data))

    # plant 1: the page-file table as a 4-byte-wchar_t build writes it
    off = h['PageFileMetadataOffset']
    wide = bytearray(data)
    tbl = bytearray()
    for name, num, guid, f in pfs:
        tbl += name.encode('utf-32-le').ljust(1024, b'\0') + struct.pack('<I16sI', num, bytes.fromhex(guid), f)
    wide[off:off + PAGEFILE_SIZE * len(pfs)] = tbl[:PAGEFILE_SIZE * len(pfs)]   # same footprint, wrong stride
    check("page-file table written with 4-byte wchar_t (1048-byte entries)", False, lambda: parse(bytes(wide)))

    # plant 2: one FourCC String value with 4-byte characters (same byte length, half the text)
    fc = bytearray(data)
    cc = h['FourCCListOffset']
    end = cc + h['FourCCListSize']
    planted = False
    stack = [(cc, end)]
    while stack and not planted:
        cc, end = stack.pop()
        while cc + 8 <= end:
            tag, fmt, ext, length = struct.unpack_from('<IBBH', data, cc)
            val = cc + 8
            if ext == 1:
                length |= struct.unpack_from('<I', data, val)[0] << 16
                val += 4
            if fmt == FMT_NODE:
                stack.append((val, val + length))
            elif fmt == FMT_STRING and length >= 8:
                s = data[val:val + length].decode('utf-16-le')[:-1]
                w = (s.encode('utf-32-le') + b'\0\0\0\0')[:length]
                fc[val:val + length] = w
                planted = True
                break
            cc = val + length
            if cc % 4:
                cc += 4 - cc % 4
    assert planted
    check("a FourCC String written with 4-byte wchar_t", False, lambda: parse(bytes(fc)))
    print("selftest:", "ok" if ok else "FAILED")
    return 0 if ok else 1


def main():
    if len(sys.argv) >= 2 and sys.argv[1] == '--selftest':
        sys.exit(selftest())
    if len(sys.argv) == 3 and sys.argv[1] == 'info':
        try:
            info(sys.argv[2])
        except Malformed as ex:
            sys.exit(f"{sys.argv[2]}: malformed: {ex}")
        return
    sys.exit(__doc__)


if __name__ == '__main__':
    main()
