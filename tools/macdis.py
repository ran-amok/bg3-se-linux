#!/usr/bin/env python3
"""Annotated disassembly of one function of the macOS oracle, for reading witnesses by hand.

The macOS build keeps the ctor/dtor/method symbols LTO inlined away in bin/bg3, so a class's
layout or a message's fill is often readable only here (the rule: when a mirror is wrong and
bin/bg3 has no symbol for the class, read the macOS oracle's constructor). GNU `objdump` cannot
open a Mach-O; this is capstone over features.py's
MachO loader, with every call target and RIP-relative operand named by the symbol table
(demangled) or, for __TEXT,__cstring, by the string it points at.

    tools/macdis.py ORACLE SYM|ADDR...     one function per argument (SYM is a mangled name,
                                           ADDR hex); --size caps the sweep, --grep RE filters
    tools/macdis.py ORACLE --find RE       list symbols whose demangled name matches RE
    tools/macdis.py ORACLE --selftest      two-sided: a known symbol resolves to its address
                                           and names its callee, a bogus one is refused

ORACLE is `dumps/oracles/bg3-macos-x86_64-af3f1c71` for build a187af94.
"""
import argparse
import bisect
import os
import re
import subprocess
import sys

import capstone as cs

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import features  # noqa: E402


class Oracle:
    def __init__(self, path):
        self.mo = features.MachO(path)
        syms, name_of = self.mo.symbols()
        self.by_name = {}
        self.addrs, self.names = [], []
        for a, sect, n in sorted(syms):
            self.by_name.setdefault(n, a)
            self.addrs.append(a)
            self.names.append(n)
        self.stubs = self.mo.stubs(name_of)
        self.text = self.mo.sec('__TEXT,__text')
        self.cstr = self.mo.sec('__TEXT,__cstring')
        self._dem = {}

    def demangle(self, names):
        todo = [n for n in names if n not in self._dem]
        if todo:
            out = subprocess.run(['c++filt'], input='\n'.join(features.unmac(n) for n in todo),
                                 capture_output=True, text=True).stdout.split('\n')
            for n, d in zip(todo, out):
                self._dem[n] = d
        return [self._dem[n] for n in names]

    def sym_at(self, a):
        i = bisect.bisect_right(self.addrs, a) - 1
        if i < 0:
            return None
        base = self.addrs[i]
        n = self.demangle([self.names[i]])[0]
        return n if base == a else '%s+0x%x' % (n, a - base)

    def func_end(self, a):
        i = bisect.bisect_right(self.addrs, a)
        return self.addrs[i] if i < len(self.addrs) else self.text['addr'] + self.text['size']

    def read(self, a, n):
        s = self.text
        off = s['off'] + (a - s['addr'])
        return bytes(self.mo.m[off:off + n])

    def cstring(self, a):
        s = self.cstr
        if not s or not (s['addr'] <= a < s['addr'] + s['size']):
            return None
        off = s['off'] + (a - s['addr'])
        end = self.mo.m.find(b'\0', off, s['off'] + s['size'])
        return self.mo.m[off:end].decode('utf-8', 'replace')

    def resolve(self, a):
        if a in self.stubs:
            return 'stub:' + self.demangle([self.stubs[a]])[0]
        c = self.cstring(a)
        if c is not None:
            return repr(c)
        return self.sym_at(a)

    def disasm(self, start, size=None, out=sys.stdout, grep=None):
        end = start + size if size else self.func_end(start)
        md = cs.Cs(cs.CS_ARCH_X86, cs.CS_MODE_64)
        md.detail = True
        print('; %s  0x%x..0x%x (%d bytes)' % (self.sym_at(start), start, end, end - start), file=out)
        rx = re.compile(grep) if grep else None
        for ins in md.disasm(self.read(start, end - start), start):
            note = ''
            if ins.mnemonic in ('call', 'jmp') or ins.mnemonic.startswith('j'):
                try:
                    tgt = int(ins.op_str, 16)
                    if start <= tgt < end:
                        note = '+0x%x' % (tgt - start)          # a local label
                    else:
                        note = self.resolve(tgt) or ''
                except ValueError:
                    pass
            if not note:
                for op in ins.operands:
                    if op.type == cs.x86.X86_OP_MEM and op.mem.base == cs.x86.X86_REG_RIP:
                        note = self.resolve(ins.address + ins.size + op.mem.disp) or ''
                    elif op.type == cs.x86.X86_OP_IMM and self.text['addr'] <= op.imm < 0x200000000:
                        r = self.resolve(op.imm)
                        if r:
                            note = r
            line = '%x  %-8s %-40s %s' % (ins.address, ins.mnemonic, ins.op_str,
                                          ('; ' + note) if note else '')
            if rx is None or rx.search(line):
                print(line, file=out)

    def lookup(self, s):
        if s in self.by_name:
            return self.by_name[s]
        if '_' + s in self.by_name:
            return self.by_name['_' + s]
        try:
            return int(s, 16)
        except ValueError:
            sys.exit('no such symbol: %s' % s)


def selftest(o):
    a = o.lookup('__ZN3esv10GameServer8KickPeerEN2ls8TypeWrapIiN3net15PeerIDClassnameELb1EEE')
    assert o.sym_at(a).startswith('esv::GameServer::KickPeer('), o.sym_at(a)
    import io
    buf = io.StringIO()
    o.disasm(a, out=buf)
    text = buf.getvalue()
    assert 'call' in text and '; ' in text, 'no annotated call in KickPeer'
    assert re.search(r'call .*; (stub:)?[A-Za-z_:<>~]', text), 'no named callee'
    try:
        o.lookup('__ZN3esv10GameServer8NoSuchFnEv')
    except SystemExit:
        pass
    else:
        raise AssertionError('bogus symbol was not refused')
    print('selftest ok: KickPeer resolves at 0x%x and names a callee; bogus symbol refused' % a)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('oracle')
    ap.add_argument('what', nargs='*')
    ap.add_argument('--size', type=lambda x: int(x, 0))
    ap.add_argument('--find')
    ap.add_argument('--selftest', action='store_true')
    ap.add_argument('--grep', help='print only lines matching this regex (plus the header)')
    args = ap.parse_args()
    o = Oracle(args.oracle)
    if args.selftest:
        return selftest(o)
    if args.find:
        rx = re.compile(args.find)
        dem = o.demangle(o.names)
        for a, n, d in zip(o.addrs, o.names, dem):
            if rx.search(d):
                print('%x  %s' % (a, d))
        return
    if not args.what:
        ap.error('nothing to disassemble')
    for w in args.what:
        o.disasm(o.lookup(w), args.size, grep=args.grep)
        print()


if __name__ == '__main__':
    main()
