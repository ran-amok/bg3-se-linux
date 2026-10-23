#!/usr/bin/env python3
"""The ECS TypeId table: names for the game's type indices, per build.

  typeids.py emit BUILD_ID [--harvest DIR]   typeids/<build-id>.tsv from dumps/harvest/<build-id>/
  typeids.py verify [TABLE] BIN              the census: every row re-established from BIN
                                             (TABLE defaults to the one for BIN's build-id)
  typeids.py verify [TABLE] BIN --selftest   calibration: the hand-read rows in
                                             tools/typeids-hand.tsv must agree with the table
                                             and with what verify finds, and planted errors
                                             must fail
  typeids.py coverage [TABLE] [--ext DIR]    how many of upstream's EngineClass names have a
                                             row (the offline half of "how many bind")

On Windows upstream fills StaticSymbols::IndexSymbolToNameMaps by pattern-scanning the
registrant functions (GameHooks/DataLibrariesBG3Game.cpp); those scans are not built here.
bin/bg3's .symtab names every `ls::TypeId<T, Context>::m_TypeIndex` global, and this table
ships the subset upstream's UpdateComponentMappings() consumes, keyed by ELF address. It is
embedded into libbg3se-ext.so (ext/CMakeLists.txt; the .S carries the table's hash, because
zig's cache does not see what .incbin pulls in) and bound by ext/linux/typeids.cpp at setup;
a build with no table gets no ECS names and nothing else happens.

Contexts in scope -- the five UpdateComponentMappings() switches on -- with their Linux
names and the count on c8da62b1:
  ecs::ComponentTypeIdContext          components                     2107
  ecs::OneFrameComponentTypeIdContext  one-frame components            540
  ecs::SystemsContext                  systems                         934  (upstream calls it
                                       ecs::EntityWorld::SystemsContext; EntitySystem.cpp takes both)
  ecs::sync::ReplicatedTypeContext     replication ids                 582
  ls::ImmutableDataHeadmaster          static data (resource managers) 121
Out, deliberately, because nothing upstream reads them: ecs::UncheckedComponentTypeIdContext
(2652), ecs::QueryRegistry (4842), ls::model::TypeIdContext (1046), ui::UISystemsContext (29:
the UI world's own systems), and
the small ones. 4284 rows in all.

A row:
  kind     ctx | id
  addr     ELF virtual address (offset from the PIE load base). ctx: the 16-byte
           ls::TypeContext<context>::m_State; the registrant list head is the qword at +8.
           id: ls::TypeId<type, context>::m_TypeIndex, an int32
  context  the TypeId context
  type     id rows: T as llvm-cxxfilt renders it (no struct/class/enum keywords, `>>`).
           ctx rows: -

verify re-establishes every row from the binary two ways. `symtab`: .symtab has an object of
the expected size and name at addr -- the source the table came from, so a consistency check.
`registrant`: the witness that does not come from the symbol table. Exactly one function
refers to both addr and the context's list head, and that function -- or one it calls, the
name getter, for most components -- refers to the .rodata string
`ls::StringView ls::GetTypeName() [T = <type>]`, clang's __PRETTY_FUNCTION__ of ls::GetTypeName<T>,
from which the game builds the name it registers (the name-from-signature routine is
0x28ddf70 on c8da62b1). That is the Linux shape of the registrant upstream's Windows pattern
matches: a static node {index*, name ptr, name len, next} pushed onto the head. The hand
readings behind it are in tools/typeids-hand.tsv; on c8da62b1 it holds
for all 4284 rows, so a row it fails is wrong, not unlucky.
"""
import argparse
import collections
import csv
import glob
import os
import re
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import harvest  # noqa: E402

HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TABLE_DIR = os.path.join(HERE, 'typeids')
HARVEST_DIR = os.path.join(HERE, 'dumps', 'harvest')
HAND = os.path.join(HERE, 'tools', 'typeids-hand.tsv')
EXT_DEFS = os.path.join(HERE, 'ext', 'src', 'BG3Extender', 'GameDefinitions')
CONTEXTS = ['ecs::ComponentTypeIdContext', 'ecs::OneFrameComponentTypeIdContext',
            'ecs::SystemsContext', 'ecs::sync::ReplicatedTypeContext', 'ls::ImmutableDataHeadmaster']
COLUMNS = ['kind', 'addr', 'context', 'type']
STATE_SIZE, INDEX_SIZE, HEAD_OFF = 16, 4, 8
NAME_STRING_RE = re.compile(rb'ls::StringView ls::GetTypeName\(\) \[T = ([^\0]*)\]\0')
SLACK = 5            # a section-relative reference is S + A + 4 .. + 8 (harvest._reloc_views)


# ---------------------------------------------------------------- the file

def read_table(path):
    """(build-id, comment lines, rows as dicts)."""
    build, comments, rows = None, [], []
    with open(path) as f:
        lines = f.read().split('\n')
    for line in lines:
        if not line:
            continue
        if line.startswith('#'):
            comments.append(line)
            m = re.match(r'# build-id ([0-9a-f]+)$', line)
            if m:
                build = m.group(1)
            continue
        cells = line.split('\t')
        if cells == COLUMNS:
            continue
        if len(cells) != len(COLUMNS):
            sys.exit('%s: bad row: %r' % (path, line))
        rows.append(dict(zip(COLUMNS, cells)))
    if build is None:
        sys.exit('%s: no "# build-id" line' % path)
    return build, comments, rows


def write_table(path, comments, rows):
    tmp = path + '.tmp'
    with open(tmp, 'w') as f:
        for c in comments:
            f.write(c + '\n')
        f.write('\t'.join(COLUMNS) + '\n')
        for r in rows:
            f.write('\t'.join(r[c] for c in COLUMNS) + '\n')
    os.replace(tmp, path)


def find_table(build, d=TABLE_DIR):
    hits = [p for p in glob.glob(os.path.join(d, '*.tsv')) if os.path.basename(p).startswith(build)]
    if len(hits) != 1:
        sys.exit('%d tables for build %s in %s' % (len(hits), build, d))
    return hits[0]


def find_dir(build, d):
    hits = [p for p in glob.glob(os.path.join(d, build + '*')) if os.path.isdir(p)]
    if len(hits) != 1:
        sys.exit('%d directories for build %s in %s' % (len(hits), build, d))
    return hits[0]


# ---------------------------------------------------------------- emit

def emit(build, harvest_dir):
    d = find_dir(build, harvest_dir)
    build = os.path.basename(d)
    ctx_addr = {}
    with open(os.path.join(d, 'named-data.tsv')) as f:
        for r in csv.DictReader(f, delimiter='\t'):
            m = re.match(r'ls::TypeContext<(.+)>::m_State$', r['demangled'])
            if m and m.group(1) in CONTEXTS:
                if int(r['size']) != STATE_SIZE:
                    sys.exit('%s: %s is %s bytes, not %d' % (d, r['demangled'], r['size'], STATE_SIZE))
                ctx_addr[m.group(1)] = int(r['address'], 16)
    missing = [c for c in CONTEXTS if c not in ctx_addr]
    if missing:
        sys.exit('%s: no ls::TypeContext<C>::m_State for %s' % (d, ', '.join(missing)))
    ids = []
    with open(os.path.join(d, 'typeids.tsv')) as f:
        for r in csv.DictReader(f, delimiter='\t'):
            if r['context'] in CONTEXTS:
                ids.append(dict(kind='id', addr='%#x' % int(r['address'], 16), context=r['context'], type=r['type']))
    order = {c: i for i, c in enumerate(CONTEXTS)}
    ids.sort(key=lambda r: (order[r['context']], r['type'], r['addr']))
    rows = [dict(kind='ctx', addr='%#x' % ctx_addr[c], context=c, type='-') for c in CONTEXTS] + ids
    counts = collections.Counter(r['context'] for r in ids)
    src = ''
    try:
        import json
        src = json.load(open(os.path.join(d, 'summary.json'))).get('binary', '')
    except (OSError, ValueError):
        pass
    comments = [
        '# build-id ' + build,
        '# The ECS TypeId table for this build: the ls::TypeId<T, Context>::m_TypeIndex globals',
        '# upstream\'s UpdateComponentMappings() consumes, by ELF address, from bin/bg3\'s .symtab. Written by',
        '# tools/typeids.py emit from %s (tools/harvest.py on %s);' % (os.path.relpath(d, HERE), src or 'bin/bg3'),
        '# every row re-established by `tools/typeids.py verify` (symtab + registrant witness). Embedded into',
        '# libbg3se-ext.so and bound by ext/linux/typeids.cpp. ctx rows: addr = ls::TypeContext<context>::m_State',
        '# (16 bytes; the registrant list head is the qword at +8). id rows: addr = the int32 m_TypeIndex.',
        '# ' + ', '.join('%s %d' % (c, counts[c]) for c in CONTEXTS) + '; %d id rows' % len(ids),
    ]
    os.makedirs(TABLE_DIR, exist_ok=True)
    out = os.path.join(TABLE_DIR, build + '.tsv')
    write_table(out, comments, rows)
    print('%s: %d ctx + %d id rows' % (os.path.relpath(out, HERE), len(CONTEXTS), len(ids)))
    for c in CONTEXTS:
        print('  %-40s %5d' % (c, counts[c]))
    return out


# ---------------------------------------------------------------- verify

class Bin:
    def __init__(self, path):
        self.h = harvest.Harvest(path)                     # needs .rela.text
        h = self.h
        self.build = h.build_id
        typ = h.tab['info'] & 0xf
        self.obj_at = collections.defaultdict(list)        # addr -> [(size, mangled)]
        for i in np.nonzero(typ == harvest.STT_OBJECT)[0].tolist():
            self.obj_at[int(h.tab['value'][i])].append((int(h.tab['size'][i]), h.names[i]))
        ok = h.site_fde >= 0
        self.fde_targets = collections.defaultdict(set)   # fde -> exact targets it refers to
        for f, t in zip(h.site_fde[ok].tolist(), h.target[ok].tolist()):
            self.fde_targets[f].add(t)
        order = np.argsort(h.target)
        self.tgt_sorted = h.target[order]
        self.fde_by_tgt = h.site_fde[order]
        self.fstart_idx = {int(a): i for i, a in enumerate(h.fstart.tolist())}
        self.str_at = collections.defaultdict(list)       # type -> [.rodata addresses of its name string]
        ro = h.elf.sec('.rodata')
        for m in NAME_STRING_RE.finditer(h.elf.m, ro['off'], ro['off'] + ro['size']):
            self.str_at[m.group(1).decode('utf-8', 'replace')].append(ro['addr'] + m.start() - ro['off'])
        self._dem = {}

    def referrers(self, a):
        """FDEs with a relocation whose exact target is a."""
        lo = np.searchsorted(self.tgt_sorted, np.uint64(a))
        hi = np.searchsorted(self.tgt_sorted, np.uint64(a), side='right')
        return set(self.fde_by_tgt[lo:hi].tolist()) - {-1}

    def refers_string(self, f, ty):
        ts = self.fde_targets.get(f, ())
        return any(a + k in ts for a in self.str_at.get(ty, ()) for k in range(SLACK))

    def demangle(self, names):
        todo = [n for n in names if n not in self._dem]
        if todo:
            self._dem.update(zip(todo, harvest.demangle(todo)))
        return {n: self._dem[n] for n in names}


def check_row(b, r, heads):
    """(failed checks, (registrant fde start, 'same' | 'callee') or None)."""
    fails = []
    a = int(r['addr'], 16)
    objs = b.obj_at.get(a, [])
    dem = b.demangle([n for _, n in objs])
    if r['kind'] == 'ctx':
        want = 'ls::TypeContext<%s>::m_State' % r['context']
        if not any(sz == STATE_SIZE and dem[n] == want for sz, n in objs):
            fails.append('symtab')
        return fails, None
    want = 'ls::TypeId<%s, %s>::m_TypeIndex' % (r['type'], r['context'])
    if not any(sz == INDEX_SIZE and dem[n] == want for sz, n in objs):
        fails.append('symtab')
    head = heads.get(r['context'])
    reg = [f for f in b.referrers(a) if head is not None and head in b.fde_targets[f]]
    if len(reg) != 1:
        fails.append('registrant:%d' % len(reg))
        return fails, None
    f = reg[0]
    how = 'same' if b.refers_string(f, r['type']) else None
    if how is None:
        for t in b.fde_targets[f]:
            g = b.fstart_idx.get(t)
            if g is not None and b.refers_string(g, r['type']):
                how = 'callee'
                break
    if how is None:
        fails.append('name-string')
    return fails, (int(b.h.fstart[f]), how)


def heads_of(rows):
    return {r['context']: int(r['addr'], 16) + HEAD_OFF for r in rows if r['kind'] == 'ctx'}


def audit(b, rows, quiet=False):
    """Every row checked; returns [(row, fails)] for the rows that failed."""
    heads = heads_of(rows)
    stat = collections.defaultdict(collections.Counter)
    bad = []
    found = {}
    for r in rows:
        fails, reg = check_row(b, r, heads)
        key = r['context'] if r['kind'] == 'id' else 'ctx'
        stat[key]['rows'] += 1
        if fails:
            bad.append((r, fails))
        else:
            stat[key]['ok'] += 1
            if reg:
                stat[key][reg[1]] += 1
        found[(r['kind'], r['addr'])] = reg
    if not quiet:
        print('%-40s %5s %5s %5s %6s' % ('context', 'rows', 'ok', 'same', 'callee'))
        for key in ['ctx'] + CONTEXTS:
            s = stat[key]
            print('%-40s %5d %5d %5d %6d' % (key, s['rows'], s['ok'], s['same'], s['callee']))
        print('%d rows, %d failed' % (len(rows), len(bad)))
        for r, fails in bad[:40]:
            print('  FAIL %s %s %s %s: %s' % (r['kind'], r['addr'], r['context'], r['type'], ' '.join(fails)))
    return bad, found


def verify(b, table):
    build, _, rows = read_table(table)
    if build != b.build:
        sys.exit('%s is for build %s, the binary is %s' % (table, build, b.build))
    bad, _ = audit(b, rows)
    return not bad


# ---------------------------------------------------------------- selftest

def read_hand(build):
    rows = []
    with open(HAND) as f:
        for r in csv.DictReader((l for l in f if not l.startswith('#')), delimiter='\t'):
            if build.startswith(r['build']):
                rows.append(r)
    return rows


def selftest(b, table):
    build, _, rows = read_table(table)
    if build != b.build:
        sys.exit('%s is for build %s, the binary is %s' % (table, build, b.build))
    ok = True

    print('== calibration: the table as it stands')
    bad, found = audit(b, rows)
    if bad:
        ok = False
        print('FAIL  %d rows of the untouched table fail' % len(bad))

    print('== hand-read rows (%s)' % os.path.relpath(HAND, HERE))
    hand = read_hand(build)
    by_key = {(r['kind'], r['addr'], r['context'], r['type']): r for r in rows}
    n_hand = 0
    for hr in hand:
        key = (hr['kind'], hr['addr'], hr['context'], hr['type'])
        n_hand += 1
        if key not in by_key:
            ok = False
            print('FAIL  hand row not in the table: %s' % ' '.join(key))
            continue
        reg = found.get((hr['kind'], hr['addr']))
        if hr['kind'] == 'id':
            if reg is None or '%#x' % reg[0] != hr['registrant']:
                ok = False
                print('FAIL  %s %s: verify found registrant %s, hand read %s'
                      % (hr['addr'], hr['type'], '%#x' % reg[0] if reg else None, hr['registrant']))
                continue
        print('ok    %-4s %-10s %-36s %-50s %s' % (hr['kind'], hr['addr'], hr['context'], hr['type'][:50],
                                                  hr['registrant']))
    if n_hand < 1 + len(CONTEXTS):
        ok = False
        print('FAIL  only %d hand rows for this build; want a ctx row and one id row per context' % n_hand)

    print('== planted errors: each must fail, and by the check that should see it')
    heads = heads_of(rows)
    ids = [r for r in rows if r['kind'] == 'id']
    first = {}
    for r in ids:
        first.setdefault(r['context'], r)
    plants = []
    for c in CONTEXTS:
        r = dict(first[c])
        nxt = [x for x in ids if x['context'] == c and x['addr'] != r['addr']][0]
        other = [x for x in CONTEXTS if x != c][0]
        plants.append(('addr+4 (%s)' % c, dict(r, addr='%#x' % (int(r['addr'], 16) + 4)), {'symtab', 'registrant'}))
        plants.append(('type of a neighbour (%s)' % c, dict(r, type=nxt['type']), {'symtab', 'name-string'}))
        plants.append(('context %s -> %s' % (c, other), dict(r, context=other), {'symtab', 'registrant'}))
    for r in rows:
        if r['kind'] == 'ctx':
            plants.append(('ctx addr+16 (%s)' % r['context'], dict(r, addr='%#x' % (int(r['addr'], 16) + 16)), {'symtab'}))
            break
    # a real TypeId global of a context nobody consumes, relabelled as a component
    typeids, _ = b.h.named_data()
    foreign = [t for t in typeids if t[4] == 'ecs::UncheckedComponentTypeIdContext']
    if foreign:
        a, _, _, _, _, ty = foreign[0]
        plants.append(('an Unchecked TypeId relabelled as a component',
                       dict(kind='id', addr='%#x' % a, context=CONTEXTS[0], type=ty), {'symtab', 'registrant'}))
    # the head moved: every id row of that context loses its registrant
    r = first[CONTEXTS[0]]
    moved = dict(heads)
    moved[CONTEXTS[0]] += 8
    fails, _ = check_row(b, r, moved)
    plants_done = [('list head at +16 instead of +8', fails, {'registrant'})]
    for label, pr, expect in plants:
        fails, _ = check_row(b, pr, heads)
        plants_done.append((label, fails, expect))
    for label, fails, expect in plants_done:
        seen = {f.split(':')[0] for f in fails}
        hit = expect & seen
        if not fails or not hit:
            ok = False
            print('FAIL  %-48s fails: %s (expected one of %s)' % (label, ' '.join(fails) or '-', ' '.join(sorted(expect))))
        else:
            print('ok    %-48s caught by %s' % (label, ' '.join(sorted(seen))))
    print('SELFTEST %s' % ('OK' if ok else 'FAILED'))
    return ok


# ---------------------------------------------------------------- coverage

def msvc_to_itanium(name):
    """Upstream's EngineClass constants are MSVC __FUNCSIG__ spellings (`struct X<enum E> >`);
    the table's are llvm-cxxfilt's. The same normalisation lives in EntitySystem.cpp
    (EngineClassKey, PORT (linux))."""
    return re.sub(r'\b(struct|class|enum) ', '', name).replace('> >', '>>')


def upstream_names(ext_dir):
    """EngineClass names by what consumes them: components (MapComponentIndices, which looks
    in the component + one-frame + replication names), systems, resource managers."""
    comps, systems, resources = set(), set(), set()
    for root, _, files in os.walk(ext_dir):
        if 'Generated' in root:
            continue
        for fn in files:
            if not fn.endswith(('.h', '.inl')):
                continue
            text = open(os.path.join(root, fn), errors='replace').read()
            # the macro definitions themselves (Base/Base.h) are not uses
            text = '\n'.join(l for l in text.split('\n') if not l.lstrip().startswith('#define'))
            for m in re.finditer(r'DEFINE_(?:PROXY_|ONEFRAME_)?COMPONENT\(\s*\w+\s*,\s*"([^"]+)"', text):
                comps.add(m.group(1))
            for m in re.finditer(r'DEFINE_(?:ONEFRAME_)?TAG_COMPONENT\(\s*([\w:]+)\s*,\s*(\w+)', text):
                comps.add('%s::%s' % (m.group(1), m.group(2)))
            for m in re.finditer(r'DEFN_BOOST\(\s*(\w+)', text):
                comps.add('eoc::%sBoostComponent' % m.group(1))
            for m in re.finditer(r'DEFINE_SYSTEM\(\s*\w+\s*,\s*"([^"]+)"', text):
                systems.add(m.group(1))
            if fn == 'GuidResources.h':
                for m in re.finditer(r'EngineClass = "([^"]+)"', text):
                    resources.add(m.group(1))
            elif fn in ('Character.h', 'Item.h') or 'Components' not in root:
                # esv::Character & co. declare EngineClass by hand, with ComponentType beside it
                for m in re.finditer(r'ComponentType = ExtComponentType::\w+;\s*static constexpr auto EngineClass = "([^"]+)"', text):
                    comps.add(m.group(1))
    return comps, systems, resources


def coverage(table, ext_dir):
    _, _, rows = read_table(table)
    have = collections.defaultdict(set)
    for r in rows:
        if r['kind'] == 'id':
            have[r['context']].add(r['type'])
    comps, systems, resources = upstream_names(ext_dir)
    comp_pool = have['ecs::ComponentTypeIdContext'] | have['ecs::OneFrameComponentTypeIdContext'] | have['ecs::sync::ReplicatedTypeContext']
    ok = True
    for label, names, pool in (('components', comps, comp_pool), ('systems', systems, have['ecs::SystemsContext']),
                               ('resource managers', resources, have['ls::ImmutableDataHeadmaster'])):
        hit = {n for n in names if msvc_to_itanium(n) in pool}
        raw = {n for n in names if n in pool}
        print('%-18s %4d of %4d named in the table (%d without the MSVC->Itanium spelling fix)'
              % (label, len(hit), len(names), len(raw)))
        for n in sorted(names - hit):
            print('  missing: %s' % n)
    return ok


# ---------------------------------------------------------------- main

def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest='cmd', required=True)
    p = sub.add_parser('emit')
    p.add_argument('build')
    p.add_argument('--harvest', default=HARVEST_DIR)
    p = sub.add_parser('verify')
    p.add_argument('args', nargs='+', help='[TABLE] BIN')
    p.add_argument('--selftest', action='store_true')
    p = sub.add_parser('coverage')
    p.add_argument('table', nargs='?')
    p.add_argument('--ext', default=EXT_DEFS)
    a = ap.parse_args()
    if a.cmd == 'emit':
        emit(a.build, a.harvest)
    elif a.cmd == 'verify':
        binpath = a.args[-1]
        b = Bin(binpath)
        table = a.args[0] if len(a.args) > 1 else find_table(b.build)
        print('%s: build %s, table %s' % (binpath, b.build, os.path.relpath(table, HERE)))
        ok = selftest(b, table) if a.selftest else verify(b, table)
        sys.exit(0 if ok else 1)
    elif a.cmd == 'coverage':
        table = a.table
        if table is None:
            tables = glob.glob(os.path.join(TABLE_DIR, '*.tsv'))
            if len(tables) != 1:
                sys.exit('give the table: %d in %s' % (len(tables), TABLE_DIR))
            table = tables[0]
        sys.exit(0 if coverage(table, a.ext) else 1)


if __name__ == '__main__':
    main()
