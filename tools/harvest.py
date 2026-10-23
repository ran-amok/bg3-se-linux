#!/usr/bin/env python3
"""Harvest what bin/bg3 says about itself.

Reads .symtab, .rela.text and .eh_frame of an ELF linked with --emit-relocs and
writes, under dumps/harvest/<build-id>/ (never in the repo's history):

  functions.tsv   one row per FDE: start, size, name if .symtab has one
  typeids.tsv     ls::TypeId<T, Context>::m_TypeIndex globals: context, T, address
  named-data.tsv  every other named Larian data symbol (static registrators ...)
  anchors.tsv     named local -> the function(s) whose code references it
  type-mentions.tsv  function -> the T of each ls::GetTypeName<T>() inlined into it
  callgraph.npz   direct call/jump edges and function-address takes, by FDE index
  summary.json    the counts, including the calibration figures

Addresses are ELF virtual addresses, i.e. offsets from the PIE load base.

  harvest.py BIN                 full harvest
  harvest.py BIN --selftest      calibration only: run the anchor step on functions
                                 whose FDE <-> name mapping .symtab already gives
  harvest.py BIN --targets XML   also check upstream's BinaryMappings.xml targets

No third-party ELF library: numpy views over an mmap, because pyelftools needs
minutes for three million relocations.
"""
import argparse
import collections
import json
import mmap
import os
import re
import struct
import subprocess
import sys
import xml.etree.ElementTree as ET

import numpy as np

SYM = np.dtype([('name', '<u4'), ('info', 'u1'), ('other', 'u1'),
                ('shndx', '<u2'), ('value', '<u8'), ('size', '<u8')])
RELA = np.dtype([('off', '<u8'), ('info', '<u8'), ('addend', '<i8')])
STT_OBJECT, STT_FUNC, STT_SECTION, STT_TLS = 1, 2, 3, 6
R_PC32, R_PLT32 = 2, 4

LARIAN_NS = ('ls', 'esv', 'ecl', 'eoc', 'ecs', 'stats', 'osi', 'gui', 'ui', 'net',
             'game', 'rpg', 'App', 'lsf', 'aspk', 'gn', 'rf')


class Elf:
    def __init__(self, path):
        self.f = open(path, 'rb')
        self.m = mmap.mmap(self.f.fileno(), 0, access=mmap.ACCESS_READ)
        m = self.m
        if m[:6] != b'\x7fELF\x02\x01' or struct.unpack_from('<H', m, 18)[0] != 62:
            sys.exit('not a little-endian x86-64 ELF64')
        shoff, = struct.unpack_from('<Q', m, 0x28)
        shentsize, shnum, shstrndx = struct.unpack_from('<HHH', m, 0x3a)
        raw = [struct.unpack_from('<IIQQQQIIQQ', m, shoff + i * shentsize)
               for i in range(shnum)]
        stroff = raw[shstrndx][4]
        self.sections = {}
        self.secaddr = np.zeros(shnum, np.uint64)
        for i, s in enumerate(raw):
            end = m.find(b'\0', stroff + s[0])
            name = m[stroff + s[0]:end].decode()
            self.sections[name] = dict(index=i, type=s[1], flags=s[2], addr=s[3], off=s[4],
                                       size=s[5], link=s[6])
            self.secaddr[i] = s[3]

    def sec(self, name):
        return self.sections.get(name)

    def build_id(self):
        s = self.sec('.note.gnu.build-id')
        if not s:
            return None
        namesz, descsz, _ = struct.unpack_from('<III', self.m, s['off'])
        o = s['off'] + 12 + ((namesz + 3) & ~3)
        return self.m[o:o + descsz].hex()

    def symbols(self):
        s = self.sec('.symtab')
        if not s:
            return None, None
        tab = np.frombuffer(self.m, SYM, count=s['size'] // 24, offset=s['off'])
        st = [v for v in self.sections.values() if v['index'] == s['link']][0]
        blob = self.m[st['off']:st['off'] + st['size']]
        names = []
        for o in tab['name'].tolist():
            names.append(blob[o:blob.index(b'\0', o)].decode('utf-8', 'replace'))
        return tab, names

    def rela(self, name):
        s = self.sec(name)
        if not s:
            return None
        return np.frombuffer(self.m, RELA, count=s['size'] // 24, offset=s['off'])

    def fdes(self):
        """(start, size) of every FDE with a non-empty range, sorted by start."""
        s = self.sec('.eh_frame')
        m, base, p = self.m, s['addr'] - s['off'], s['off']
        end = p + s['size']
        cies, out, empty = {}, [], 0
        while p < end:
            ln, = struct.unpack_from('<I', m, p)
            if ln == 0:
                break
            if ln == 0xffffffff:
                sys.exit('.eh_frame: 64-bit DWARF length not handled')
            body = p + 4
            cid, = struct.unpack_from('<I', m, body)
            if cid == 0:
                cies[p] = self._cie_fde_encoding(body + 4)
            else:
                if cies[body - cid] != 0x1b:  # DW_EH_PE_pcrel | sdata4
                    sys.exit('.eh_frame: FDE pointer encoding %#x not handled'
                             % cies[body - cid])
                rel, rng = struct.unpack_from('<iI', m, body + 4)
                if rng:
                    out.append((body + 4 + base + rel, rng))
                else:
                    empty += 1
            p = body + ln
        fd = np.array(sorted(out), dtype=np.uint64)
        return fd, empty

    def _cie_fde_encoding(self, q):
        m = self.m
        ver = m[q]
        q += 1
        e = m.find(b'\0', q)
        aug = m[q:e].decode()
        q = e + 1
        q = self._skip_leb(q)            # code alignment
        q = self._skip_leb(q)            # data alignment
        q = q + 1 if ver == 1 else self._skip_leb(q)   # return address register
        enc = 0
        if aug.startswith('z'):
            q = self._skip_leb(q)
            for c in aug[1:]:
                if c == 'R':
                    enc = m[q]
                    q += 1
                elif c == 'L':
                    q += 1
                elif c == 'P':
                    q += 1 + {0: 8, 2: 2, 3: 4, 4: 8, 0xa: 2, 0xb: 4, 0xc: 8}[m[q] & 0xf]
        return enc

    def _skip_leb(self, q):
        while self.m[q] & 0x80:
            q += 1
        return q + 1


def demangle(names):
    if not names:
        return []
    r = subprocess.run(['llvm-cxxfilt'], input='\n'.join(names), text=True,
                       capture_output=True, check=True)
    out = r.stdout.split('\n')[:len(names)]
    assert len(out) == len(names)
    return out


# ---------------------------------------------------------------- owners

LOCAL_PREFIXES = (('.L__FUNCTION__.', 'FUNCTION'), ('.L__func__.', 'func'),
                  ('.L__PRETTY_FUNCTION__.', 'PRETTY_FUNCTION'),
                  ('.Lswitch.table.', 'switch'), ('.L__const.', 'const'))


def split_static_owner(local, is_valid):
    """_ZZ<encoding>E<entity> -> _Z<encoding>. The encoding can itself contain
    'E', so take the longest prefix that is_valid() accepts as an owner."""
    body = local[5:] if local.startswith('_ZGVZ') else local[3:]
    cands = ['_Z' + body[:i] for i, c in enumerate(body) if c == 'E']
    for c in reversed(cands):
        if is_valid(c, local):
            return c
    return None


def owners_of_locals(tab, names, funcnames):
    """index of named local -> (owner mangled name, kind). Function-statics are
    split with the demangler; `funcnames` lets the selftest check that split."""
    typ = tab['info'] & 0xf
    owner = {}
    statics = []
    for i, n in enumerate(names):
        if typ[i] not in (STT_OBJECT, STT_TLS):
            continue
        for pre, kind in LOCAL_PREFIXES:
            if n.startswith(pre):
                o = n[len(pre):]
                if '.' in o:                      # .Lswitch.table.<fn>.<n>, .L__const.<fn>.<var>
                    o = o[:o.index('.')]
                if o.startswith('_Z'):
                    owner[i] = (o, kind)
                break
        else:
            if n.startswith(('_ZZ', '_ZGVZ')):
                statics.append(i)
    cands = set()
    for i in statics:
        n = names[i]
        body = n[5:] if n.startswith('_ZGVZ') else n[3:]
        cands.update('_Z' + body[:k] for k, c in enumerate(body) if c == 'E')
    cands = sorted(cands)
    dem = dict(zip(cands, demangle(cands)))
    ldem = dict(zip((names[i] for i in statics), demangle([names[i] for i in statics])))

    def valid(c, local):
        d = dem[c]
        if d == c or '(' not in d:
            return False
        ld = re.sub(r'^guard variable for ', '', ldem[local])
        return (d + '::') in ld

    unsplit = 0
    for i in statics:
        o = split_static_owner(names[i], valid)
        if o is None:
            unsplit += 1
        else:
            owner[i] = (o, 'guard' if names[i].startswith('_ZGVZ') else 'static')
    return owner, unsplit


# ---------------------------------------------------------------- core

class Harvest:
    def __init__(self, path, need_relocs=True):
        self.elf = Elf(path)
        e = self.elf
        self.build_id = e.build_id()
        self.tab, self.names = e.symbols()
        if self.tab is None:
            sys.exit('no .symtab: this build is stripped, nothing to harvest')
        self.rel = e.rela('.rela.text')
        if self.rel is None and need_relocs:
            sys.exit('no .rela.text: not linked with --emit-relocs')
        self.fd, self.empty_fdes = e.fdes()
        self.fstart = self.fd[:, 0]
        self.fend = self.fd[:, 0] + self.fd[:, 1]
        t = e.sec('.text')
        self.text = (t['addr'], t['addr'] + t['size'])
        typ = self.tab['info'] & 0xf
        self.styp = typ
        infunc = (typ == STT_FUNC) & (self.tab['shndx'] == t['index'])
        self.func_idx = np.nonzero(infunc)[0]
        self.funcname_at = {}
        for i in self.func_idx.tolist():
            self.funcname_at.setdefault(int(self.tab['value'][i]), self.names[i])
        if self.rel is not None:
            self._reloc_views()

    def fde_of(self, addrs):
        """FDE index containing each address, -1 where none does."""
        addrs = np.asarray(addrs, dtype=np.uint64)
        i = np.searchsorted(self.fstart, addrs, side='right') - 1
        ok = (i >= 0) & (addrs < self.fend[np.maximum(i, 0)])
        return np.where(ok, i, -1)

    def _reloc_views(self):
        r = self.rel
        self.rtype = (r['info'] & 0xffffffff).astype(np.uint32)
        self.rsym = (r['info'] >> 32).astype(np.uint32)
        self.site = r['off']                       # .rela.text offsets are addresses
        if self.elf.sec('.text')['addr'] > int(self.site.min()):
            self.site = self.site + np.uint64(self.elf.sec('.text')['addr'])
        # S + A + 4: the referenced address when the 32-bit field ends the
        # instruction. An immediate after the field makes it S + A + 5..8, so a
        # section-relative data target can be off by that much; symbol-relative
        # references are exact because the symbol identifies the target.
        s = self.tab['value'][self.rsym].astype(np.int64)
        self.target = (s + r['addend'] + 4).astype(np.uint64)
        self.site_fde = self.fde_of(self.site)

    # -- tables

    def functions_rows(self):
        for i, (a, n) in enumerate(self.fd.tolist()):
            yield i, a, n, self.funcname_at.get(a, '')

    def named_data(self):
        """Named Larian data symbols, TypeIds apart from the rest."""
        idx = [i for i in np.nonzero((self.styp == STT_OBJECT) | (self.styp == STT_TLS))[0].tolist()
               if self.names[i].startswith('_ZN')]
        dem = demangle([self.names[i] for i in idx])
        typeids, other = [], []
        for i, d in zip(idx, dem):
            if d.split('::', 1)[0].split('<', 1)[0] not in LARIAN_NS:
                continue
            row = (int(self.tab['value'][i]), int(self.tab['size'][i]), self.names[i], d)
            m = re.match(r'ls::TypeId<(.*), ([^<>,]+)>::m_TypeIndex$', d)
            if m:
                typeids.append(row + (m.group(2), m.group(1)))
            else:
                other.append(row)
        return typeids, other

    def callgraph(self):
        """Direct transfers (PLT32) and address takes (PC32 to a function start)."""
        lo, hi = self.text
        tin = (self.target >= lo) & (self.target < hi)
        call = (self.rtype == R_PLT32) & tin
        callee = self.fde_of(self.target[call])
        exact = callee >= 0
        exact[exact] = self.fstart[callee[exact]] == self.target[call][exact]
        take = (self.rtype == R_PC32) & tin
        taken = self.fde_of(self.target[take])
        tex = taken >= 0
        tex[tex] = self.fstart[taken[tex]] == self.target[take][tex]
        stats = dict(call_relocs=int(call.sum()), call_to_fde_start=int(exact.sum()),
                     call_from_no_fde=int((self.site_fde[call] < 0).sum()),
                     call_external_plt=int(((self.rtype == R_PLT32) & ~tin).sum()),
                     address_takes=int(tex.sum()))
        keep = exact & (self.site_fde[call] >= 0)
        edges = np.unique(np.stack([self.site_fde[call][keep], callee[keep]], 1), axis=0)
        tk = tex & (self.site_fde[take] >= 0)
        takes = np.unique(np.stack([self.site_fde[take][tk], taken[tk]], 1), axis=0)
        stats['call_edges'] = len(edges)
        stats['take_edges'] = len(takes)
        return edges, takes, stats

    # -- anchors

    def anchors(self):
        """owner mangled name -> {'kinds', 'locals', 'fdes': Counter(fde -> refs)}"""
        funcnames = set(self.names[i] for i in self.func_idx.tolist())
        owner, unsplit = owners_of_locals(self.tab, self.names, funcnames)
        self.unsplit_statics = unsplit
        is_local = np.zeros(len(self.tab), bool)
        is_local[list(owner)] = True
        sel = is_local[self.rsym]
        out = {}
        for i, (o, kind) in owner.items():
            a = out.setdefault(o, dict(kinds=set(), locals=0, fdes=collections.Counter(),
                                       nofde=0))
            a['kinds'].add(kind)
            a['locals'] += 1
        for s, f in zip(self.rsym[sel].tolist(), self.site_fde[sel].tolist()):
            a = out[owner[s][0]]
            if f < 0:
                a['nofde'] += 1
            else:
                a['fdes'][f] += 1
        return out


def is_type_mention(owner):
    """ls::GetTypeName<T>() exists only inlined: its __PRETTY_FUNCTION__ local
    says "this function mentions T", never "this function is"."""
    return owner.startswith('_ZN2lsL11GetTypeNameI')   # L: internal linkage


def resolve(anchors):
    """Decide which referencing FDE is the owner itself. Returns owner -> (status,
    fde), fde -1 when undecided.

      single       one FDE references the owner's locals, and no other owner
                   makes the same claim on it. Calibrated by --selftest.
      conflict     one FDE, but another owner's locals also live only there: at
                   least one of the two exists only as an inlined copy.
      elimination  several FDEs, all but one of them claimed by a `single` owner.
                   Assumes an out-of-line copy exists. NOT calibrated: the
                   third-party code has almost no multi-FDE owners to test on.
      ambiguous    several FDEs left.
    """
    own = {o: a for o, a in anchors.items() if not is_type_mention(o)}
    claims = collections.defaultdict(list)
    for o, a in own.items():
        if len(a['fdes']) == 1:
            claims[next(iter(a['fdes']))].append(o)
    res = {}
    for o, a in own.items():
        fs = list(a['fdes'])
        if not fs:
            res[o] = ('unreferenced', -1)
        elif len(fs) == 1:
            res[o] = ('single', fs[0]) if len(claims[fs[0]]) == 1 else ('conflict', -1)
    taken = {f for st, f in res.values() if st == 'single'}
    for o, a in own.items():
        if o in res:
            continue
        left = [f for f in a['fdes'] if f not in taken]
        res[o] = ('elimination', left[0]) if len(left) == 1 else ('ambiguous', -1)
    return res


# ---------------------------------------------------------------- selftest

def selftest(h, verbose=True):
    """Calibrate on third-party code, where .symtab names the functions.

    1. FDE <-> function: every FUNC symbol in .text should start an FDE of its size.
    2. Anchor step: for named locals whose owner is a named FUNC, how often does
       each resolve() status point at the owner's own FDE?
    """
    out = {}
    fn = h.tab[h.func_idx]
    fn = fn[fn['size'] > 0]
    i = h.fde_of(fn['value'])
    start = (i >= 0) & (h.fstart[np.maximum(i, 0)] == fn['value'])
    size = start & (h.fd[np.maximum(i, 0), 1] == fn['size'])
    out['fde_vs_func'] = dict(funcs=len(fn), starts_an_fde=int(start.sum()),
                              same_size=int(size.sum()),
                              inside_other_fde=int(((i >= 0) & ~start).sum()),
                              no_fde=int((i < 0).sum()))

    anchors = h.anchors()
    res = resolve(anchors)
    addr_of = {}
    for k in h.func_idx.tolist():
        addr_of.setdefault(h.names[k], int(h.tab['value'][k]))
    known = {o: a for o, a in anchors.items() if o in addr_of}
    tally = collections.defaultdict(lambda: dict(right=0, wrong=0, undecided=0))
    truth_in_refs = 0
    wrong = []
    for o, a in known.items():
        if o not in res:
            continue
        t = int(h.fde_of([addr_of[o]])[0])
        truth_in_refs += t in a['fdes']
        status, f = res[o]
        if f < 0:
            tally[status]['undecided'] += 1
        elif f == t:
            tally[status]['right'] += 1
        else:
            tally[status]['wrong'] += 1
            wrong.append((status, o))
    out['anchor_calibration'] = dict(
        owners_with_known_function=sum(1 for o in known if o in res),
        owner_fde_among_referencing=truth_in_refs,
        by_status=dict(sorted(tally.items())),
        unsplit_function_statics=h.unsplit_statics)
    if verbose:
        print(json.dumps(out, indent=1))
        for status, o in wrong[:40]:
            print('WRONG', status, o[:150])
    return out, anchors, res


# ---------------------------------------------------------------- upstream targets

def cstring_refs(h, value):
    """Where a NUL-terminated `value` sits in .rodata (LLD tail-merges, so a copy
    may be the tail of a longer string) and which FDEs / data words point at it.
    None if the string is not there at all."""
    ro = h.elf.sec('.rodata')
    m, needle = h.elf.m, value.encode() + b'\0'
    whole, tail, p = [], [], ro['off']
    while True:
        p = m.find(needle, p, ro['off'] + ro['size'])
        if p < 0:
            break
        (whole if p == ro['off'] or m[p - 1] == 0 else tail).append(p - ro['off'] + ro['addr'])
        p += 1
    if not whole and not tail:
        return None
    addrs = np.array(whole + tail, np.uint64)
    hit = np.isin(h.target, addrs) & (h.rtype == R_PC32)
    data = 0
    for sec in ('.rela.data.rel.ro', '.rela.data'):
        r = h.elf.rela(sec)
        if r is not None:
            abs64 = (r['info'] & 0xffffffff) == 1
            t = (h.tab['value'][(r['info'] >> 32).astype(np.uint32)].astype(np.int64)
                 + r['addend']).astype(np.uint64)
            data += int((np.isin(t, addrs) & abs64).sum())
    return dict(whole=len(whole), tail=len(tail), data_refs=data,
                fdes=sorted(set(h.site_fde[hit].tolist()) - {-1}))


def qualified(demangled):
    """'void ns::C<T>::f(int) const' -> 'ns::C::f', for matching upstream's names."""
    d, depth, out = demangled, 0, []
    for ch in d:                                   # drop <...> and (...) groups
        if ch in '<(':
            depth += 1
        elif ch in '>)':
            depth -= 1
        elif depth == 0:
            out.append(ch)
    return ''.join(out).replace(' const', '').strip().split(' ')[-1]


def check_targets(h, xml, anchors, res, edges):
    """Upstream's mapping targets against what this binary names by itself.

    Per target symbol, the strongest of:
      named      .symtab has a FUNC of that qualified name (third-party code)
      anchored   an anchor owner of that name, with its resolve() status
      string     the mapping's String condition occurs here and exactly one
                 function references it: the *pattern site* is located, and the
                 target is one of `site_refs` things that function refers to
      class      resolved anchors exist in the same class; `neighbours` is how
                 many functions call or are called by them (a candidate set, not
                 a location)
      none
    Upstream's names are IDA labels, not mangled names, so matching is by
    qualified name with templates and parameters dropped."""
    root = ET.parse(xml).getroot()
    fnames = sorted(set(h.names[i] for i in h.func_idx.tolist()))
    named = collections.defaultdict(list)
    for n, d in zip(fnames, demangle(fnames)):
        named[qualified(d).lower()].append(n)
    own = collections.defaultdict(list)
    for o, d in zip(res, demangle(list(res))):
        own[qualified(d).lower()].append(o)
    out_refs = collections.Counter(h.site_fde[h.site_fde >= 0].tolist())

    def neighbours(fdes):
        f = np.array(sorted(fdes))
        n = set(edges[np.isin(edges[:, 0], f), 1].tolist()) | set(edges[np.isin(edges[:, 1], f), 0].tolist())
        return len(n - set(fdes))

    rows = []
    for mp in root.iter('Mapping'):
        strings = {c.get('Value'): cstring_refs(h, c.get('Value'))
                   for c in mp.iter('Condition') if c.get('Type') == 'String'}
        site = [s['fdes'][0] for s in strings.values() if s and len(s['fdes']) == 1]
        syms = list(dict.fromkeys(t.get('Symbol') for t in mp.iter('Target') if t.get('Symbol')))
        for sym in syms:
            q = sym.replace('__', '::').lower()   # upstream: EoCClient, binary: EocClient
            row = dict(mapping=mp.get('Name'), symbol=sym, verdict='none')
            cls = q.rsplit('::', 1)[0] if q.count('::') >= 2 else None
            near = [res[o][1] for k, os_ in own.items() if cls and k.startswith(cls + '::')
                    for o in os_ if res[o][1] >= 0]
            if q in named:
                row.update(verdict='named', symtab=named[q][:4])
            elif q in own:
                row.update(verdict='anchored', anchors=[(o, res[o][0]) for o in own[q]])
            elif site:
                row.update(verdict='string', site='%#x' % h.fstart[site[0]],
                           site_refs=out_refs[site[0]])
            elif near:
                row.update(verdict='class', cls=cls, class_anchors=len(near),
                           neighbours=neighbours(near))
            rows.append(row)
        if not syms:
            rows.append(dict(mapping=mp.get('Name'), symbol=None, verdict='none'))
        rows[-1]['strings'] = {k: v for k, v in strings.items()}
    return rows


# ---------------------------------------------------------------- main

def write_tsv(path, header, rows):
    with open(path, 'w') as f:
        f.write('\t'.join(header) + '\n')
        for r in rows:
            f.write('\t'.join(str(x) for x in r) + '\n')


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('binary')
    ap.add_argument('--out', help='output directory (default dumps/harvest/<build-id>)')
    ap.add_argument('--selftest', action='store_true')
    ap.add_argument('--targets', metavar='XML', help="upstream's BinaryMappings.xml")
    a = ap.parse_args()

    h = Harvest(a.binary)
    if a.selftest:
        selftest(h)
        return
    out = a.out or os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
                                'dumps', 'harvest', h.build_id or 'unknown')
    os.makedirs(out, exist_ok=True)
    summary = dict(binary=os.path.abspath(a.binary), build_id=h.build_id,
                   fdes=len(h.fd), empty_fdes=h.empty_fdes, text_relocs=len(h.rel))

    write_tsv(os.path.join(out, 'functions.tsv'), ('fde', 'start', 'size', 'symbol'),
              ((i, '%#x' % s, n, nm) for i, s, n, nm in h.functions_rows()))

    typeids, other = h.named_data()
    write_tsv(os.path.join(out, 'typeids.tsv'), ('address', 'context', 'type', 'mangled'),
              (('%#x' % r[0], r[4], r[5], r[2]) for r in sorted(typeids, key=lambda r: (r[4], r[5]))))
    write_tsv(os.path.join(out, 'named-data.tsv'), ('address', 'size', 'demangled', 'mangled'),
              (('%#x' % r[0], r[1], r[3], r[2]) for r in sorted(other, key=lambda r: r[3])))
    summary['typeids'] = len(typeids)
    summary['typeids_by_context'] = dict(collections.Counter(r[4] for r in typeids).most_common())
    summary['other_named_larian_data'] = len(other)

    edges, takes, cg = h.callgraph()
    np.savez_compressed(os.path.join(out, 'callgraph.npz'), fde=h.fd, calls=edges, takes=takes)
    summary['callgraph'] = cg

    cal, anchors, res = selftest(h, verbose=False)
    summary['selftest'] = cal
    dem = dict(zip(anchors, demangle(list(anchors))))
    rows, mentions = [], []
    for o, an in sorted(anchors.items(), key=lambda kv: dem[kv[0]]):
        if o not in res:                       # GetTypeName<T>() and the like
            mentions.extend(('%#x' % h.fstart[k], dem[o]) for k in sorted(an['fdes']))
            continue
        status, f = res[o]
        rows.append((status, '%#x' % h.fstart[f] if f >= 0 else '',
                     ','.join(sorted(an['kinds'])), an['locals'],
                     ' '.join('%#x:%d' % (h.fstart[k], n) for k, n in sorted(an['fdes'].items())),
                     dem[o], o))
    write_tsv(os.path.join(out, 'anchors.tsv'),
              ('status', 'function', 'kinds', 'locals', 'referencing_fdes',
               'demangled', 'mangled'), rows)
    write_tsv(os.path.join(out, 'type-mentions.tsv'), ('function', 'pretty_function_owner'),
              sorted(mentions))
    larian = [r for r in rows if qualified(r[5]).split('::')[0] in LARIAN_NS]
    summary['anchors'] = dict(collections.Counter(r[0] for r in rows))
    summary['anchors_larian'] = dict(collections.Counter(r[0] for r in larian))
    summary['type_mentions'] = len(mentions)

    if a.targets:
        t = check_targets(h, a.targets, anchors, res, edges)
        with open(os.path.join(out, 'upstream-targets.json'), 'w') as f:
            json.dump(t, f, indent=1)
        summary['upstream_target_symbols'] = dict(collections.Counter(r['verdict'] for r in t))
    with open(os.path.join(out, 'summary.json'), 'w') as f:
        json.dump(summary, f, indent=1)
    print(json.dumps(summary, indent=1))
    print('wrote', out)


if __name__ == '__main__':
    main()
