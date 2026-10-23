#!/usr/bin/env python3
"""Resolve upstream's BinaryMappings.xml targets in the Linux binary.

  targets.py XML LINUX_FEATURES MAC_FEATURES MATCH_DIR     write dumps/targets/<ids>/
  targets.py XML LINUX_FEATURES MAC_FEATURES --selftest    calibrate the referrer check

XML is upstream's BG3Extender/GameHooks/BinaryMappings.xml; the two feature
directories are features.py's, MATCH_DIR is match.py's. tools/targets.tsv says which
macOS symbol each upstream symbol is — that part is a person's reading, not a search.

Per target row the macOS symbol is looked up in match.py's output, and then the
Linux address is tested from the outside:

  referrers  every macOS function that refers to the macOS node and has a matched
             Linux counterpart: does the counterpart refer to the Linux node?
             agree/asked, with the totals on both sides. Printed, never counted:
             callers vote in the matcher, and --selftest shows the figure does not
             tell its right answers from its wrong ones (see below).
  string     upstream's own <Condition Type="String">: the Linux functions that use
             that string, and whether one of them refers to the Linux node (or, for
             an Offset=0 target, is it). Independent of the macOS binary altogether.
             Larian code mostly holds a string as a {pointer, length} constant in
             .data.rel.ro, so a function "uses" a string when it refers to the text
             or to a relocated pointer to it. The witness says how many other things
             of the target's kind that function refers to: upstream's pattern pins a
             position in it, this check only the function.
  anchor     harvest.py names the Linux function from a PRETTY_FUNCTION local.
  symtab     the Linux .symtab itself carries the name.

  hand       tools/targets-hand.tsv: an address somebody established by reading the
             disassembly, with what they saw. It is the verdict where present; where
             the matcher has another address, the row's note says so.

Where the macOS node is unmatched, the same referrers are asked whom they refer to
instead, and the tally is printed as candidates: a lead for hand work, not a result.

--selftest: a referrer figure means nothing until it has been seen on pairs known to
be right and known to be wrong. The matcher is rerun with all of Noesis withheld
(match.py's honest calibration), and the figure is taken for every withheld pair it
got right and every one it got wrong. Measured on c8da62b1: >= 3 agreeing at >= 0.9
passes 1,551 of 11,177 right pairs and 4 of 86 wrong ones — a likelihood ratio near 3,
which is no witness. (The wrong ones are mostly "contains the callee's body", and
their callers agree by construction.) For data, 817 of 818 withheld globals came back
right, so there is nothing to tell apart; the figure stays informational there too.
"""
import argparse
import collections
import csv
import json
import os
import re
import sys
import zlib
import xml.etree.ElementTree as ET

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import match as mt                                                   # noqa: E402

HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def upstream_targets(path):
    """[(mapping, symbol, target type, offset, [condition strings])], DllImports too."""
    out = []
    for m in ET.parse(path).getroot().find('Mappings'):
        if m.tag == 'DllImport':
            out.append((m.get('Proc'), m.get('Symbol'), 'DllImport', '', []))
            continue
        conds = [c.get('Value') for c in m.findall('Condition') if c.get('Type') == 'String']
        for t in m.findall('Target'):
            if t.get('Symbol'):
                out.append((m.get('Name'), t.get('Symbol'), t.get('Type'), t.get('Offset'), conds))
    return out


def table(path):
    rows = collections.defaultdict(list)
    with open(path, newline='') as f:
        lines = [x for x in f if not x.startswith('#')]
    for r in csv.DictReader(lines, delimiter='\t', quoting=csv.QUOTE_NONE):
        rows[r['upstream']].append(r)
    return rows


def referrers(L, M, ml, m, l):
    """(agree, asked, macOS referrers, Linux referrers) for the pair m -> l."""
    agree = asked = 0
    for mf in M.callers.get(m, ()):
        lf = ml.get(mf)
        if lf is not None and lf < L.n:
            asked += 1
            agree += l in L.out[lf]
    return agree, asked, len(M.callers.get(m, ())), len(L.callers.get(l, ()))


def candidates(L, M, ml, lm, m):
    """Unmatched Linux nodes of m's kind that the counterparts of m's referrers refer
    to, by how many of them do."""
    tally = collections.Counter()
    asked = 0
    for mf in M.callers.get(m, ()):
        lf = ml.get(mf)
        if lf is None or lf >= L.n:
            continue
        asked += 1
        for x in L.out[lf]:
            if x not in lm and (x < L.n) == (m < M.n):
                tally[x] += 1
    return asked, tally.most_common(4)


def load_matches(d, L, M):
    ml, why = {}, {}
    for name in ('matches.tsv', 'data.tsv'):
        with open(os.path.join(d, name), newline='') as f:
            for r in csv.DictReader(f, delimiter='\t', quoting=csv.QUOTE_NONE):
                l, m = L.node[int(r['linux'], 16)], M.node[int(r['mac'], 16)]
                ml[m], why[m] = l, r['evidence']
    return ml, why


def string_holders(path, texts):
    """text -> the addresses whose bytes are that C string, and the data words that a
    RELATIVE relocation points at one of them (ELF only)."""
    import mmap
    import struct
    import numpy as np
    out = {}
    with open(path, 'rb') as f:
        d = mmap.mmap(f.fileno(), 0, access=mmap.ACCESS_READ)
    shoff, = struct.unpack_from('<Q', d, 0x28)
    shentsize, shnum, shstrndx = struct.unpack_from('<HHH', d, 0x3a)
    secs = [struct.unpack_from('<IIQQQQIIQQ', d, shoff + i * shentsize) for i in range(shnum)]
    stroff = secs[shstrndx][4]
    name = lambda s: d[stroff + s[0]:d.find(b'\0', stroff + s[0])].decode()
    rela = next(s for s in secs if name(s) == '.rela.dyn')
    r = np.frombuffer(d, dtype='<u8', count=rela[5] // 8, offset=rela[4]).reshape(-1, 3)
    r = r[(r[:, 1] & 0xffffffff) == 8]                       # R_X86_64_RELATIVE
    alloc = [s for s in secs if s[3] and s[1] == 1]           # PROGBITS with an address
    for t in texts:
        needle, at, holders = b'\0' + t.encode() + b'\0', [], []
        k = d.find(needle)
        while k >= 0:
            for s in alloc:
                if s[4] <= k + 1 < s[4] + s[5]:
                    at.append(s[3] + k + 1 - s[4])
            k = d.find(needle, k + 1)
        for x in at:
            holders += r[r[:, 2] == x][:, 0].tolist()
        out[t] = (at, holders)
    out[None] = [(x[3], x[3] + x[5]) for x in secs if x[3] and x[2] & 1]      # SHF_WRITE
    return out


def hand_table(path, build):
    out = {}
    if os.path.exists(path):
        with open(path, newline='') as f:
            lines = [x for x in f if not x.startswith('#')]
        for r in csv.DictReader(lines, delimiter='\t', quoting=csv.QUOTE_NONE):
            if build.startswith(r['build']):
                out[r['upstream']] = r
    return out


def anchors(harvest_dir, build):
    out = {}
    path = os.path.join(harvest_dir, build, 'anchors.tsv')
    if os.path.exists(path):
        with open(path, newline='') as f:
            for r in csv.DictReader(f, delimiter='\t', quoting=csv.QUOTE_NONE):
                if r['status'] == 'single' and 'PRETTY_FUNCTION' in r['kinds']:
                    out[int(r['function'], 16)] = mt.norm(r['mangled'])
    return out


def verdict(row):
    """located = an address, and something other than the matcher says so."""
    if row['witness'].startswith(('inlined', 'absent')):
        return row['witness'].split(' ')[0].rstrip(':').lower()
    if row['linux'] == '':
        return 'open'
    if row['witness']:
        return 'located'
    return 'matched'


def selftest(L, M, a):
    truth, dtruth, _ = mt.name_pairs(L, M)
    rx = re.compile('Noesis')
    held = {l: v for l, v in truth.items() if rx.search(v[1])}
    dheld = {l: v for l, v in dtruth.items() if zlib.crc32(v[1].encode()) % 5 == 0}
    m = mt.Matcher(L, M, 2, 0.8, 1.25)
    for tr in (truth, dtruth):
        for l, (mm, _) in tr.items():
            if l not in held and l not in dheld:
                m.add(l, mm, 'name')
    m.run(lambda *x: print(*x, file=sys.stderr, flush=True))
    ml = {v: k for k, v in m.lm.items()}
    out = {}
    for title, asked_for in (('functions: all of Noesis withheld', held),
                             ('data: a hashed fifth of the named globals withheld', dheld)):
        cells = collections.defaultdict(lambda: [0, 0])
        for l, (want, _) in asked_for.items():
            got = m.lm.get(l)
            if got is None:
                continue
            ok = got == want or mt._same_entity(M.names[want][0], M.names.get(got, ['?'])[0])
            agree, asked, _, _ = referrers(L, M, ml, got, l)
            ratio = agree / asked if asked else 0
            band = ('0' if agree == 0 else '1-2' if agree < 3 else '3-9' if agree < 10 else '10+',
                    '<0.5' if ratio < 0.5 else '<0.9' if ratio < 0.9 else '>=0.9')
            cells[band][0 if ok else 1] += 1
        print('%s\n  agreeing referrers, agree/asked: right / wrong' % title)
        for band in sorted(cells):
            print('  %-5s %-6s %6d / %d' % (band + tuple(cells[band])))
        out[title] = cells
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('xml')
    ap.add_argument('linux')
    ap.add_argument('mac')
    ap.add_argument('match', nargs='?')
    ap.add_argument('--table', default=os.path.join(HERE, 'tools', 'targets.tsv'))
    ap.add_argument('--harvest', default=os.path.join(HERE, 'dumps', 'harvest'))
    ap.add_argument('--out', default=os.path.join(HERE, 'dumps', 'targets'))
    ap.add_argument('--hand', default=os.path.join(HERE, 'tools', 'targets-hand.tsv'))
    ap.add_argument('--selftest', action='store_true')
    a = ap.parse_args()

    vocab = {}
    L, M = mt.Side(a.linux, vocab), mt.Side(a.mac, vocab)
    if a.selftest:
        mt.prune(L, M)
    L.index(), M.index()
    if a.selftest:
        selftest(L, M, a)
        return
    ml, why = load_matches(a.match, L, M)
    lm = {v: k for k, v in ml.items()}
    inv = collections.defaultdict(list)
    for i, ns in M.names.items():
        for x in ns:
            inv[x].append(i)
    anch = anchors(a.harvest, L.summary['id'])
    tab = table(a.table)
    hand = hand_table(a.hand, L.summary['id'])
    ups = upstream_targets(a.xml)
    held = string_holders(L.summary['binary'], sorted({s for u in ups for s in u[4]}))

    def users(text):
        at, holders = held[text]
        tok = vocab.get('s:' + json.dumps(text, ensure_ascii=False))
        fns = set(L.token_fns.get(tok, ())) if tok is not None else set()
        for x in at + holders:
            if x in L.node:
                fns |= L.callers.get(L.node[x], set())
        return fns

    rows = []
    for mapping, sym, ttype, off, conds in ups:
        if sym not in tab:
            rows.append(dict(upstream=sym, mapping=mapping, kind='', mac='', linux='', evidence='',
                             referrers='', witness='', note='NOT IN targets.tsv'))
            continue
        for t in tab[sym]:
            row = dict(upstream=sym, mapping=mapping, kind=t['kind'], mac=t['mac'], linux='',
                       evidence='', referrers='', witness='', note=t['note'] or '')
            rows.append(row)
            if t['mac'] == '-':
                continue
            ms = [i for i in inv.get(mt.norm(t['mac']), ()) if (i < M.n) == (t['kind'] == 'fn')]
            if len(ms) != 1:
                row['note'] = 'macOS symbol is %d nodes (unreferenced data is not a node). %s' % (len(ms), row['note'])
                continue
            m = ms[0]
            l = ml.get(m)
            if l is None:
                asked, cand = candidates(L, M, ml, lm, m)
                row['evidence'] = 'unmatched'
                row['referrers'] = 'candidates of %d matched referrers (mac has %d): %s' % (
                    asked, len(M.callers.get(m, ())), ' '.join('%#x:%d' % (L.addr[x], n) for x, n in cand))
                continue
            row['linux'], row['evidence'] = '%#x' % L.addr[l], why[m]
            agree, asked, nm, nl = referrers(L, M, ml, m, l)
            row['referrers'] = '%d/%d (mac %d, linux %d)' % (agree, asked, nm, nl)
            w = []
            for s in conds:
                for f in sorted(users(s)):
                    if f == l:
                        w.append('string %s: used by it' % json.dumps(s))
                    elif l in L.out[f]:
                        # how many things of l's kind that function refers to: 1 of 1 pins it,
                        # 1 of 30 says only that the neighbourhood is right
                        n = sum(1 for x in L.out[f] if (x < L.n) == (l < L.n) and
                                (x < L.n or any(lo <= L.addr[x] < hi for lo, hi in held[None])))
                        w.append('string %s: %#x refers to it, 1 of %d %s there' % (
                            json.dumps(s), L.addr[f], n, 'callees' if l < L.n else 'writable globals'))
            if l < L.n and anch.get(L.addr[l]) == mt.norm(t['mac']):
                w.append('anchor')
            if mt.norm(t['mac']) in L.names.get(l, ()):
                w.append('symtab')
            row['witness'] = '; '.join(w)

    seen = set()
    for r in list(rows):
        h = hand.get(r['upstream'])
        if h is None:
            continue
        if h['status'] != 'at':
            if r['upstream'] not in seen:
                rows.append(dict(r, kind=r['kind'], mac='', linux=h['linux'], evidence='hand', referrers='',
                                 witness='%s: %s' % (h['status'], h['witness']), note=''))
            seen.add(r['upstream'])
            continue
        if r['linux'] == h['linux']:
            r['witness'] = '; '.join(x for x in (r['witness'], 'hand: ' + h['witness']) if x)
            seen.add(r['upstream'])
    for sym, h in hand.items():
        if h['status'] == 'at' and sym not in seen:
            auto = sorted({r['linux'] for r in rows if r['upstream'] == sym and r['linux']})
            mapping = next((r['mapping'] for r in rows if r['upstream'] == sym), '')
            rows.append(dict(upstream=sym, mapping=mapping, kind='', mac='', linux=h['linux'], evidence='hand',
                             referrers='', note='the matcher has %s' % (', '.join(auto) or 'nothing'),
                             witness='hand: ' + h['witness']))
    for r in rows:
        r['verdict'] = verdict(r)
    out = os.path.join(a.out, '%s_%s' % (L.summary['id'][:12], M.summary['id'][:12]))
    os.makedirs(out, exist_ok=True)
    cols = ['upstream', 'kind', 'verdict', 'linux', 'evidence', 'referrers', 'witness', 'mac', 'mapping', 'note']
    with open(os.path.join(out, 'targets.tsv'), 'w') as f:
        f.write('\t'.join(cols) + '\n')
        for r in rows:
            f.write('\t'.join(r[c] for c in cols) + '\n')

    # a symbol's standing is its best row's
    rank = {'located': 0, 'inlined': 1, 'absent': 2, 'matched': 3, 'open': 4}
    best = {}
    for r in rows:
        if r['upstream'] not in best or rank[r['verdict']] < rank[best[r['upstream']]]:
            best[r['upstream']] = r['verdict']
    count = collections.Counter(best.values())
    summary = dict(symbols=len(best), rows=len(rows), **count)
    with open(os.path.join(out, 'summary.json'), 'w') as f:
        json.dump(summary, f, indent=1)
    for r in rows:
        print('%-8s %-52s %-10s %-10s %-24s %s' % (r['verdict'], r['upstream'][:52], r['linux'],
                                                   r['evidence'], r['referrers'][:24], r['witness']))
    print(json.dumps(summary), file=sys.stderr)


if __name__ == '__main__':
    main()
