#!/usr/bin/env python3
"""Annotated disassembly of one function of bin/bg3, for reading witnesses by hand.

  disasm.py BIN ADDR [ADDR...] [--size N] [--match DIR] [--grep RE]

Prints the FDE's instructions (capstone, Intel syntax) with, per rip-relative operand
or direct call: the .symtab name at the target, the C string or relocated {ptr,len}
view there (manifest.Bin.texts_at), and with --match DIR the macOS name match.py gave
the target (matches.tsv / data.tsv). --size overrides the FDE size (for a start that
is not an FDE). This shows evidence; it decides nothing.
"""
import argparse
import csv
import os
import subprocess
import sys

import capstone
from capstone import x86

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import manifest  # noqa: E402


def load_names(match_dir):
    out = {}
    for fn in ('matches.tsv', 'data.tsv'):
        p = os.path.join(match_dir, fn)
        if not os.path.exists(p):
            continue
        with open(p, newline='') as f:
            for r in csv.DictReader(f, delimiter='\t'):
                out[int(r['linux'], 16)] = r.get('mac_names') or r.get('mac_name') or ''
    names = list(set(out.values()))
    dem = subprocess.run(['c++filt'], input='\n'.join(names), capture_output=True, text=True).stdout.split('\n')
    d = dict(zip(names, dem))
    return {a: d.get(n, n) for a, n in out.items()}


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('bin')
    ap.add_argument('addr', nargs='+')
    ap.add_argument('--size', type=int)
    ap.add_argument('--match')
    ap.add_argument('--grep', help='print only lines matching this regex (plus the header)')
    a = ap.parse_args()
    b = manifest.Bin(a.bin)
    sym_of = {}
    for n, addrs in b.sym_at.items():
        for x in addrs:
            sym_of.setdefault(x, n)
    mac = load_names(a.match) if a.match else {}
    import re
    grep = re.compile(a.grep) if a.grep else None

    def note(t):
        parts = []
        if t in sym_of:
            parts.append(sym_of[t])
        if t in mac:
            parts.append('mac:' + mac[t])
        if t in b.fde_start:
            parts.append('fn')
        name, sec = b.section_of(t)
        if sec is not None and not sec['flags'] & manifest.SHF_EXECINSTR:
            for s in sorted(b.texts_at(t))[:2]:
                if s.isprintable():
                    parts.append('"%s"' % s[:80])
        return ' ;; '.join(parts)

    for addr in a.addr:
        start = int(addr, 16)
        size = a.size or b.fde_start.get(start)
        if not size:
            print('%#x: no FDE starts here; give --size' % start)
            continue
        print('==== %#x size %d %s' % (start, size, sym_of.get(start, '')))
        code = b.read(start, size)
        for ins in b.md.disasm(code, start):
            targets = []
            for op in ins.operands:
                if op.type == x86.X86_OP_MEM and op.mem.base == x86.X86_REG_RIP:
                    targets.append(ins.address + ins.size + op.mem.disp)
                elif op.type == x86.X86_OP_IMM and (ins.group(capstone.CS_GRP_JUMP) or ins.group(capstone.CS_GRP_CALL)):
                    targets.append(op.imm)
            ann = '   '.join('[%#x] %s' % (t, note(t)) for t in targets
                             if not (ins.group(capstone.CS_GRP_JUMP) and start <= t < start + size))
            line = '%8x:  %-7s %-40s %s' % (ins.address, ins.mnemonic, ins.op_str, ann)
            if grep is None or grep.search(line):
                print(line.rstrip())


if __name__ == '__main__':
    main()
