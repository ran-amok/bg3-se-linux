#!/usr/bin/env python3
"""Larian package (.pak, LSPK versions 15, 16 and 18) reader and a minimal (v18) writer, for
putting a Script Extender test mod where the game's mod manager looks (Phase 5 exit test) and
for judging mods fetched from Nexus, many of which were packed by older LSLib (v15/16).

  lspk.py list PAK                 the file table
  lspk.py pack OUT.pak DIR         DIR's files, stored uncompressed, relative names
  lspk.py extract PAK NAME         one member's bytes to stdout (stored, zlib, lz4 or zstd)
  lspk.py --selftest [GAME_PAK [PAK...]]
                                   calibration: a game pak lists sanely, and a packed
                                   directory reads back byte for byte (default pak:
                                   the installed game's DiceSet01.pak); each further PAK
                                   (e.g. a v15 mod) lists sanely and every member decodes
                                   to the size its entry claims

Format (as LSLib's PackageReader/Writer describe version 18): a 40-byte header
{LSPK, u32 version, u64 file-list offset, u32 file-list size, u8 flags, u8 priority,
md5[16], u16 parts}; the file list is {u32 count, u32 compressed size} and an LZ4
block of count * 272-byte entries {name[256], u32 offset lo, u16 offset hi, u8 part,
u8 flags (low nibble: 0 none, 1 zlib, 2 lz4, 3 zstd), u32 size on disk, u32 size}.
Versions 15 and 16 (LSLib LSPKHeader15/16, FileEntry15): the same list, a 38-byte header
without the parts count (15) or with it (16), and 296-byte entries {name[256], u64 offset,
u64 size on disk, u64 size, u32 part, u32 flags, u32 crc, u32 unknown}. Header flag 4 is a
solid package, one compressed stream for all members; its table reads, its members do not.
No dependency on an lz4 module: the block codec is here, a decoder for reading, and for
writing a block of literals only, which is valid LZ4 (the game's paks compress the
list; a mod's thirty entries need not).
"""
import glob
import hashlib
import os
import struct
import sys
import tempfile
import zlib

try:                                    # 3.14+; the game compresses most pak members with it
    from compression import zstd
except ImportError:
    zstd = None

HEADER = struct.Struct('<4sIQIBB16sH')          # 40 bytes
ENTRY = struct.Struct('<256sIHBBII')            # 272 bytes
HEADER15 = struct.Struct('<4sIQIBB16s')         # 38 bytes; 16 adds u16 parts = HEADER
ENTRY15 = struct.Struct('<256sQQQIIII')         # 296 bytes, versions 15 and 16
VERSION = 18
READABLE = (15, 16, 18)
SOLID = 4


def lz4_decode(src, out_size):
    out = bytearray()
    i = 0
    while i < len(src):
        token = src[i]; i += 1
        lit = token >> 4
        if lit == 15:
            while True:
                b = src[i]; i += 1
                lit += b
                if b != 255:
                    break
        out += src[i:i + lit]; i += lit
        if i >= len(src):
            break
        off = src[i] | src[i + 1] << 8; i += 2
        if off == 0:
            raise ValueError('lz4: zero offset')
        mlen = (token & 15) + 4
        if (token & 15) == 15:
            while True:
                b = src[i]; i += 1
                mlen += b
                if b != 255:
                    break
        start = len(out) - off
        if start < 0:
            raise ValueError('lz4: offset before start')
        for k in range(mlen):
            out.append(out[start + k])
    if len(out) != out_size:
        raise ValueError('lz4: decoded %d bytes, expected %d' % (len(out), out_size))
    return bytes(out)


def lz4_encode_literals(data):
    n = len(data)
    if n == 0:
        return b'\x00'
    head = bytes([min(n, 15) << 4])
    if n >= 15:
        rest = n - 15
        while rest >= 255:
            head += b'\xff'; rest -= 255
        head += bytes([rest])
    return head + data


def read_table(f):
    f.seek(0)
    magic, version = struct.unpack('<4sI', f.read(8))
    if magic != b'LSPK':
        raise ValueError('not an LSPK file')
    if version not in READABLE:
        raise ValueError('LSPK version %d, only %s are handled' % (version, READABLE))
    f.seek(0)
    if version == 15:
        magic, version, list_off, list_size, flags, priority, md5 = HEADER15.unpack(f.read(HEADER15.size))
        parts = 1
    else:
        magic, version, list_off, list_size, flags, priority, md5, parts = HEADER.unpack(f.read(HEADER.size))
    ent = ENTRY if version == 18 else ENTRY15
    f.seek(list_off)
    count, csize = struct.unpack('<II', f.read(8))
    raw = lz4_decode(f.read(csize), count * ent.size)
    entries = []
    for k in range(count):
        if version == 18:
            name, off1, off2, part, eflags, size_on_disk, size = ent.unpack_from(raw, k * ent.size)
            offset = off1 | off2 << 32
        else:
            name, offset, size_on_disk, size, part, eflags, _crc, _ = ent.unpack_from(raw, k * ent.size)
        entries.append(dict(name=name.split(b'\0')[0].decode(), offset=offset, part=part,
                            method=eflags & 15, level=eflags >> 4, size_on_disk=size_on_disk, size=size))
    return dict(version=version, flags=flags, priority=priority, md5=md5, parts=parts, list_off=list_off), entries


def pack(out, directory):
    files = []
    for root, dirs, names in os.walk(directory):
        dirs.sort()
        for n in sorted(names):
            p = os.path.join(root, n)
            files.append((os.path.relpath(p, directory).replace(os.sep, '/'), p))
    with open(out + '.tmp', 'wb') as f:
        f.write(b'\0' * HEADER.size)
        entries = b''
        for rel, p in files:
            data = open(p, 'rb').read()
            off = f.tell()
            f.write(data)
            name = rel.encode()
            if len(name) > 255:
                raise ValueError('name too long: ' + rel)
            entries += ENTRY.pack(name.ljust(256, b'\0'), off & 0xffffffff, off >> 32, 0, 0, len(data), len(data))
        list_off = f.tell()
        block = lz4_encode_literals(entries)
        f.write(struct.pack('<II', len(files), len(block)))
        f.write(block)
        f.seek(0)
        f.write(HEADER.pack(b'LSPK', VERSION, list_off, 8 + len(block), 0, 0, b'\0' * 16, 1))
    os.replace(out + '.tmp', out)
    return len(files)


def part_file(f, part):
    """The file that holds an entry of a multi-part pak: part 0 is the pak itself,
    part N is <stem>_N.pak beside it (the game's VirtualTextures.pak has 20). Reading the
    offset off part 0 instead returns whatever bytes sit there (tens of MB of zeros for a
    .gts), so a missing part is an error, never a silent read."""
    if part == 0:
        return f
    stem, ext = os.path.splitext(f.name)
    path = '%s_%d%s' % (stem, part, ext)
    if not os.path.exists(path):
        raise ValueError('%s: part %d of the pak is missing (%s)' % (f.name, part, path))
    return open(path, 'rb')


def listed_size(e):
    """The member's real length in bytes. LSLib writes UncompressedSize = 0 for a *stored*
    entry (every entry of a mod pak packed uncompressed),
    so `size` alone reads every such member as empty; `size_on_disk` is the length there.
    extract() reads the right bytes either way -- only the entry's own report differs."""
    return e['size'] if e['method'] else e['size_on_disk']


def empty_members(entries):
    """The members a pak declares as zero bytes long. The Linux game reads one of these as
    *the rest of the package file*: `ls::FileManager::DoMapViewOfFileToMemory` (bin/bg3
    0x2707290 on a187af94) treats a length of 0 as "to the end of the file" and records
    `stat(pak).st_size - offset` as the member's size, where the Windows build (bg3.exe
    0x144191840) and the macOS build (0x107443920) both record the 0.
    So a mod that ships an empty file inside its pak breaks on Linux only, and nothing in the
    extender can see the real length at runtime -- it has to be caught here."""
    return [e['name'] for e in entries if listed_size(e) == 0]


def extract(f, entries, name):
    for e in entries:
        if e['name'] == name:
            src = part_file(f, e['part'])
            src.seek(e['offset'])
            raw = src.read(e['size_on_disk'])
            if src is not f:
                src.close()
            if e['method'] == 0:
                return raw
            if e['method'] == 1:                    # zlib, older LSLib
                out = zlib.decompress(raw)
                if len(out) != e['size']:
                    raise ValueError('%s: zlib gave %d bytes, the entry says %d' % (name, len(out), e['size']))
                return out
            if e['method'] == 2:                    # lz4 block, size known from the entry
                return lz4_decode(raw, e['size'])
            if e['method'] == 3:                    # zstd frame: the game's own story/goal files
                if zstd is None:
                    raise ValueError('%s is zstd; needs python 3.14+ (compression.zstd)' % name)
                out = zstd.decompress(raw)
                if len(out) != e['size']:
                    raise ValueError('%s: zstd gave %d bytes, the entry says %d' % (name, len(out), e['size']))
                return out
            raise ValueError('%s: unknown compression method %d' % (name, e['method']))
    raise KeyError(name)


def check_pak(pak):
    """An arbitrary pak lists sanely and every member decodes to the size its entry claims."""
    with open(pak, 'rb') as f:
        hdr, entries = read_table(f)
        size = os.fstat(f.fileno()).st_size
        sane = hdr['list_off'] < size and all(0 < e['offset'] and e['offset'] + e['size_on_disk'] <= hdr['list_off']
                                               for e in entries)
        methods = sorted({e['method'] for e in entries})
        bad = []
        if hdr['flags'] & SOLID:
            print('pak %s: v%d, solid, %d entries, offsets within file: %s; members not decoded' % (
                os.path.basename(pak), hdr['version'], len(entries), sane))
            return sane and len(entries) > 0
        for e in entries:
            want = listed_size(e)
            try:
                if len(extract(f, entries, e['name'])) != want:
                    bad.append(e['name'])
            except ValueError as x:
                bad.append('%s (%s)' % (e['name'], x))
    empty = empty_members(entries)
    print('pak %s: v%d, %d entries, methods %s, offsets within file: %s, members decoding short: %d%s'
          ', zero-length members: %d%s' % (
        os.path.basename(pak), hdr['version'], len(entries), methods, sane, len(bad),
        (' ' + bad[0]) if bad else '', len(empty), (' ' + empty[0]) if empty else ''))
    return sane and not bad and len(entries) > 0


def selftest(game_pak, extra=()):
    ok = True
    with open(game_pak, 'rb') as f:
        hdr, entries = read_table(f)
        size = os.fstat(f.fileno()).st_size
    top = {e['name'].split('/')[0] for e in entries}
    sane = all(0 < e['offset'] + e['size_on_disk'] <= hdr['list_off'] for e in entries) and hdr['list_off'] < size
    print('game pak %s: version %d, %d entries, top-level %s, offsets within file: %s' % (
        os.path.basename(game_pak), hdr['version'], len(entries), sorted(top), sane))
    ok &= sane and len(entries) > 0 and all('/' in e['name'] for e in entries)
    # round trip: a directory with a small, a 15-byte (the literal-length boundary), a large and a nested file
    with tempfile.TemporaryDirectory() as d:
        want = {'Mods/T/meta.lsx': b'<save/>\n', 'Mods/T/a.bin': bytes(range(15)),
                'Mods/T/ScriptExtender/Lua/BootstrapServer.lua': b'Ext.Utils.Print("x")\n' * 3000, 'Mods/T/empty': b''}
        src = os.path.join(d, 'src')
        for rel, body in want.items():
            p = os.path.join(src, rel)
            os.makedirs(os.path.dirname(p), exist_ok=True)
            open(p, 'wb').write(body)
        out = os.path.join(d, 't.pak')
        n = pack(out, src)
        with open(out, 'rb') as f:
            hdr, entries = read_table(f)
            got = {e['name']: extract(f, entries, e['name']) for e in entries}
        same = got == want
        print('round trip: %d files packed, %d listed, contents identical: %s' % (n, len(entries), same))
        ok &= same and n == len(want)
        # empty_members(), two-sided on that same table: the one zero-byte member must be
        # named and the three non-empty ones must not. This is the only place the
        # stored-entry shape is catchable -- at runtime the Linux game hands the
        # extender `stat(pak).st_size - offset` for it.
        flagged = empty_members(entries)
        both = flagged == ['Mods/T/empty']
        print('zero-length members of the round-trip pak: %s (the other %d not flagged): %s' % (
            flagged, len(entries) - len(flagged), both))
        ok &= both
        # LSLib's stored-entry convention, planted on that same table: it writes
        # UncompressedSize = 0 whenever a member is stored, so a reader that reports `size`
        # calls every member of an uncompressed mod pak empty. listed_size() must report
        # size_on_disk there, and must still report
        # `size` for a compressed entry -- both sides, on entries read out of a real pak.
        with open(out, 'rb') as f:
            _, ents = read_table(f)
            for e in ents:
                e['size'] = 0                       # the plant
            stored = all(listed_size(e) == len(want[e['name']]) for e in ents)
            fake = dict(ents[0], method=2, size=1234, size_on_disk=7)
            compressed = listed_size(fake) == 1234
            print('LSLib stored entries (size=0 planted): %d members sized from size_on_disk: %s; '
                  'a compressed entry still sized from size: %s' % (len(ents), stored, compressed))
            ok &= stored and compressed
        # the block decoder against a block with a match, hand-assembled: "abcabcabcabc" =
        # literals "abc" then a match of length 9 at offset 3
        blk = bytes([0x30 | 5]) + b'abc' + bytes([3, 0])
        dec = lz4_decode(blk, 12)
        print('lz4 decoder on a hand-made match block: %r' % dec)
        ok &= dec == b'abcabcabcabc'
    # compressed members off the real pak: one per method present, decoded to the length the
    # entry claims. The entry's own size is the only independent check available here -- a
    # decoder that silently truncates is the failure this catches.
    with open(game_pak, 'rb') as f:
        hdr, entries = read_table(f)
        for method in (2, 3):
            member = next((e for e in entries if e['method'] == method and e['size'] > 0), None)
            if member is None:
                print('compressed member m%d: none in this pak, not exercised' % method)
                continue
            if method == 3 and zstd is None:
                print('compressed member m3: no compression.zstd on this python, not exercised')
                continue
            body = extract(f, entries, member['name'])
            good = len(body) == member['size']
            print('compressed member m%d: %s -> %d bytes, entry says %d: %s' % (
                method, member['name'].split('/')[-1], len(body), member['size'], good))
            ok &= good
        if not any(e['method'] == 3 for e in entries):
            lsv = sorted(glob.glob(os.path.expanduser(
                '~/.local/share/Larian Studios/Baldur\'s Gate 3/PlayerProfiles/*/Savegames/Story/*/*.lsv')))
            if not lsv:
                print('compressed member m3: no pak member and no savegame found, NOT exercised')
            else:
                with open(lsv[-1], 'rb') as g:
                    _, sav = read_table(g)
                    m3 = next((e for e in sav if e['method'] == 3 and e['size'] > 0), None)
                    if m3 is None:
                        print('compressed member m3: savegame has none, NOT exercised')
                    else:
                        body = extract(g, sav, m3['name'])
                        good = len(body) == m3['size']
                        print('compressed member m3 (savegame %s): %s -> %d bytes, entry says %d: %s' % (
                            os.path.basename(os.path.dirname(lsv[-1]))[:24], m3['name'], len(body), m3['size'], good))
                        ok &= good
    for pak in extra:
        ok &= check_pak(pak)
    print('selftest:', 'PASS' if ok else 'FAIL')
    return 0 if ok else 1


def main():
    a = sys.argv[1:]
    if not a:
        print(__doc__); return 2
    if a[0] == '--selftest':
        pak = a[1] if len(a) > 1 else os.path.expanduser('~/.local/share/Steam/steamapps/common/Baldurs Gate 3/Data/DiceSet01.pak')
        return selftest(pak, a[2:])
    if a[0] == 'list':
        with open(a[1], 'rb') as f:
            hdr, entries = read_table(f)
        print('version %d flags %d priority %d parts %d md5 %s' % (hdr['version'], hdr['flags'], hdr['priority'], hdr['parts'], hdr['md5'].hex()))
        for e in entries:
            print('%10d %10d  m%d  %s%s%s' % (e['offset'], listed_size(e), e['method'], e['name'],
                                              '  part %d' % e['part'] if e['part'] else '',
                                              '  ZERO-LENGTH: the Linux game reads this as the rest'
                                              ' of the pak' if listed_size(e) == 0 else ''))
        return 0
    if a[0] == 'pack':
        n = pack(a[1], a[2])
        print('%s: %d files' % (a[1], n))
        return 0
    if a[0] == 'extract':
        with open(a[1], 'rb') as f:
            hdr, entries = read_table(f)
            sys.stdout.buffer.write(extract(f, entries, a[2]))
        return 0
    print(__doc__); return 2


if __name__ == '__main__':
    sys.exit(main())
