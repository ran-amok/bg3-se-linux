#!/usr/bin/env python3
"""Which NoesisGUI is statically linked into `bin/bg3`, and does it differ from the SDK we build against.

The extender's `Ext.UI` compiles against upstream's pinned NoesisGUI Native SDK **3.1.7**
headers (`build/ext-external/NoesisGUI-NativeSDK-win-3.1.7-Indie`).
`bin/bg3` links Noesis statically and LTO strips its exports, so the usual oracle route
(`nm` on a shared library) is not available.  Two things stand in for it.

**`version BIN`** -- Noesis's assert macros bake `__FILE__` into `.rodata`, and Larian build
their copy out of a tree rooted at the version number, so `bin/bg3` carries literals of the
shape `/3.1.6/Src/Packages/Gui/Core/Src/UI.cpp`.  The version directory is the binary's own
statement of which Noesis it is.  More than one distinct token, or none, is an ERR and not a
guess.

**`abi LIB...`** -- whether two Noesis builds agree on the layout that matters to a caller.
NuGet's `Noesis.GUI` package (anonymous download, no login) ships an official
`runtimes/linux-x64/native/libNoesis.so` for every release; `fetch` gets them.  They are
stripped, so nothing can be named in them -- but `.data.rel.ro` still holds every vtable,
every RTTI table and every static table of pointers, and each pointer carries an
`R_X86_64_RELATIVE` relocation.  The section-relative offsets of those relocations, and the
run lengths they fall into, are the *shape* of all of it: a class gaining, losing or
reordering a virtual function moves them.  Two builds whose offset list is identical cannot
differ in vtable layout.

**`vorder BIN CLASS [HEADER]`** -- whether the SDK header declares a class's virtual functions
in the order `bin/bg3` was built with.  `abi` cannot see this: reordering two virtuals of the
same arity moves no relocation offset at all.  `bin/bg3` does have it -- `Noesis::TypeProperty`'s
two value getters are the other way round there, which made every
property read through `Lua/Libs/ClientUI` call the wrong overload and crash `DirectProperties()`.
`ext/fetch-externals.sh` swaps the two declarations; this verb is the witness for that edit, and
it is meant to be re-run on a game update.  Exit 1 on any disagreement.

**`vtables BIN [RE]`** -- slot counts for the named `_ZTVN6Noesis...` vtables `bin/bg3`'s
`.symtab` does keep (1445 of them on a187af94).  `size/8 - 2` slots, the two leading words
being offset-to-top and the typeinfo pointer; a class with interface bases spans several
address points in one `_ZTV`, so compare the whole size, not a slot index.  These are the
numbers a clang `-fdump-vtable-layouts` over the real headers has to reproduce, a comparison
the build itself can be asked for.

  noesis-version.py version BIN
  noesis-version.py abi LIB [LIB...]
  noesis-version.py vtables BIN [RE]
  noesis-version.py vorder BIN CLASS [HEADER]
  noesis-version.py fetch VERSION [VERSION...]
  noesis-version.py --selftest

`--selftest` is two-sided and needs the four oracle libraries `fetch 3.1.5 3.1.6 3.1.7 3.2.0`
puts in `dumps/oracles/noesis-nuget/` (ignored, regenerable): the fingerprint must equate the
pair that agrees and separate both adjacent pairs that do not, and `version` must read a
planted token and refuse a planted second one.
"""

import hashlib
import io
import os
import re
import subprocess
import sys
import tempfile
import urllib.request
import zipfile

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ORACLES = os.path.join(REPO, 'dumps', 'oracles', 'noesis-nuget')
NUPKG_URL = 'https://www.nuget.org/api/v2/package/Noesis.GUI/%s'
NUPKG_MEMBER = 'runtimes/linux-x64/native/libNoesis.so'

# Noesis bakes __FILE__ into its assert macros; Larian's tree is rooted at the version.
VERSION_RE = re.compile(rb'/(\d+\.\d+\.\d+)/Src/Packages/[A-Za-z0-9_/.+-]+\.(?:cpp|inl|h)\x00')


# ---------------------------------------------------------------- version


def version_tokens(path):
    """{version string: [literals]} for every /X.Y.Z/Src/Packages/... literal in the file."""
    blob = open(path, 'rb').read()
    found = {}
    for m in VERSION_RE.finditer(blob):
        found.setdefault(m.group(1).decode(), []).append(m.group(0)[:-1].decode())
    return found


def cmd_version(path):
    found = version_tokens(path)
    for v in sorted(found):
        print('%s\t%d literals\te.g. %s' % (v, len(found[v]), sorted(found[v])[0]))
    if not found:
        print('ERR\tno /X.Y.Z/Src/Packages/ literal in %s' % path, file=sys.stderr)
        return 1
    if len(found) > 1:
        print('ERR\t%d distinct version tokens: %s -- not a single Noesis'
              % (len(found), ', '.join(sorted(found))), file=sys.stderr)
        return 1
    print('noesis\t%s' % sorted(found)[0])
    return 0


# ---------------------------------------------------------------- abi fingerprint


def sections(path):
    out = subprocess.run(['readelf', '-S', '-W', path],
                         capture_output=True, text=True, check=True).stdout
    secs = {}
    for line in out.splitlines():
        p = line.replace(']', ' ').split()
        if len(p) > 6 and p[1].startswith('.') and re.fullmatch(r'[0-9a-f]+', p[3] or ''):
            secs[p[1]] = (int(p[3], 16), int(p[5], 16))
    return secs


def relro_fingerprint(path):
    """(section size, [section-relative offsets], [run lengths]) of .data.rel.ro's pointers."""
    secs = sections(path)
    if '.data.rel.ro' not in secs:
        sys.exit('%s: no .data.rel.ro' % path)
    start, size = secs['.data.rel.ro']
    out = subprocess.run(['readelf', '-r', '-W', path],
                         capture_output=True, text=True, check=True).stdout
    offs = []
    for line in out.splitlines():
        p = line.split()
        if len(p) >= 3 and p[2] == 'R_X86_64_RELATIVE':
            o = int(p[0], 16)
            if start <= o < start + size:
                offs.append(o - start)
    offs.sort()
    runs, cur = [], 1
    for i in range(1, len(offs)):
        if offs[i] - offs[i - 1] == 8:
            cur += 1
        else:
            runs.append(cur)
            cur = 1
    if offs:
        runs.append(cur)
    return size, offs, runs


def fingerprint_digest(fp):
    size, offs, runs = fp
    h = hashlib.sha256(('%x\n' % size).encode())
    h.update(b','.join(b'%x' % o for o in offs))
    return h.hexdigest()[:16]


def cmd_abi(paths):
    fps = {}
    for p in paths:
        fp = relro_fingerprint(p)
        fps[p] = fp
        print('%s\t.data.rel.ro 0x%x\tpointers %d\truns %d\t%s'
              % (os.path.basename(p), fp[0], len(fp[1]), len(fp[2]), fingerprint_digest(fp)))
    if len(paths) < 2:
        return 0
    print()
    for i in range(len(paths)):
        for j in range(i + 1, len(paths)):
            a, b = fps[paths[i]], fps[paths[j]]
            same = a[0] == b[0] and a[1] == b[1]
            print('%s vs %s\t%s' % (os.path.basename(paths[i]), os.path.basename(paths[j]),
                                    'IDENTICAL' if same else 'DIFFERENT'))
    return 0


# ---------------------------------------------------------------- vtables


def noesis_vtables(path, pattern=None):
    out = subprocess.run(['nm', '-S', path], capture_output=True, text=True).stdout
    rows = []
    rx = re.compile(pattern) if pattern else None
    for line in out.splitlines():
        p = line.split()
        if len(p) == 4 and p[3].startswith('_ZTVN6Noesis'):
            name = p[3]
            if rx and not rx.search(name):
                continue
            size = int(p[1], 16)
            rows.append((int(p[0], 16), size, size // 8 - 2, name))
    rows.sort(key=lambda r: r[3])
    return rows


def cmd_vtables(path, pattern):
    rows = noesis_vtables(path, pattern)
    if not rows:
        print('ERR\tno _ZTVN6Noesis vtable matched', file=sys.stderr)
        return 1
    for addr, size, slots, name in rows:
        dem = subprocess.run(['c++filt', name], capture_output=True, text=True).stdout.strip()
        print('%#x\t0x%x\t%d\t%s' % (addr, size, slots, dem))
    print('# %d vtables' % len(rows))
    return 0


# ---------------------------------------------------------------- fetch


def cmd_fetch(versions):
    os.makedirs(ORACLES, exist_ok=True)
    for v in versions:
        dest = os.path.join(ORACLES, 'libNoesis-%s.so' % v)
        if os.path.exists(dest):
            print('have\t%s' % dest)
            continue
        with tempfile.NamedTemporaryFile(suffix='.nupkg') as tmp:
            with urllib.request.urlopen(NUPKG_URL % v) as r:
                tmp.write(r.read())
            tmp.flush()
            with zipfile.ZipFile(tmp.name) as z:
                if NUPKG_MEMBER not in z.namelist():
                    sys.exit('%s: no %s in the package' % (v, NUPKG_MEMBER))
                data = z.read(NUPKG_MEMBER)
        if data[:4] != b'\x7fELF':
            sys.exit('%s: %s is not an ELF' % (v, NUPKG_MEMBER))
        open(dest, 'wb').write(data)
        print('fetched\t%s\t%d bytes\tsha256 %s'
              % (dest, len(data), hashlib.sha256(data).hexdigest()))
    return 0


# ---------------------------------------------------------------- vtable slot order

# `abi` compares relocation *offsets*, which cannot see two virtuals declared in a different
# order: the table is the same size and the pointers sit in the same places.  That is exactly
# the defect `bin/bg3` has against the 3.1.7 headers -- so the slot
# order of the virtuals the extender calls has to be read, not inferred from `abi`.
#
# `bin/bg3` keeps no `_ZTV` for an abstract base, but it keeps the concrete subclasses', and a
# subclass's table *is* the base's order with the overrides filled in.  So the reading is: take
# every vtable matching a pattern, require at least two of them and require them to agree slot
# for slot, and compare that against the order the header declares.  The names are compared
# after the class qualification and the template arguments are stripped, and the parameter types
# are canonicalised (`const void*` and `void const*` are one type), because the demangler and
# the header spell them differently.
VORDER = {
    # class: (vtable symbol pattern, header path under Include/, {header name: game name})
    # Larian's Noesis spells the two-argument getter `Get`; 3.1.7 spells it `GetCopy`
    # (ext/fetch-externals.sh renames TypePropertyFunction's override to match).
    'TypeProperty': (r'^_ZTVN6Noesis18TypePropertyOffsetI',
                     'NsCore/TypeProperty.h', {'GetCopy': 'Get'}),
}
SDK_INCLUDE = os.path.join(REPO, 'build', 'ext-external',
                           'NoesisGUI-NativeSDK-win-3.1.7-Indie', 'Include')


def canon_type(t):
    """`const void* ptr`, `void const*` -> `void* const`; a parameter name is dropped."""
    t = re.sub(r'\s+', ' ', t.strip()).replace('Noesis::', '')
    t = re.sub(r'\s*\*\s*', '*', t)
    if '*' in t:                       # anything after the last `*` is the parameter's name
        t = t[:t.rindex('*') + 1]
    else:                              # `BaseComponent value` -> `BaseComponent`
        words = t.split()
        if len(words) > 1 and re.fullmatch(r'[A-Za-z_]\w*', words[-1]) \
                and words[-1] not in ('void', 'int', 'char', 'bool', 'float', 'double',
                                      'short', 'long', 'unsigned', 'signed', 'const'):
            t = ' '.join(words[:-1])
    stars = t.count('*')
    base = t.replace('*', '')
    const = 'const' in base.split()
    base = ' '.join(w for w in base.split() if w != 'const')
    return '%s%s%s' % (base, ' const' if const else '', '*' * stars)


def split_args(s):
    """Top-level comma split: `Ptr<A, B>*, void*` is two arguments, not three."""
    out, depth, cur = [], 0, ''
    for c in s:
        if c in '<([':
            depth += 1
        elif c in '>)]':
            depth -= 1
        if c == ',' and depth == 0:
            out.append(cur)
            cur = ''
        else:
            cur += c
    if cur.strip():
        out.append(cur)
    return [canon_type(a) for a in out if a.strip() and a.strip() != 'void']


def split_signature(decl):
    """`...TypePropertyOffset<X>::Get(void const*) const` -> ('Get', ['void* const'])."""
    depth = 0
    for i, c in enumerate(decl):
        if c in '<[':
            depth += 1
        elif c in '>]':
            depth -= 1
        elif c == '(' and depth == 0:
            break
    else:
        return None
    head, rest = decl[:i], decl[i + 1:]
    args = rest[:rest.rindex(')')] if ')' in rest else rest
    # the unqualified name: the last `::` segment outside any template argument list
    depth, cut = 0, 0
    for j in range(len(head)):
        if head[j] in '<[':
            depth += 1
        elif head[j] in '>]':
            depth -= 1
        elif depth == 0 and head.startswith('::', j):
            cut = j + 2
    name = head[cut:].strip().split()[-1] if head[cut:].strip() else ''
    if name.startswith('~') or '~' in name:
        name = '~'                     # a destructor is named for its own class
    return name, split_args(args)


def relative_targets(path):
    """offset -> addend, for every R_X86_64_RELATIVE relocation."""
    out = subprocess.run(['readelf', '-r', '-W', path],
                         capture_output=True, text=True, check=True).stdout
    rel = {}
    for line in out.splitlines():
        p = line.split()
        if len(p) >= 4 and p[2] == 'R_X86_64_RELATIVE':
            rel[int(p[0], 16)] = int(p[3], 16)
    return rel


def address_names(path):
    """address -> the defined symbol names at it (LTO folds, so there may be several)."""
    out = subprocess.run(['nm', '--defined-only', path],
                         capture_output=True, text=True).stdout
    names = {}
    for line in out.splitlines():
        p = line.split(' ', 2)
        if len(p) == 3:
            try:
                names.setdefault(int(p[0], 16), []).append(p[2].strip())
            except ValueError:
                pass
    return names


def vtable_slots(path, pattern):
    """[(vtable symbol, [(slot, target, demangled name)])] for every matching _ZTV."""
    rows = noesis_vtables(path, pattern)
    if not rows:
        return []
    rel, names = relative_targets(path), address_names(path)
    want = sorted({t for a, s, _, _ in rows for off in range(0, s, 8)
                   for t in [rel.get(a + off)] if t is not None})
    mangled = [names.get(t, [''])[0] for t in want]
    dem = subprocess.run(['c++filt'], input='\n'.join(mangled),
                         capture_output=True, text=True).stdout.splitlines()
    byaddr = dict(zip(want, dem))
    out = []
    for addr, size, _, name in rows:
        slots = []
        for off in range(16, size, 8):     # past offset-to-top and the typeinfo pointer
            t = rel.get(addr + off)
            slots.append((off // 8 - 2, t, byaddr.get(t, '') if t is not None else ''))
        out.append((name, slots))
    return out


def header_virtuals(path, cls):
    """The virtuals `class cls` declares, in order, as (name, [args]); a dtor is two slots."""
    text = open(path, encoding='utf-8', errors='replace').read().replace('\r', '')
    m = re.search(r'\bclass\b[^;{}\n]*\b%s\b[^;{}]*\{' % re.escape(cls), text)
    if not m:
        sys.exit('%s: no `class %s` in %s' % (os.path.basename(path), cls, path))
    i, depth = m.end(), 1
    while i < len(text) and depth:
        depth += {'{': 1, '}': -1}.get(text[i], 0)
        i += 1
    body = text[m.end():i - 1]
    out = []
    for decl in re.findall(r'\bvirtual\b([^;{]*);', body):
        decl = re.sub(r'=\s*0\s*$', '', decl.strip()).strip()
        if '~' in decl:
            out += [('~', []), ('~', [])]      # Itanium: complete, then deleting
            continue
        sig = split_signature(decl)
        if sig:
            out.append(sig)
    return out


def vorder_compare(path, cls, header=None):
    """(vtable names, [(slot, verdict, header sig, binary sig)]) or an ERR string."""
    pattern, rel_header, alias = VORDER[cls]
    header = header or os.path.join(SDK_INCLUDE, rel_header)
    tables = vtable_slots(path, pattern)
    if len(tables) < 2:
        return ('ERR\t%s: %d vtable(s) match %s, need at least two that agree'
                % (os.path.basename(path), len(tables), pattern))
    read = {}
    for name, slots in tables:
        sigs = [split_signature(d) or ('?', []) for _, _, d in slots]
        read.setdefault(tuple((n, tuple(a)) for n, a in sigs), []).append(name)
    if len(read) > 1:
        return ('ERR\tthe %d matching vtables do not agree slot for slot: %s'
                % (len(tables), '; '.join('%d slots in %s' % (len(k), v[0])
                                          for k, v in read.items())))
    game = [(n, list(a)) for n, a in next(iter(read))]
    want = [(alias.get(n, n), a) for n, a in header_virtuals(header, cls)]
    rows = []
    for slot in range(max(len(game), len(want))):
        g = game[slot] if slot < len(game) else None
        w = want[slot] if slot < len(want) else None
        if w is None:
            verdict = 'extra'            # a subclass virtual past the base's -- not a mismatch
        elif g is None:
            verdict = 'MISSING'
        else:
            verdict = 'ok' if g == w else 'MISMATCH'
        rows.append((slot, verdict, w, g))
    return (sorted(n for ns in read.values() for n in ns), rows, header)


def cmd_vorder_quiet(path, cls, header=None):
    """cmd_vorder's exit code with its table and its ERR line suppressed."""
    out, err = sys.stdout, sys.stderr
    sys.stdout = sys.stderr = io.StringIO()
    try:
        return cmd_vorder(path, cls, header)
    finally:
        sys.stdout, sys.stderr = out, err


def cmd_vorder(path, cls, header=None):
    got = vorder_compare(path, cls, header)
    if isinstance(got, str):
        print(got, file=sys.stderr)
        return 1
    names, rows, header = got
    print('# %s: %d vtables agree (%s), header %s'
          % (cls, len(names), ', '.join(names[:2]),
             os.path.relpath(header, REPO) if header.startswith(REPO) else header))
    for slot, verdict, w, g in rows:
        print('%d\t%-8s\t%s\t%s' % (slot, verdict, sigstr(w), sigstr(g)))
    bad = sum(1 for _, v, _, _ in rows if v in ('MISMATCH', 'MISSING'))
    print('# %s' % ("ok: the header declares them in the binary's order"
                    if not bad else 'FAIL: %d slot(s) disagree' % bad))
    return 1 if bad else 0


def sigstr(sig):
    return '-' if sig is None else '%s(%s)' % (sig[0], ', '.join(sig[1]))


# ---------------------------------------------------------------- selftest

# Read by hand off `nm -S` on the a187af94 bin/bg3.
HAND_VTABLES = {
    '_ZTVN6Noesis13BaseComponentE': (0x78291d8, 0x40, 6),
    '_ZTVN6Noesis14BaseCollectionE': (0x782c938, 0x120, 34),
    '_ZTVN6Noesis6VisualE': (0x7867b78, 0x1a8, 51),
    '_ZTVN6Noesis9TypeClassE': (0x78292d0, 0x40, 6),
}
HAND_BIN = os.path.join(os.environ.get('BG3_SPECIMENS', '$BG3_SPECIMENS'),
                        'steam-25605617_a187af943be20f96b004dcfb74a33c43f65ba613/bin/bg3')


def selftest():
    ok = True

    def check(label, got, want):
        nonlocal ok
        good = got == want
        ok &= good
        print('%-58s %s  got %r' % (label, 'ok ' if good else 'FAIL', got))

    # -- the fingerprint, two-sided over four official builds --------------
    libs = {v: os.path.join(ORACLES, 'libNoesis-%s.so' % v)
            for v in ('3.1.5', '3.1.6', '3.1.7', '3.2.0')}
    missing = [v for v, p in libs.items() if not os.path.exists(p)]
    if missing:
        print('ERR\toracles missing: %s -- run `noesis-version.py fetch %s`'
              % (', '.join(missing), ' '.join(sorted(libs))), file=sys.stderr)
        return False
    fp = {v: relro_fingerprint(p) for v, p in libs.items()}
    for v in sorted(fp):
        lit = version_literal(libs[v])
        check('%s reports itself as %s' % (os.path.basename(libs[v]), v), lit, v)
    # The pair that must agree: the SDK we compile against and the game's version.
    check('3.1.6 vs 3.1.7 rel-ro identical', fp['3.1.6'][1] == fp['3.1.7'][1], True)
    # The two pairs that must not: an adjacent patch and the next minor.
    check('3.1.5 vs 3.1.6 rel-ro differs', fp['3.1.5'][1] != fp['3.1.6'][1], True)
    check('3.1.7 vs 3.2.0 rel-ro differs', fp['3.1.7'][1] != fp['3.2.0'][1], True)
    check('3.1.6/3.1.7 same pointer count', len(fp['3.1.6'][1]) == len(fp['3.1.7'][1]), True)

    # -- version token reading, planted both ways --------------------------
    blob = bytearray(b'noise' * 40)
    blob += b'/3.1.6/Src/Packages/Gui/Core/Src/UI.cpp\x00'
    blob += b'more noise'
    with tempfile.TemporaryDirectory(prefix='noesis-version-selftest-') as d:
        one = os.path.join(d, 'one')
        open(one, 'wb').write(blob)
        check('planted single token is read', sorted(version_tokens(one)), ['3.1.6'])
        two = os.path.join(d, 'two')
        open(two, 'wb').write(blob + b'/3.2.0/Src/Packages/Gui/Core/Src/UI.cpp\x00')
        check('planted second token is not hidden', sorted(version_tokens(two)),
              ['3.1.6', '3.2.0'])
        check('planted second token is an ERR', cmd_version(two), 1)
        none = os.path.join(d, 'none')
        open(none, 'wb').write(b'no version here at all\x00')
        check('no token is an ERR', cmd_version(none), 1)

    # -- the vtable reader against four hand-read rows ---------------------
    if os.path.exists(HAND_BIN):
        rows = {n: (a, s, sl) for a, s, sl, n in noesis_vtables(HAND_BIN)}
        for name, want in sorted(HAND_VTABLES.items()):
            check('hand-read %s' % name, rows.get(name), want)
        check('bin/bg3 Noesis vtable count > 1000', len(rows) > 1000, True)
        check('bin/bg3 version token', sorted(version_tokens(HAND_BIN)), ['3.1.6'])

        # -- vorder, two-sided: the fetched header must agree with the binary, and the
        #    header as the zip ships it (the two getters the other way round) must not.
        #    The unmodified-input run is the one above it: a `vorder` that passed only
        #    because the comparison is blind would pass the planted ones too.
        pattern, rel_header, _ = VORDER['TypeProperty']
        real = os.path.join(SDK_INCLUDE, rel_header)
        if not os.path.exists(real):
            print('%-58s skip (no %s -- run ext/fetch-externals.sh)'
                  % ('vorder TypeProperty', real))
        else:
            got = vorder_compare(HAND_BIN, 'TypeProperty')
            check('vorder: the fetched header is in the binary order',
                  [] if isinstance(got, str) else
                  [(sl, v) for sl, v, _, _ in got[1] if v != 'ok'], [])
            check('vorder: 9 slots read, >= 2 vtables agreeing',
                  None if isinstance(got, str) else (len(got[0]) >= 2, len(got[1])),
                  (True, 9))
            # newline='' keeps the zip's CRLFs, which the planted edits match on
            text = open(real, encoding='utf-8', newline='').read()
            with tempfile.TemporaryDirectory(prefix='noesis-vorder-selftest-') as d:
                # plant 1: the zip's own order back again -- the defect this check exists for.
                g1 = '    virtual const void* Get(const void* ptr) const;'
                g2 = '    virtual void GetCopy(const void* ptr, void* dest) const;'
                check('plant: both getter declarations are where expected',
                      (text.count(g1 + '\r\n'), text.count(g2 + '\r\n')), (1, 1))
                swapped = os.path.join(d, 'swapped.h')
                open(swapped, 'w', newline='').write(
                    text.replace(g1 + '\r\n', '\x00MARK\r\n')
                        .replace(g2 + '\r\n', g1 + '\r\n')
                        .replace('\x00MARK\r\n', g2 + '\r\n'))
                got = vorder_compare(HAND_BIN, 'TypeProperty', swapped)
                check('plant: the swapped header mismatches at slots 6 and 7',
                      [(sl, v) for sl, v, _, _ in got[1] if v != 'ok'],
                      [(6, 'MISMATCH'), (7, 'MISMATCH')])
                check('plant: the swapped header is an exit-1',
                      cmd_vorder_quiet(HAND_BIN, 'TypeProperty', swapped), 1)
                # plant 2: a virtual deleted -- every slot after it shifts.
                dropped = os.path.join(d, 'dropped.h')
                open(dropped, 'w', newline='').write(
                    text.replace('    virtual bool IsReadOnly() const;\r\n', '', 1))
                got = vorder_compare(HAND_BIN, 'TypeProperty', dropped)
                check('plant: a dropped virtual shifts slots 3 and up',
                      [sl for sl, v, _, _ in got[1] if v != 'ok'], [3, 4, 5, 6, 7, 8])
    else:
        print('%-58s skip (no %s)' % ('hand-read vtables', HAND_BIN))

    print('selftest: %s' % ('ok' if ok else 'FAIL'))
    return ok


def version_literal(path):
    """The bare "X.Y.Z" string GetBuildVersion() returns, as it sits in .rodata."""
    out = subprocess.run(['strings', '-a', path], capture_output=True, text=True).stdout
    lits = sorted({l for l in out.splitlines() if re.fullmatch(r'\d+\.\d+\.\d+', l)})
    return lits[0] if len(lits) == 1 else lits


def main(argv):
    if not argv or argv[0] in ('-h', '--help'):
        print(__doc__)
        return 0
    if argv[0] == '--selftest':
        return 0 if selftest() else 1
    if argv[0] == 'version' and len(argv) == 2:
        return cmd_version(argv[1])
    if argv[0] == 'abi' and len(argv) >= 2:
        return cmd_abi(argv[1:])
    if argv[0] == 'vtables' and len(argv) in (2, 3):
        return cmd_vtables(argv[1], argv[2] if len(argv) == 3 else None)
    if argv[0] == 'vorder' and len(argv) in (3, 4):
        if argv[2] not in VORDER:
            sys.exit('noesis-version.py: no vorder recipe for %r (have %s)'
                     % (argv[2], ', '.join(sorted(VORDER))))
        return cmd_vorder(argv[1], argv[2], argv[3] if len(argv) == 4 else None)
    if argv[0] == 'fetch' and len(argv) >= 2:
        return cmd_fetch(argv[1:])
    sys.exit('noesis-version.py: unknown command %r '
             '(version, abi, vtables, vorder, fetch, --selftest)' % ' '.join(argv))


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
