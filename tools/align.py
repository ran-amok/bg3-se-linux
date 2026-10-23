#!/usr/bin/env python3
"""Hand-work view for locating targets: one macOS function beside one Linux function.

  align.py LINUX_FEATURES MAC_FEATURES MATCH_DIR MAC [LINUX]

MAC is a macOS symbol (Itanium, or a regex with --re) or 0xADDRESS; LINUX is a Linux
function address, by default the one MAC was matched to. Prints the two event
sequences (calls, data references, strings, imports) aligned on what match.py already
paired, so an unmatched callee shows up as a gap with a Linux address opposite it.
Also lists who refers to MAC on the macOS side and what they were matched to.
This shows evidence; it decides nothing.
"""
import argparse
import difflib
import os
import re
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import match as mt                                                   # noqa: E402
from targets import load_matches                                     # noqa: E402


def demangle(names):
    out = subprocess.run(['c++filt'], input='\n'.join(names), capture_output=True, text=True).stdout
    return dict(zip(names, out.split('\n')))


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('linux')
    ap.add_argument('mac')
    ap.add_argument('match')
    ap.add_argument('sym')
    ap.add_argument('laddr', nargs='?')
    ap.add_argument('--re', action='store_true', help='SYM is a regex over mangled names')
    ap.add_argument('--referrers', type=int, default=12, help='how many referrers to list')
    a = ap.parse_args()

    vocab = {}
    L, M = mt.Side(a.linux, vocab), mt.Side(a.mac, vocab)
    L.index(), M.index()
    words = {v: k for k, v in vocab.items()}
    ml, why = load_matches(a.match, L, M)
    lm = {v: k for k, v in ml.items()}

    if a.sym.startswith('0x'):
        ms = [M.node[int(a.sym, 16)]]
    else:
        rx = re.compile(a.sym if a.re else re.escape(mt.norm(a.sym)) + '$')
        ms = sorted({i for i, ns in M.names.items() for x in ns if rx.match(x)})
    if len(ms) != 1:
        for i in ms[:40]:
            print('%#x %s' % (M.addr[i], M.names[i][0][:160]))
        sys.exit('%d macOS nodes match' % len(ms))
    m = ms[0]
    l = L.node[int(a.laddr, 16)] if a.laddr else ml.get(m)

    def mname(i):
        return M.names.get(i, ['%#x' % M.addr[i]])[0]

    def lkey(e):
        if e >= 0:
            return ('t', e)
        x = -e - 1
        return ('n', lm[x]) if x in lm else ('L', x)

    def mkey(e):
        if e >= 0:
            return ('t', e)
        x = -e - 1
        return ('n', x) if x in ml else ('M', x)

    names = set()
    rows = []
    print('macOS %#x %s' % (M.addr[m], mname(m)[:200]))
    print('linux %s (%s)' % ('%#x' % L.addr[l] if l is not None else '-', why.get(m, 'unmatched')))
    refs = sorted(M.callers.get(m, ()))
    print('\nreferrers on macOS: %d, matched %d' % (len(refs), sum(1 for f in refs if f in ml)))
    for f in refs[:a.referrers]:
        lf = ml.get(f)
        hit = '' if lf is None or l is None else ('refers' if l in L.out[lf] else 'does NOT refer')
        rows.append(('  %#x' % M.addr[f], mname(f), '%#x %s %s' % (L.addr[lf], why[f], hit) if lf is not None else '-'))
        names.add(mname(f))
    if m < M.n and (l is None or l < L.n):
        me, le = M.events[m], L.events[l] if l is not None else []
        sm = difflib.SequenceMatcher(None, [mkey(e) for e in me], [lkey(e) for e in le], autojunk=False)
        rows.append(None)
        for tag, i1, i2, j1, j2 in sm.get_opcodes():
            for k in range(max(i2 - i1, j2 - j1)):
                x = me[i1 + k] if i1 + k < i2 else None
                y = le[j1 + k] if j1 + k < j2 else None
                left = '' if x is None else words[x] if x >= 0 else mname(-x - 1)
                right = '' if y is None else words[y] if y >= 0 else '%#x%s' % (
                    L.addr[-y - 1], ' = ' + mname(lm[-y - 1]) if -y - 1 in lm else '')
                rows.append(('=' if tag == 'equal' else '~', left, right))
                names.update(w for w in (left, right.split(' = ')[-1]) if w.startswith('_Z'))
    dm = demangle(sorted(names)) if names else {}
    short = lambda s: dm.get(s, s)[:88]
    for r in rows:
        if r is None:
            print('\nevents, macOS | linux:')
            continue
        right = r[2]
        if ' = ' in right:
            head, tail = right.split(' = ', 1)
            right = head + ' = ' + short(tail)
        print('%-12s %-90s | %s' % (r[0], short(r[1]), right[:110]))


if __name__ == '__main__':
    main()
