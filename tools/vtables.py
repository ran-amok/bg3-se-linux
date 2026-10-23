#!/usr/bin/env python3
"""Vtables of bin/bg3, read off .rela.data.rel.ro.

Larian's classes have no RTTI and no _ZTV symbols, but --emit-relocs kept a
relocation for every function pointer in .data.rel.ro. A vtable's function
slots are a run of such words; the two words in front of it (offset-to-top and
the null typeinfo pointer) carry no relocation, which is what ends a run. A
run is named through the anchored functions (harvest.py anchors.tsv) in it.

A run is not proof of a vtable: any table of function pointers looks the same,
and a pure-virtual slot (__cxa_pure_virtual is imported, so not in .text)
splits one vtable into two runs. Slot indices are therefore relative to the
run, and only equal vtable indices when the run starts at the address point.

  vtables.py BIN                 every run holding a resolved Larian anchor
  vtables.py BIN --dump ADDR     all slots of the run containing ADDR

Needs harvest.py's output for this build under dumps/harvest/<build-id>/.
"""
import argparse
import csv
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import harvest as H  # noqa: E402


def load(binary):
    h = H.Harvest(binary)
    r = h.elf.rela('.rela.data.rel.ro')
    if r is None:
        sys.exit('no .rela.data.rel.ro')
    tgt = (h.tab['value'][(r['info'] >> 32).astype(np.uint32)].astype(np.int64)
           + r['addend']).astype(np.uint64)
    intext = (tgt >= h.text[0]) & (tgt < h.text[1]) & ((r['info'] & 0xffffffff) == 1)
    slots = dict(zip(r['off'][intext].tolist(), tgt[intext].tolist()))
    d = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
                     'dumps', 'harvest', h.build_id, 'anchors.tsv')
    if not os.path.exists(d):
        sys.exit('run harvest.py first: %s is missing' % d)
    csv.field_size_limit(10 ** 9)
    anchors = {}
    for row in csv.DictReader(open(d), delimiter='\t', quoting=csv.QUOTE_NONE):
        if row['function']:
            anchors[int(row['function'], 16)] = (row['status'], row['demangled'])
    return h, slots, anchors


def run_of(slots, addr):
    lo = hi = addr
    while lo - 8 in slots:
        lo -= 8
    while hi + 8 in slots:
        hi += 8
    return lo, (hi - lo) // 8 + 1


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('binary')
    ap.add_argument('--dump', metavar='ADDR', type=lambda x: int(x, 16))
    a = ap.parse_args()
    h, slots, anchors = load(a.binary)

    if a.dump is not None:
        if a.dump not in slots:
            sys.exit('%#x is not a function-pointer slot' % a.dump)
        lo, n = run_of(slots, a.dump)
        print('run %#x, %d slots' % (lo, n))
        for i in range(n):
            f = slots[lo + 8 * i]
            k = int(h.fde_of([f])[0])
            name = anchors.get(f, ('', h.funcname_at.get(f, '')))
            print('%3d +%#05x %#x %6d  %s %s' % (i, 8 * i, f, h.fd[k, 1] if k >= 0 else -1,
                                                 name[0], name[1][:110]))
        return

    by_target = {}
    for s, f in slots.items():
        by_target.setdefault(f, []).append(s)
    runs = {}
    for f, (status, dem) in anchors.items():
        if H.qualified(dem).split('::')[0] not in H.LARIAN_NS:
            continue
        for s in by_target.get(f, []):
            lo, n = run_of(slots, s)
            runs.setdefault((lo, n), []).append(((s - lo) // 8, status, dem))
    for (lo, n), members in sorted(runs.items()):
        print('run %#x, %d slots' % (lo, n))
        for i, status, dem in sorted(members):
            print('   %3d  %-11s %s' % (i, status, dem[:120]))


if __name__ == '__main__':
    main()
