#!/usr/bin/env python3
"""Read a Steam depot manifest (the binary DepotDownloader keeps) as a per-file SHA1 listing.

  depot-manifest.py list MANIFEST [-o OUT]     the listing, in DepotDownloader's
                                               -manifest-only column layout
  depot-manifest.py check MANIFEST ROOT        SHA1 every listed file present under ROOT;
                                               exit 1 on any mismatch, 2 if none present
  depot-manifest.py --selftest                 calibration, below

Why: a fetch is judged by content, and fetch-oracles.sh pins each file's SHA1. DepotDownloader
writes the depot's own manifest beside every fetch
(`<dir>/.DepotDownloader/<depot>_<gid>.manifest`), so on a new build the SHA1s are here
without a separate `-manifest-only` login run. The file is Steam's native format: a
ContentManifestPayload protobuf behind magic 0x71F617D0, then ContentManifestMetadata behind 0x1F4812BE. Names are
in clear (not encrypted) in every manifest seen so far; an encrypted one is refused.

--selftest: the two build-24532579 manifests, kept as `steam-manifest.bin` beside the
`steam-manifest.txt` a -manifest-only run prints in `$BG3_SPECIMENS/<depot fetch>/` (the
directory names in CALIBRATION; skipped when unset), must decode to exactly those listings
(sha, flags, name, size: 228 macOS entries, 17 Windows), and a planted flip in one SHA1 must
make the comparison fail.
"""
import argparse, hashlib, os, struct, sys

PAYLOAD_MAGIC = 0x71F617D0
METADATA_MAGIC = 0x1F4812BE
ARCHIVE = os.environ.get('BG3_SPECIMENS', '')
CALIBRATION = ['macos-depot1419660-m7960684881345890822',
               'windows-depot1419652-m7005275183208636774']


def varint(b, i):
    v = s = 0
    while True:
        c = b[i]; i += 1
        v |= (c & 0x7f) << s; s += 7
        if c < 0x80:
            return v, i


def fields(b):
    """(field number, wire type, value) over one protobuf message."""
    i = 0
    while i < len(b):
        key, i = varint(b, i)
        f, wt = key >> 3, key & 7
        if wt == 0:
            v, i = varint(b, i)
        elif wt == 2:
            n, i = varint(b, i); v = b[i:i + n]; i += n
        elif wt == 1:
            v = b[i:i + 8]; i += 8
        elif wt == 5:
            v = b[i:i + 4]; i += 4
        else:
            raise ValueError(f'wire type {wt}')
        yield f, wt, v


def section(b, off, magic):
    m, n = struct.unpack_from('<II', b, off)
    if m != magic:
        raise ValueError(f'magic {m:#x} at {off:#x}, want {magic:#x}')
    return b[off + 8:off + 8 + n], off + 8 + n


def decode(path):
    b = open(path, 'rb').read()
    payload, off = section(b, 0, PAYLOAD_MAGIC)
    meta_b, _ = section(b, off, METADATA_MAGIC)
    meta = {f: v for f, wt, v in fields(meta_b) if wt == 0}
    if meta.get(4):
        raise ValueError('filenames are encrypted')
    files = []
    for f, wt, v in fields(payload):
        if f != 1:
            continue
        e = {'name': '', 'size': 0, 'flags': 0, 'sha': b'', 'chunks': 0}
        for g, _, w in fields(v):
            if g == 1: e['name'] = w.decode('utf-8').replace('\\', '/')
            elif g == 2: e['size'] = w
            elif g == 3: e['flags'] = w
            elif g == 5: e['sha'] = w
            elif g == 6: e['chunks'] += 1
        files.append(e)
    files.sort(key=lambda e: e['name'].lower())
    return {'depot': meta.get(1), 'gid': meta.get(2), 'created': meta.get(3), 'files': files}


def sha_hex(e):
    return e['sha'].hex() or '0' * 40


def listing(m):
    out = [f"Content Manifest for Depot {m['depot']} (decoded by tools/depot-manifest.py)", '',
           f"Manifest ID / created  : {m['gid']} / {m['created']}",
           f"Total number of files  : {len(m['files'])}", '',
           '          Size Chunks File SHA                                 Flags Name']
    for e in m['files']:
        out.append(f"{e['size']:>14} {e['chunks']:>6} {sha_hex(e)} {e['flags']:>5x} {e['name']}")
    return '\n'.join(out) + '\n'


def rows(text):
    """(sha, flags, name, size) from a listing, ours or DepotDownloader's."""
    r = set()
    for line in text.splitlines():
        p = line.split(None, 4)
        if len(p) == 5 and len(p[2]) == 40 and all(c in '0123456789abcdef' for c in p[2]):
            r.add((p[2], p[3], p[4], p[0]))
    return r


def check(m, root):
    seen = bad = 0
    for e in m['files']:
        p = os.path.join(root, e['name'])
        if e['flags'] & 0x240 or not os.path.isfile(p) or os.path.islink(p):
            continue
        seen += 1
        h = hashlib.sha1()
        with open(p, 'rb') as fh:
            for blk in iter(lambda: fh.read(1 << 20), b''):
                h.update(blk)
        ok = h.hexdigest() == sha_hex(e)
        bad += not ok
        print(f"{'ok ' if ok else 'BAD'} {sha_hex(e)} {e['name']}" + ('' if ok else f' (got {h.hexdigest()})'))
    print(f"depot {m['depot']} manifest {m['gid']}: {len(m['files'])} entries, {seen} present, {bad} bad",
          file=sys.stderr)
    return 1 if bad else (2 if not seen else 0)


def selftest():
    if not ARCHIVE:
        print('skip: BG3_SPECIMENS is unset, so there are no manifests to calibrate on')
        return 2
    fails = 0
    for d in CALIBRATION:
        base = os.path.join(ARCHIVE, d)
        want = rows(open(os.path.join(base, 'steam-manifest.txt')).read())
        got = rows(listing(decode(os.path.join(base, 'steam-manifest.bin'))))
        ok = want == got and len(want) > 0
        print(f"{'ok  ' if ok else 'FAIL'} {d}: {len(got)} decoded, {len(want)} listed, "
              f"{len(want ^ got)} differ")
        fails += not ok
        planted = set(got)
        s, f, n, z = sorted(planted)[-1]
        planted.discard((s, f, n, z)); planted.add((('f' if s[0] != 'f' else 'e') + s[1:], f, n, z))
        caught = planted != want
        print(f"{'ok  ' if caught else 'FAIL'} {d}: planted SHA1 flip {'caught' if caught else 'MISSED'}")
        fails += not caught
    return 1 if fails else 0


def main():
    if '--selftest' in sys.argv[1:]:
        return selftest()
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest='cmd', required=True)
    a = sub.add_parser('list'); a.add_argument('manifest'); a.add_argument('-o')
    a = sub.add_parser('check'); a.add_argument('manifest'); a.add_argument('root')
    args = ap.parse_args()
    m = decode(args.manifest)
    if args.cmd == 'list':
        text = listing(m)
        if args.o:
            open(args.o, 'w').write(text)
        else:
            sys.stdout.write(text)
        return 0
    return check(m, args.root)


if __name__ == '__main__':
    sys.exit(main())
