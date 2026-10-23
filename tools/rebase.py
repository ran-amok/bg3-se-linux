#!/usr/bin/env python3
"""Carry addresses from one build of bin/bg3 to the next, when the next is a small patch.

  rebase.py OLD NEW summary                  the pairing: FDE runs, changed functions,
                                             section deltas
  rebase.py OLD NEW map ADDR...              one address each, or why not
  rebase.py OLD NEW manifest IN.tsv -o OUT   a manifest for NEW from OLD's: addr, the
                                             addresses in checks and in notes, the header
  rebase.py OLD NEW hand FILE PREFIXLEN      append NEW's rows to a per-build hand TSV
                                             (tools/targets-hand.tsv, typeids-hand.tsv):
                                             OLD's rows with every address carried over
  rebase.py OLD NEW --selftest               calibration, below
  --keep 0xA,0xB   (manifest, hand) hex tokens a person has read as constants, not
                   addresses (0x1ffffff, GlobalAllocator::Free's segment mask): left as they are

What it relies on, and refuses without: both builds have the same number of FDEs and pair
one for one (same order), every allocated section other than .text has the same size, and
the two .symtabs have the same length. Build 25605617 (a187af94) over 24532579 (c8da62b1)
is that case: 372531 FDEs each, 9 of them changed size, code moving in 7 runs of constant
delta and every section after .text by +0x80. A patch that adds
or drops a function is not this case, and the matcher pipeline (features.py, match.py,
targets.py) is the route for it.

An address maps when it lies in a function whose size is unchanged (+ that FDE's delta),
is the start of a changed one (flagged: its body is new and must be read again), sits in
padding between two FDEs with the same delta, or lies in a non-.text section (+ that
section's delta). The interior of a changed function does not map; nothing is guessed.

--selftest: every .symtab entry with an address in an allocated section must pair by index
and name, and map(old value) must equal the new value, for all but symbols inside changed
functions (those are counted as refused, not agreed). Then two plants must each produce
disagreements: one FDE run's delta shifted by 16, and the .data delta shifted by 8.
"""
import bisect
import os
import re
import struct
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from harvest import Elf  # noqa: E402

HEX = re.compile(r'\b0x[0-9a-fA-F]+\b')
KEEP = set()


class Rebase:
    def __init__(self, old, new):
        self.old, self.new = Elf(old), Elf(new)
        self.old_path, self.new_path = old, new
        o, n = self.old, self.new
        self.old_id, self.new_id = o.build_id(), n.build_id()
        self.secs = []                      # (start, end, delta, name): mapped whole
        self.unproven = []                  # (start, end, name): refused
        t_addr = o.sec('.text')['addr']
        for name, s in o.sections.items():
            # .relro_padding is lld's NOBITS filler up to the page; it absorbs the shift
            # and holds nothing, so an address in it is refused like any unmapped one
            if not s['addr'] or name in ('.text', '.relro_padding', '.rodata'):
                continue
            t = n.sec(name)
            if t is None or t['size'] != s['size']:
                sys.exit('section %s: %s in the new build; not a rebase case'
                         % (name, 'absent' if t is None else 'resized'))
            span = (s['addr'], s['addr'] + s['size'])
            if s['addr'] < t_addr and self._content(o, s) != self._content(n, t):
                # before .text nothing moves, so a section there maps only if it is
                # byte-identical (.eh_frame, .dynsym and the like are not)
                self.unproven.append(span + (name,))
                continue
            self.secs.append(span + (t['addr'] - s['addr'], name))
        self.secs.sort()
        self.rod_old = o.sec('.rodata')
        self.rod_new = n.sec('.rodata')
        if self.rod_new['addr'] != self.rod_old['addr'] or self.rod_new['size'] != self.rod_old['size']:
            sys.exit('.rodata moved or resized; not a rebase case')
        self.text = o.sec('.text')
        if n.sec('.text')['addr'] != self.text['addr']:
            sys.exit('.text moved; not a rebase case')
        ofd, _ = o.fdes()
        nfd, _ = n.fdes()
        if len(ofd) != len(nfd):
            sys.exit('FDE count %d -> %d; not a rebase case' % (len(ofd), len(nfd)))
        self.ostart = ofd[:, 0].astype(np.int64)
        self.osize = ofd[:, 1].astype(np.int64)
        self.nstart = nfd[:, 0].astype(np.int64)
        self.nsize = nfd[:, 1].astype(np.int64)
        self.delta = self.nstart - self.ostart
        self.changed = self.osize != self.nsize
        self._ostart_list = self.ostart.tolist()
        # a function whose code changed but not its size (0x23d0150 on a187af94: the same
        # instructions reordered; 0x3f22fd0: the version string inlined as immediates) is
        # changed too: its bytes, relocation sites masked, are what decides
        _, _, pos = self.text_census(positions=True)
        i = np.searchsorted(self.ostart, pos, side='right') - 1
        inside = (i >= 0) & (pos < self.ostart[np.maximum(i, 0)] + self.osize[np.maximum(i, 0)])
        self.resized = self.changed.copy()
        self.changed[np.unique(i[inside])] = True
        self.gap_diffs = pos[~inside]
        self.rod_runs = self.verify_rodata(self.rodata_candidates())

    @staticmethod
    def _content(e, s):
        if s['type'] == 8:                  # SHT_NOBITS
            return b''
        return bytes(e.m[s['off']:s['off'] + s['size']])

    def _sym_pairs(self):
        """(old value, new value, name, section name) for every .symtab entry that has an
        address: paired by index, names equal. TLS symbols carry offsets, not addresses."""
        if getattr(self, '_pairs', None) is None:
            ot, on = self.old.symbols()
            nt, nn = self.new.symbols()
            if len(ot) != len(nt):
                sys.exit('.symtab length %d -> %d; not a rebase case' % (len(ot), len(nt)))
            secname = {s['index']: k for k, s in self.old.sections.items()}
            pairs, namediff = [], 0
            ov, nv = ot['value'].tolist(), nt['value'].tolist()
            typ, sh = (ot['info'] & 0xf).tolist(), ot['shndx'].tolist()
            for i in range(len(ot)):
                if on[i] != nn[i]:
                    namediff += 1
                    continue
                if not ov[i] or sh[i] == 0 or sh[i] >= 0xff00 or typ[i] in (3, 6):  # SECTION, TLS
                    continue
                pairs.append((ov[i], nv[i], on[i], secname.get(sh[i])))
            self._pairs, self._namediff = pairs, namediff
        return self._pairs

    def rodata_candidates(self):
        """Runs of constant delta in .rodata, from the symtab: [(start, end, delta)]."""
        pts = sorted({(a, b - a) for a, b, _, sec in self._sym_pairs() if sec == '.rodata'})
        r0, r1 = self.rod_old['addr'], self.rod_old['addr'] + self.rod_old['size']
        runs = []
        for a, d in pts:
            if runs and runs[-1][2] == d:
                continue
            if runs:
                runs[-1][1] = a
            runs.append([a if runs else r0, r1, d])
        return [tuple(x) for x in runs]

    def verify_rodata(self, cands):
        """Each candidate run, proven by its bytes: old[start:end] against new at +delta.
        A difference is explained when it is an aligned int32 that changed by the gap
        between two known deltas -- a rel32 jump-table entry or a relative pointer, whose
        table stayed and whose target moved (32k of them on a187af94: +0x80, +0x50, +0x40
        into moved code, -14 into the moved strings). An unexplained difference is cut out:
        the run ends at the last NUL before it and resumes after the first NUL past it (the
        section is mostly C strings, so no part of a string that changed is kept)."""
        o, n = self.rod_old, self.rod_new
        old = self._content(self.old, o)
        new = self._content(self.new, n)
        base = o['addr']
        known = {int(x) for x in set(self.delta.tolist())}
        known |= {d for _, _, d, _ in self.secs} | {d for _, _, d in cands}
        allowed = {x - y for x in known for y in known} - {0}
        out = []
        for a, b, d in cands:
            x0, x1 = a - base, b - base
            if x0 + d < 0 or x1 + d > len(new):
                continue
            ov = np.frombuffer(old, np.uint8, x1 - x0, x0)
            nv = np.frombuffer(new, np.uint8, x1 - x0, x0 + d)
            bad = set()
            for k in np.nonzero(ov != nv)[0].tolist():
                w = (x0 + k) & ~3                       # the aligned word, old-relative
                if w in bad:
                    continue
                if w + 4 <= len(old) and w + d + 4 <= len(new) and w + d >= 0:
                    diff = (struct.unpack_from('<i', new, w + d)[0]
                            - struct.unpack_from('<i', old, w)[0])
                    if diff in allowed:
                        continue
                bad.add(x0 + k)
            cur = x0
            for k in sorted(bad):
                if k < cur:
                    continue
                nul = old.rfind(b'\0', cur, k)
                if nul >= 0 and nul + 1 > cur:
                    out.append((base + cur, base + nul + 1, d))
                nxt = old.find(b'\0', k)
                cur = x1 if nxt < 0 or nxt >= x1 else nxt + 1
            if cur < x1:
                out.append((base + cur, base + x1, d))
        return out

    # ------------------------------------------------------------ one address
    def map(self, a):
        """(new address, flag) or (None, reason). flag: '' or 'changed-start'."""
        t0, t1 = self.text['addr'], self.text['addr'] + self.text['size']
        if t0 <= a < t1:
            i = bisect.bisect_right(self._ostart_list, a) - 1
            if i >= 0 and a < self.ostart[i] + self.osize[i]:
                d = int(self.delta[i])
                if not self.changed[i]:
                    return a + d, ''
                if a == self.ostart[i]:
                    return a + d, 'changed-start'
                return None, 'inside changed function %#x (+%#x)' % (self.ostart[i], a - self.ostart[i])
            lo = int(self.delta[i]) if i >= 0 else 0
            hi = int(self.delta[i + 1]) if i + 1 < len(self.delta) else lo
            if lo == hi:
                return a + lo, ''
            return None, 'padding between FDEs whose deltas differ (%#x, %#x)' % (lo, hi)
        r0, r1 = self.rod_old['addr'], self.rod_old['addr'] + self.rod_old['size']
        if r0 <= a < r1:
            for s0, s1, d in self.rod_runs:
                if s0 <= a < s1:
                    return a + d, ''
            return None, '.rodata bytes not proven equal at any delta'
        for s0, s1, d, name in self.secs:
            if s0 <= a < s1:
                return a + d, ''
        for s0, s1, name in self.unproven:
            if s0 <= a < s1:
                return None, '%s differs and is not mapped' % name
        # one past the end of a section (a symbol like _end, __bss_start)
        for s0, s1, d, name in self.secs:
            if a == s1:
                return a + d, ''
        return None, 'in no allocated section'

    def moves(self, a):
        """True when a is an image address that the rebase would change."""
        r, _ = self.map(a)
        return r is not None and r != a

    # ------------------------------------------------------------ text rewriting
    def rewrite(self, text, refuse):
        """Every hex token in text that is an image address, carried over. Tokens that
        are not image addresses (offsets, macOS addresses, constants) are left alone; one
        that is an image address from .rodata up but does not map (inside a changed
        function, unproven .rodata) is left alone and reported through refuse."""
        def sub(m):
            a = int(m.group(0), 16)
            if a in KEEP:
                return m.group(0)
            r, why = self.map(a)
            if r is None:
                # from .rodata up a refused token is an address nobody could carry; below
                # it (.dynsym, .rela.dyn ...) it is far likelier a small constant (0xC600)
                if why != 'in no allocated section' and a >= self.rod_old['addr']:
                    refuse(m.group(0), why)
                return m.group(0)
            return '%#x' % r if r != a else m.group(0)
        return HEX.sub(sub, text)

    # ------------------------------------------------------------ calibration
    def symtab_census(self, delta=None, secdelta=None):
        pairs = self._sym_pairs()
        saved = (self.delta, self.secs)
        if delta is not None:
            self.delta = delta
        if secdelta is not None:
            self.secs = secdelta
        agree, disagree, refused, examples = {}, 0, {}, []
        try:
            for v, w, name, sec in pairs:
                r, why = self.map(v)
                if r is None:
                    refused[sec] = refused.get(sec, 0) + 1
                elif r == w:
                    agree[sec] = agree.get(sec, 0) + 1
                else:
                    disagree += 1
                    if len(examples) < 5:
                        examples.append((name, v, r, w))
        finally:
            self.delta, self.secs = saved
        return dict(agree=agree, disagree=disagree, refused=refused, namediff=self._namediff,
                    examples=examples)

    def text_census(self, delta=None, positions=False):
        """Bytes of every FDE run, old against new at the run's delta, with the old
        build's .rela.text sites (4 bytes; 8 for R_X86_64_64) and the changed functions
        masked out: (bytes compared, bytes differing)."""
        delta = self.delta if delta is None else delta
        o, t = self.old, self.text
        n = self.new
        nt = n.sec('.text')
        ov = np.frombuffer(o.m, np.uint8, t['size'], t['off'])
        nv = np.frombuffer(n.m, np.uint8, nt['size'], nt['off'])
        mask = np.zeros(t['size'], bool)
        rel = o.rela('.rela.text')
        off = rel['off'].astype(np.int64) - t['addr']
        wide = (rel['info'] & 0xffffffff) == 1
        for k in range(4):
            mask[off + k] = True
        for k in range(4, 8):
            mask[off[wide] + k] = True
        for i in np.nonzero(self.changed)[0].tolist():
            a = int(self.ostart[i]) - t['addr']
            mask[a:a + int(self.osize[i])] = True
        bounds = [i for i, _, _ in self.runs_of(delta)] + [len(delta)]
        compared = differ = 0
        where = []
        for j in range(len(bounds) - 1):
            i0, i1 = bounds[j], bounds[j + 1]
            a = int(self.ostart[i0]) - t['addr'] if j else 0
            b = int(self.ostart[i1]) - t['addr'] if i1 < len(delta) else t['size']
            d = int(delta[i0])
            lo, hi = max(a, -d), min(b, len(nv) - d)
            keep = ~mask[lo:hi]
            eq = ov[lo:hi][keep] == nv[lo + d:hi + d][keep]
            compared += int(keep.sum())
            differ += int((~eq).sum())
            if positions:
                where.append(np.arange(lo, hi)[keep][~eq] + t['addr'])
        if positions:
            return compared, differ, (np.concatenate(where) if where else np.zeros(0, np.int64))
        return compared, differ

    def runs_of(self, delta):
        out, prev = [], None
        for i, d in enumerate(delta.tolist()):
            if d != prev:
                out.append((i, int(self.ostart[i]), d))
                prev = d
        return out

    def runs(self):
        out, prev = [], None
        for i, d in enumerate(self.delta.tolist()):
            if d != prev:
                out.append((i, int(self.ostart[i]), d))
                prev = d
        return out


def selftest(rb):
    fails = 0
    c = rb.symtab_census()
    nagree = sum(c['agree'].values())
    ok = c['disagree'] == 0 and c['namediff'] == 0 and nagree > 1000
    print('%s symtab: %d agree, %d disagree, %d names differ by index' % (
        'ok  ' if ok else 'FAIL', nagree, c['disagree'], c['namediff']))
    print('     agree by section: %s' % ', '.join('%s %d' % kv for kv in sorted(c['agree'].items())))
    print('     refused by section: %s' % (', '.join('%s %d' % kv for kv in sorted(c['refused'].items())) or 'none'))
    for e in c['examples']:
        print('     %s old %#x mapped %#x new %#x' % e)
    fails += not ok
    # the refusals must be only where nothing is proven: inside changed functions, and
    # .rodata the byte check did not cover -- and few
    ntext = c['refused'].get('.text', 0)
    inside = sum(1 for v, w, n, sec in rb._sym_pairs() if sec == '.text'
                 and rb.map(v)[1].startswith('inside changed'))
    ok = ntext == inside and sum(c['refused'].values()) < nagree // 1000
    print('%s refusals: %d in .text, all inside changed functions: %s; %d in total' % (
        'ok  ' if ok else 'FAIL', ntext, ntext == inside, sum(c['refused'].values())))
    fails += not ok
    cmp_, dif = rb.text_census()
    ok = dif == len(rb.gap_diffs) and cmp_ > rb.text['size'] // 2
    print('%s .text bytes: %d compared outside relocation sites and changed functions, %d differ, '
          'all in padding between FDEs: %s' % ('ok  ' if ok else 'FAIL', cmp_, dif, dif == len(rb.gap_diffs)))
    fails += not ok
    # plant 1: the largest .text run, delta +16
    runs = rb.runs() + [(len(rb.delta), None, None)]
    big = max(range(len(runs) - 1), key=lambda k: runs[k + 1][0] - runs[k][0])
    d = rb.delta.copy()
    d[runs[big][0]:runs[big + 1][0]] += 16
    p = rb.symtab_census(delta=d)
    caught = p['disagree'] > c['disagree']
    print('%s plant: .text run from %#x delta +16: %d disagree (baseline %d)' % (
        'ok  ' if caught else 'FAIL', runs[big][1], p['disagree'], c['disagree']))
    fails += not caught
    # the same plant against the byte census, on the smallest run too (few or no symbols)
    small = min(range(len(runs) - 1), key=lambda k: runs[k + 1][0] - runs[k][0])
    for k in (big, small):
        d = rb.delta.copy()
        d[runs[k][0]:runs[k + 1][0]] += 16
        _, pd = rb.text_census(d)
        caught = pd > dif
        print('%s plant: .text run from %#x (%d FDEs) delta +16: %d bytes differ (baseline %d)' % (
            'ok  ' if caught else 'FAIL', runs[k][1], runs[k + 1][0] - runs[k][0], pd, dif))
        fails += not caught
    # plant 2: .data delta +8
    secs = [(s0, s1, dd + 8 if name == '.data' else dd, name) for s0, s1, dd, name in rb.secs]
    p = rb.symtab_census(secdelta=secs)
    caught = p['disagree'] > c['disagree']
    print('%s plant: .data delta +8: %d disagree (baseline %d)' % (
        'ok  ' if caught else 'FAIL', p['disagree'], c['disagree']))
    fails += not caught
    # plant 3: every .rodata run whose delta is not 0, delta +1: the byte check must reject it
    cands = rb.rodata_candidates()
    for a, b, dd in cands:
        true = sum(y - x for x, y, _ in rb.verify_rodata([(a, b, dd)]))
        if true < 4096:
            continue
        kept = sum(y - x for x, y, _ in rb.verify_rodata([(a, b, dd + 1)]))
        caught = kept < max(64, true // 100)
        print('%s plant: .rodata run %#x delta %+d -> %+d: %d of %d bytes still "proven"' % (
            'ok  ' if caught else 'FAIL', a, dd, dd + 1, kept, true))
        fails += not caught
    # two-sided on the refusal: the interior of a changed function must not map
    ch = np.nonzero(rb.changed)[0]
    if len(ch):
        a = int(rb.ostart[ch[0]]) + 1
        r, why = rb.map(a)
        ok = r is None
        print('%s refusal: %#x (inside changed %#x) -> %s' % (
            'ok  ' if ok else 'FAIL', a, rb.ostart[ch[0]], why if r is None else '%#x' % r))
        fails += not ok
    return 1 if fails else 0


def summary(rb):
    print('old %s\nnew %s' % (rb.old_id, rb.new_id))
    print('FDEs %d, changed %d (%d resized)' % (len(rb.delta), int(rb.changed.sum()), int(rb.resized.sum())))
    for i, a, d in rb.runs():
        print('  run from FDE %6d %#x: %+#x' % (i, a, d))
    for i in np.nonzero(rb.changed)[0].tolist():
        print('  changed %#x %d -> %#x %d%s' % (rb.ostart[i], rb.osize[i], rb.nstart[i], rb.nsize[i],
                                               '' if rb.resized[i] else ' (same size, bytes differ)'))
    g = rb.gap_diffs
    print('padding bytes that differ: %d, at %s' % (len(g), ', '.join('%#x' % x for x in sorted(set((g & ~0xf).tolist())))))
    moved = sorted({(d, name) for s0, s1, d, name in rb.secs if d})
    print('sections moved: %s' % ', '.join('%s %+#x' % (n, d) for d, n in moved))
    print('sections not mapped (bytes differ): %s' % ', '.join(n for _, _, n in rb.unproven))
    rod = rb.rod_old['size']
    cov = sum(b - a for a, b, _ in rb.rod_runs)
    print('.rodata: %d runs proven by bytes, %d of %d bytes (%d refused)' % (
        len(rb.rod_runs), cov, rod, rod - cov))
    for a, b, d in rb.rod_runs:
        print('  .rodata %#x..%#x %+d' % (a, b, d))


def rebase_manifest(rb, src, dst):
    import manifest as mf
    build, head, rows = mf.read_manifest(src)
    if build != rb.old_id:
        sys.exit('%s is for %s, not the old binary %s' % (src, build, rb.old_id))
    problems = []
    for r in rows:
        refuse = lambda tok, why, s=r['symbol']: problems.append('%s: %s %s' % (s, tok, why))
        a, flag = rb.map(r['addr'])
        if a is None:
            # written with the old address, which verify then fails: a person reads it
            problems.append('%s: addr %#x %s' % (r['symbol'], r['addr'], flag))
        elif flag:
            problems.append('%s: addr %#x is the start of a changed function: read it again'
                            % (r['symbol'], r['addr']))
        if a is not None:
            r['addr'] = a
        r['checks'] = rb.rewrite(r['checks'], refuse)
        r['note'] = rb.rewrite(r['note'], refuse)
    newhead = []
    for line in head:
        if line.startswith('# build-id '):
            line = '# build-id %s\n' % rb.new_id
        elif rb.old_id in line or rb.old_id[:8] in line:
            # version, Steam build, snapshot path: not derivable here
            problems.append('header line names the old build, edit it: ' + line.strip())
        newhead.append(line)
    newhead.insert(1, '# Rebased from %s by tools/rebase.py: every address, including those in checks and\n'
                      '# notes, carried over the FDE pairing; the witnesses were read on the old build and\n'
                      '# re-established here by `tools/manifest.py verify`.\n' % rb.old_id)
    mf.write_manifest(dst, newhead, rows)
    for p in problems:
        print('REFUSED ' + p)
    print('%d rows written to %s, %d problems' % (len(rows), dst, len(problems)))
    return 1 if problems else 0


def rebase_hand(rb, path, plen):
    lines = open(path).read().splitlines(keepends=True)
    old_key, new_key = rb.old_id[:plen], rb.new_id[:plen]
    have = [l for l in lines if l.split('\t', 1)[0] == new_key]
    if have:
        sys.exit('%s already has %d rows for %s' % (path, len(have), new_key))
    problems, out = [], []
    for l in lines:
        if l.split('\t', 1)[0] != old_key:
            continue
        refuse = lambda tok, why: problems.append('%s: %s %s' % (l.split('\t')[1], tok, why))
        out.append(new_key + rb.rewrite(l[len(old_key):], refuse))
    for p in problems:
        print('REFUSED ' + p)
    if problems:
        return 1
    with open(path, 'a') as f:
        f.writelines(out)
    print('%d rows appended to %s for %s' % (len(out), path, new_key))
    return 0


def main():
    a = sys.argv[1:]
    if '--keep' in a:
        k = a.index('--keep')
        KEEP.update(int(x, 16) for x in a[k + 1].split(','))
        del a[k:k + 2]
    if len(a) < 3:
        sys.exit(__doc__)
    rb = Rebase(a[0], a[1])
    cmd = a[2]
    if cmd == '--selftest':
        return selftest(rb)
    if cmd == 'summary':
        summary(rb)
        return 0
    if cmd == 'map':
        rc = 0
        for x in a[3:]:
            r, why = rb.map(int(x, 16))
            print('%s -> %s' % (x, '%#x %s' % (r, why) if r is not None else 'REFUSED ' + why))
            rc |= r is None
        return rc
    if cmd == 'manifest' and len(a) == 6 and a[4] == '-o':
        return rebase_manifest(rb, a[3], a[5])
    if cmd == 'hand' and len(a) == 5:
        return rebase_hand(rb, a[3], int(a[4]))
    sys.exit(__doc__)


if __name__ == '__main__':
    sys.exit(main())
