#!/usr/bin/env python3
"""The hooking engine's x86-64 decoder (loader/src/hook.cpp) against capstone, on
bin/bg3 itself.

  hook-decode-check.py BIN [DECODE_DUMP]      calibration + hookability census
  hook-decode-check.py BIN --selftest         the checker's own calibration: every
                                              997th decoded instruction is falsified
                                              (length, disp position, or kind, in turn)
                                              and each must come back as a disagreement

BIN is an ELF whose features.py output exists (dumps/features/<build-id>/, for the
function table); DECODE_DUMP defaults to build/loader/decode-dump.

1. Calibration. Every function's first 32 bytes, and every 25th function whole, are
   decoded by both. Per instruction: length, whether it has a rip-relative operand
   and where its disp32 sits, and for direct branches the target. A disagreement is
   a decoder bug; a decoder refusal where capstone decodes is a coverage gap, which
   is safe (install() refuses) but costs hook points. Exit 1 on any disagreement.
2. Census. hook::probe on every function, whole body: how many functions install()
   would take, and why the rest are refused. A target in the offset manifest should
   probe `ok`; this is the check the audit repeats per build.
"""
import argparse
import collections
import os
import struct
import subprocess
import sys

import capstone
from capstone import x86
import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import harvest  # noqa: E402

HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
KINDS = ['other', 'jmp8', 'jmp32', 'jcc8', 'jcc32', 'call32', 'loop8', 'ret', 'stop']
REL = {'jmp8', 'jmp32', 'jcc8', 'jcc32', 'call32', 'loop8'}


def records(funcs, text, ta, head):
    out = bytearray()
    for s, n in funcs:
        n = min(n, head) if head else n
        b = text[s - ta:s - ta + n]
        out += struct.pack('<QI', s, len(b)) + b
    return bytes(out)


def run(tool, data, *args):
    p = subprocess.run([tool, *args], input=data, stdout=subprocess.PIPE, check=True)
    return p.stdout.decode().splitlines()


def capstone_view(md, code, addr):
    """address -> (length, disp_off or -1, rel target or None, is_ret), linearly"""
    out = {}
    for ins in md.disasm(code, addr):
        disp = -1
        target = None
        for op in ins.operands:
            if op.type == x86.X86_OP_MEM and op.mem.base == x86.X86_REG_RIP:
                disp = ins.disp_offset
        if (ins.group(capstone.CS_GRP_JUMP) or ins.group(capstone.CS_GRP_CALL)) and \
                len(ins.operands) == 1 and ins.operands[0].type == x86.X86_OP_IMM:
            target = ins.operands[0].imm
        out[ins.address] = (ins.size, disp, target, ins.group(capstone.CS_GRP_RET))
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('bin')
    ap.add_argument('tool', nargs='?', default=os.path.join(HERE, 'build', 'loader', 'decode-dump'))
    ap.add_argument('--every', type=int, default=25, help='decode every Nth function whole')
    ap.add_argument('--selftest', action='store_true')
    a = ap.parse_args()

    e = harvest.Elf(a.bin)
    bid = e.build_id()
    z = np.load(os.path.join(HERE, 'dumps', 'features', bid, 'features.npz'))
    t = e.sec('.text')
    ta, to, ts = t['addr'], t['off'], t['size']
    text = e.m[to:to + ts]
    funcs = [(s, n) for s, n in zip(z['fstart'].tolist(), z['fsize'].tolist())
             if ta <= s and s + n <= ta + ts and n > 0]
    print('build-id %s: %d functions in .text' % (bid, len(funcs)))

    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
    md.detail = True

    # 1. calibration
    sets = [('prologues (first 32 bytes)', funcs, 32),
            ('whole bodies (every %dth)' % a.every, funcs[::a.every], 0)]
    bad = planted = 0
    for title, fs, head in sets:
        lines = run(a.tool, records(fs, text, ta, head))
        cs = {}
        for s, n in fs:
            n = min(n, head) if head else n
            cs.update(capstone_view(md, bytes(text[s - ta:s - ta + n]), s))
        c = collections.Counter()
        gaps = collections.Counter()
        shown = 0
        for i, ln in enumerate(lines):
            f = ln.split('\t')
            addr = int(f[0], 16)
            ref = cs.get(addr)
            if f[1] == 'FAIL':
                if ref:
                    c['decoder refuses, capstone decodes'] += 1
                    ins = next(md.disasm(bytes(text[addr - ta:addr - ta + 16]), addr), None)
                    gaps[ins.mnemonic if ins else '?'] += 1
                else:
                    c['both refuse'] += 1
                continue
            n, kind, disp, target = int(f[1]), KINDS[int(f[2])], int(f[3]), int(f[4], 16)
            if a.selftest and i % 997 == 0:
                planted += 1
                m = (i // 997) % 3
                if m == 0:
                    n += 1
                elif m == 1:
                    disp = disp + 1 if disp >= 0 else 2
                else:
                    kind, target = ('other', 0) if kind in REL else ('jmp32', addr + n)
            if ref is None:
                c['capstone refuses, decoder decodes'] += 1
                bad += 1
                continue
            why = []
            if n != ref[0]:
                why.append('length %d vs %d' % (n, ref[0]))
            elif disp != ref[1]:
                why.append('rip disp at %d vs %d' % (disp, ref[1]))
            elif (kind in REL) != (ref[2] is not None):
                why.append('kind %s vs target %s' % (kind, ref[2]))
            elif kind in REL and target != ref[2]:
                why.append('target %#x vs %#x' % (target, ref[2]))
            elif (kind == 'ret') != bool(ref[3]):
                why.append('ret %s vs %s' % (kind, ref[3]))
            if why:
                c['disagree'] += 1
                bad += 1
                if shown < 15 and not a.selftest:
                    shown += 1
                    ins = next(md.disasm(bytes(text[addr - ta:addr - ta + 16]), addr))
                    print('  DISAGREE %#x %s %s: %s' % (addr, ins.mnemonic, ins.op_str, '; '.join(why)))
            else:
                c['agree'] += 1
        print('%s: %s' % (title, dict(c)))
        if gaps:
            print('  coverage gaps by mnemonic:', dict(gaps.most_common(12)))

    if a.selftest:
        ok = bad == planted
        print('selftest: %d planted, %d disagreements found: %s' % (planted, bad, 'ok' if ok else 'FAIL'))
        sys.exit(0 if ok else 1)

    # 2. census
    st = collections.Counter(ln.split('\t')[1] for ln in run(a.tool, records(funcs, text, ta, 0), '--probe'))
    print('probe, %d functions whole:' % len(funcs))
    for k, v in st.most_common():
        print('  %7d  %5.1f%%  %s' % (v, 100 * v / len(funcs), k))
    sys.exit(1 if bad else 0)


if __name__ == '__main__':
    main()
