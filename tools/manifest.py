#!/usr/bin/env python3
"""The offset manifest: what the loader may touch in bin/bg3, per build.

  manifest.py verify [BIN]            the audit: every row of BIN's manifest re-checked
                                      against BIN (default: the installed game)
  manifest.py verify [BIN] --selftest the audit's calibration: planted errors must fail
  manifest.py fill MANIFEST BIN       (re)compute the size and bytes columns from BIN
  manifest.py seed TARGETS_TSV        rows for targets.py's located verdicts, to start a
                                      manifest for a new build (checks to be written)

manifest/<build-id>.tsv, one file per ELF build-id, is embedded into libbg3se.so at
build time; the loader stays passive on a build that has none. A row is one address:

  symbol   upstream's name where upstream has one (BinaryMappings.xml / StaticSymbols)
  kind     fn | data | rodata
  addr     ELF virtual address (offset from the PIE load base)
  size     fn: the FDE's range. data: -. rodata: the object's length (a C string:
           through its NUL)
  bytes    fn: the first 16 bytes (fewer if the function is shorter), which the loader
           compares in memory before it trusts the row. data: -. rodata: all of them --
           the content is the witness, and the loader compares it too
  checks   what the audit re-establishes from the binary, space-separated kind:arg
           (arg in "..." with JSON escapes where it has spaces):
             symtab:NAME   .symtab has NAME at addr
             str:TEXT      the function refers to the string TEXT, directly or through
                           a relocated {ptr, len} constant
             in:WHO        the function WHO (address or row symbol) refers to addr
             refs:WHAT     the function refers to WHAT (address or row symbol)
             in-str:TEXT   some function that refers to TEXT refers to addr
             callers:N     at least N direct call sites
             jt:FN:TABLE:I entry I of the rel32 jump table TABLE, which FN refers to,
                           leads to a block of FN whose first call is to addr (a
                           switch case: the position, where in:FN is only membership)
             nth:FN:K      addr is the K-th (from 0) distinct .data/.bss global that FN
                           refers to, in code order: a position in a run of globals
             text-re:RE    the C string at addr matches RE in full (rodata rows): what
                           the reader of the row relies on, so a neighbouring literal
                           with size and bytes recomputed cannot pass for it
  note     the witness in words

A check is a necessary condition that a person's reading implies, not the reading
itself: it catches a mistyped or stale address, and --selftest measures how much
each row's checks tell a neighbour apart. The audit also requires, per fn row, an
FDE starting at addr with that size, the bytes, and hook::probe `ok` (through
build/loader/decode-dump); per data row, .data or .bss; per rodata row, .rodata and the
bytes.
"""
import argparse
import collections
import csv
import json
import os
import re
import struct
import subprocess
import sys

import capstone
from capstone import x86
import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import harvest  # noqa: E402

HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MANIFEST_DIR = os.path.join(HERE, 'manifest')
GAME = os.path.expanduser('~/.local/share/Steam/steamapps/common/Baldurs Gate 3/bin/bg3')
DECODE_DUMP = os.path.join(HERE, 'build', 'loader', 'decode-dump')
COLUMNS = ['symbol', 'kind', 'addr', 'size', 'bytes', 'checks', 'note']
FINGERPRINT = 16
SHF_WRITE, SHF_ALLOC, SHF_EXECINSTR = 1, 2, 4
GLOBALS = ('.data', '.bss')                  # where a data row may be: not .data.rel.ro
RODATA = '.rodata'                           # where a rodata row may be
R_RELATIVE = 8
CHECK_RE = re.compile(r'([a-z-]+):("(?:[^"\\]|\\.)*"|\S+)')


# ---------------------------------------------------------------- the file

def read_manifest(path):
    """(build-id, header comment lines, rows as dicts)."""
    build, head, body = None, [], []
    with open(path, newline='') as f:
        for line in f:
            if line.startswith('#'):
                head.append(line)
                m = re.match(r'# build-id ([0-9a-f]+)\s*$', line)
                if m:
                    build = m.group(1)
            else:
                body.append(line)
    rows = list(csv.DictReader(body, delimiter='\t', quoting=csv.QUOTE_NONE))
    for r in rows:
        r['addr'] = int(r['addr'], 16)
        r['size'] = None if r['size'] == '-' else int(r['size'])
    return build, head, rows


def write_manifest(path, head, rows):
    with open(path, 'w') as f:
        f.writelines(head)
        f.write('\t'.join(COLUMNS) + '\n')
        for r in rows:
            f.write('\t'.join([r['symbol'], r['kind'], '%#x' % r['addr'],
                               '-' if r['size'] is None else str(r['size']),
                               r['bytes'], r['checks'], r['note']]) + '\n')


def parse_checks(s):
    out, pos = [], 0
    for m in CHECK_RE.finditer(s):
        if s[pos:m.start()].strip():
            raise ValueError('unparsed check text: %r' % s[pos:m.start()])
        arg = m.group(2)
        out.append((m.group(1), json.loads(arg) if arg.startswith('"') else arg))
        pos = m.end()
    if s[pos:].strip():
        raise ValueError('unparsed check text: %r' % s[pos:])
    return out


def find_manifest(build, d=MANIFEST_DIR):
    p = os.path.join(d, build + '.tsv')
    return p if os.path.exists(p) else None


# ---------------------------------------------------------------- the binary

class Bin:
    def __init__(self, path):
        self.path = path
        self.h = harvest.Harvest(path, need_relocs=False)   # in-str and callers need them
        self.e = self.h.elf
        self.build = self.h.build_id
        self.secs = sorted(self.e.sections.items(), key=lambda kv: kv[1]['addr'])
        self.fde_start = {int(a): int(n) for a, n in self.h.fd.tolist()}
        self.sym_at = collections.defaultdict(set)
        for i, n in enumerate(self.h.names):
            if n:
                self.sym_at[n].add(int(self.h.tab['value'][i]))
        r = self.e.rela('.rela.dyn')
        rel = r[(r['info'] & 0xffffffff) == R_RELATIVE]
        o = np.argsort(rel['off'])
        self.rel_off, self.rel_add = rel['off'][o], rel['addend'][o].astype(np.uint64)
        self.md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
        self.md.detail = True
        self._refs = {}
        self._callers = None

    def section_of(self, a):
        for name, s in self.secs:
            if s['addr'] and s['addr'] <= a < s['addr'] + s['size']:
                return name, s
        return None, None

    def read(self, a, n):
        _, s = self.section_of(a)
        if s is None or s['type'] == 8:                       # SHT_NOBITS
            return None
        o = s['off'] + a - s['addr']
        return bytes(self.e.m[o:o + min(n, s['addr'] + s['size'] - a)])

    def cstring(self, a):
        """The NUL-terminated bytes at a (without the NUL), or None if none within 4 KiB."""
        t = self.read(a, 4096)
        if t is None or b'\0' not in t:
            return None
        return t[:t.index(b'\0')]

    def relative_at(self, a):
        i = np.searchsorted(self.rel_off, np.uint64(a))
        if i < len(self.rel_off) and int(self.rel_off[i]) == a:
            return int(self.rel_add[i])
        return None

    def texts_at(self, a):
        """The strings an address can stand for: the C string at it, and for a
        relocated {ptr, len} constant the len bytes it points at (and their C string)."""
        out = set()
        b = self.read(a, 4096)
        if b and b'\0' in b:
            out.add(b[:b.index(b'\0')].decode('utf-8', 'replace'))
        p = self.relative_at(a)
        if p is not None:
            b = self.read(p, 4096)
            if b:
                if b'\0' in b:
                    out.add(b[:b.index(b'\0')].decode('utf-8', 'replace'))
                ln = self.read(a + 8, 8)
                if ln and len(ln) == 8:
                    n, = struct.unpack('<Q', ln)
                    if 0 < n <= len(b):
                        out.add(b[:n].decode('utf-8', 'replace'))
        return out

    def refs(self, fn):
        """Exact addresses the function at fn refers to: direct branch targets and
        rip-relative operands, by capstone over the FDE's whole range."""
        if fn in self._refs:
            return self._refs[fn]
        size = self.fde_start[fn]
        code = self.read(fn, size)
        out, o = set(), 0
        while o < len(code):
            n = 0
            for ins in self.md.disasm(code[o:], fn + o):
                n += ins.size
                for op in ins.operands:
                    if op.type == x86.X86_OP_MEM and op.mem.base == x86.X86_REG_RIP:
                        out.add(ins.address + ins.size + op.mem.disp)
                    elif op.type == x86.X86_OP_IMM and (ins.group(capstone.CS_GRP_JUMP) or
                                                        ins.group(capstone.CS_GRP_CALL)):
                        out.add(op.imm)
            o += n + 1 if o + n < len(code) else n            # skip one undecodable byte
        self._refs[fn] = out
        return out

    def globals_in(self, fn):
        """The distinct .data/.bss addresses fn's instructions refer to, in code order."""
        out, lo = [], [(s['addr'], s['addr'] + s['size']) for n, s in self.secs if n in GLOBALS]
        for ins in self.md.disasm(self.read(fn, self.fde_start[fn]), fn):
            for op in ins.operands:
                if op.type == x86.X86_OP_MEM and op.mem.base == x86.X86_REG_RIP:
                    t = ins.address + ins.size + op.mem.disp
                    if t not in out and any(a <= t < e for a, e in lo):
                        out.append(t)
        return out

    def string_users(self, text):
        """Functions whose relocations point at TEXT or at a relocated pointer to it
        (the candidates; refs() then asks each exactly)."""
        needle, at, m = text.encode() + b'\0', [], self.e.m
        for name, s in self.secs:
            if not s['addr'] or s['type'] == 8 or not s['flags'] & SHF_ALLOC:
                continue
            k = m.find(needle, s['off'], s['off'] + s['size'])
            while k >= 0:
                at.append(s['addr'] + k - s['off'])
                k = m.find(needle, k + 1, s['off'] + s['size'])
        if not at:
            return []
        at = np.array(at, np.uint64)
        holders = self.rel_off[np.isin(self.rel_add, at)]
        want = np.concatenate([at, holders])
        t = self.h.target
        near = np.zeros(len(t), bool)
        for k in range(5):                                  # S + A + 4, an imm after: + 1..4
            near |= np.isin(t - np.uint64(k), want)
        fdes = set(self.h.site_fde[near].tolist()) - {-1}
        return sorted(int(self.h.fstart[i]) for i in fdes)

    def callers(self, a):
        if self._callers is None:
            call = self.h.rtype == harvest.R_PLT32
            self._callers = collections.Counter(self.h.target[call].tolist())
        return self._callers.get(a, 0)


# ---------------------------------------------------------------- the audit

def resolve_ref(arg, rows_by_name):
    if arg in rows_by_name:
        return rows_by_name[arg]['addr']
    return int(arg, 16)


def check_row(b, r, rows_by_name, probe):
    """Failures of one row, as strings; empty = the row verifies."""
    bad, a = [], r['addr']
    name, s = b.section_of(a)
    if r['kind'] == 'fn':
        if a not in b.fde_start:
            bad.append('no FDE starts at %#x (%s)' % (a, name))
            return bad
        if r['size'] != b.fde_start[a]:
            bad.append('size %s, FDE says %d' % (r['size'], b.fde_start[a]))
        want = b.read(a, min(FINGERPRINT, b.fde_start[a])).hex()
        if r['bytes'] != want:
            bad.append('bytes %s, binary has %s' % (r['bytes'], want))
        if probe is not None and probe.get(a) != 'ok':
            bad.append('hook::probe: %s' % probe.get(a, 'not probed'))
    elif r['kind'] == 'data':
        if name not in GLOBALS:
            bad.append('%#x is in %s, not .data or .bss' % (a, name))
    elif r['kind'] == 'rodata':
        if name != RODATA:
            bad.append('%#x is in %s, not %s' % (a, name, RODATA))
        elif r['size'] is None or r['bytes'] == '-':
            bad.append('size and bytes are the witness: run fill')
        else:
            want = b.read(a, r['size'])
            if want is None or want.hex() != r['bytes']:
                bad.append('bytes %s, binary has %s' % (r['bytes'], want.hex() if want else None))
    else:
        bad.append('kind %r' % r['kind'])
    try:
        checks = parse_checks(r['checks'])
    except ValueError as e:
        return bad + [str(e)]
    if not checks:
        bad.append('no checks: nothing ties this address to its name')
    for kind, arg in checks:
        why = None
        if kind == 'symtab':
            if a not in b.sym_at.get(arg, ()):
                why = '.symtab does not have it here'
        elif kind == 'str':
            if r['kind'] != 'fn':
                why = 'only a function refers to strings'
            elif not any(arg in b.texts_at(t) for t in b.refs(a)):
                why = 'the function does not refer to it'
        elif kind == 'in':
            who = resolve_ref(arg, rows_by_name)
            if who not in b.fde_start:
                why = 'no FDE starts at %#x' % who
            elif a not in b.refs(who):
                why = '%#x does not refer to %#x' % (who, a)
        elif kind == 'refs':
            what = resolve_ref(arg, rows_by_name)
            if r['kind'] != 'fn':
                why = 'only a function refers to things'
            elif what not in b.refs(a):
                why = 'not referred to'
        elif kind in ('in-str', 'callers') and b.h.rel is None:
            why = 'needs .rela.text, and this binary was not linked with --emit-relocs'
        elif kind == 'in-str':
            users = b.string_users(arg)
            if not users:
                why = 'no function refers to that string'
            elif not any(a in b.refs(u) for u in users):
                why = 'none of its %d users refers to %#x' % (len(users), a)
        elif kind == 'jt':
            fn, table, i = arg.split(':')
            why = jump_case(b, a, resolve_ref(fn, rows_by_name), int(table, 16), int(i))
        elif kind == 'nth':
            fn, k = arg.split(':')
            fn, k = resolve_ref(fn, rows_by_name), int(k)
            seq = b.globals_in(fn) if fn in b.fde_start else []
            if k >= len(seq) or seq[k] != a:
                why = 'global %d of %#x is %s' % (k, fn, '%#x' % seq[k] if k < len(seq) else 'none')
        elif kind == 'callers':
            n = b.callers(a)
            if n < int(arg):
                why = '%d call sites' % n
        elif kind == 'text-re':
            if r['kind'] != 'rodata':
                why = 'only a rodata row is text'
            else:
                t = b.cstring(a)
                if t is None or not re.fullmatch(arg, t.decode('utf-8', 'replace')):
                    why = 'the text is %r' % (t[:40] if t is not None else None)
        else:
            why = 'unknown check'
        if why:
            bad.append('%s:%s — %s' % (kind, json.dumps(arg) if ' ' in str(arg) else arg, why))
    return bad


def jump_case(b, a, fn, table, i):
    """None if entry i of the jump table leads, inside fn, to a first call to a."""
    if fn not in b.fde_start or table not in b.refs(fn):
        return '%#x is not a function referring to the table %#x' % (fn, table)
    off, = struct.unpack('<i', b.read(table + 4 * i, 4))
    blk = table + off
    if not fn <= blk < fn + b.fde_start[fn]:
        return 'entry %d leads to %#x, outside %#x' % (i, blk, fn)
    for ins in b.md.disasm(b.read(blk, 64), blk):
        if ins.group(capstone.CS_GRP_CALL):
            ops = ins.operands
            if ops and ops[0].type == x86.X86_OP_IMM and ops[0].imm == a:
                return None
            return 'entry %d: the first call at %#x is %s %s' % (i, ins.address, ins.mnemonic, ins.op_str)
        if ins.group(capstone.CS_GRP_JUMP) or ins.group(capstone.CS_GRP_RET):
            break
    return 'entry %d: no call before the block at %#x ends' % (i, blk)


def run_probe(b, addrs, tool):
    """hook::probe on each function, whole body, through decode-dump --probe."""
    recs = b''
    for a in addrs:
        code = b.read(a, b.fde_start[a])
        recs += struct.pack('<QI', a, len(code)) + code
    out = subprocess.run([tool, '--probe'], input=recs, capture_output=True, check=True).stdout
    return {int(x.split(b'\t')[0], 16): x.split(b'\t')[1].decode() for x in out.splitlines()}


def audit(b, rows, tool, quiet=False):
    rows_by_name = {r['symbol']: r for r in rows}
    fns = [r['addr'] for r in rows if r['kind'] == 'fn' and r['addr'] in b.fde_start]
    probe = run_probe(b, fns, tool) if tool else None
    failed = 0
    for r in rows:
        bad = check_row(b, r, rows_by_name, probe)
        failed += bool(bad)
        if not quiet:
            print('%-4s  %-50s %-4s %#x' % ('FAIL' if bad else 'ok', r['symbol'], r['kind'], r['addr']))
            for x in bad:
                print('        ' + x)
    return failed


def selftest(b, rows, tool):
    """Plant errors the audit exists to catch; each must fail its row.
    fn:   the next function in the binary, with size and bytes recomputed (a wrong
          address that a careless fill made self-consistent) — only the checks can
          catch it; and the right address with one byte of the fingerprint changed.
    data: the neighbouring slots, 8 bytes either side; and every other writable
          global that the row's in: functions all refer to — the sibling in the same
          pattern, which is how a person misreads one (five managers loaded in a row).
    rodata: the neighbouring slots; one byte of the content changed; and the next
          literal in .rodata with size and bytes recomputed from it — the careless
          fill again, which only the checks (text-re, in) can tell from the row."""
    starts = sorted(b.fde_start)
    idx = {a: i for i, a in enumerate(starts)}
    by_name = {r['symbol']: r for r in rows}
    missed, planted = [], 0

    def plant(r, label, **change):
        nonlocal planted
        planted += 1
        m = dict(r, **change)
        names = dict(by_name, **{r['symbol']: m})
        probe = run_probe(b, [m['addr']], tool) if tool and m['kind'] == 'fn' else None
        if not check_row(b, m, names, probe):
            missed.append((r['symbol'], label))

    for r in rows:
        if r['kind'] == 'fn':
            nxt = starts[idx[r['addr']] + 1]
            n = b.fde_start[nxt]
            plant(r, 'next function %#x' % nxt, addr=nxt, size=n,
                  bytes=b.read(nxt, min(FINGERPRINT, n)).hex())
            bs = bytearray.fromhex(r['bytes'])
            bs[-1] ^= 0x01
            plant(r, 'fingerprint byte', bytes=bs.hex())
        elif r['kind'] == 'rodata':
            for d in (-8, 8):
                plant(r, 'slot %+d' % d, addr=r['addr'] + d)
            bs = bytearray.fromhex(r['bytes'])
            bs[-2] ^= 0x01                                    # not the NUL
            plant(r, 'content byte', bytes=bs.hex())
            nxt = r['addr'] + r['size']
            t = b.cstring(nxt)
            if t is not None:
                plant(r, 'next literal %#x' % nxt, addr=nxt, size=len(t) + 1, bytes=(t + b'\0').hex())
        else:
            for d in (-8, 8):
                plant(r, 'slot %+d' % d, addr=r['addr'] + d)
            ins = [resolve_ref(x, by_name) for k, x in parse_checks(r['checks']) if k == 'in']
            if ins:
                common = set.intersection(*[b.refs(f) for f in ins]) - {r['addr']}
                for g in sorted(common):
                    if b.section_of(g)[0] in GLOBALS:
                        plant(r, 'sibling %#x' % g, addr=g)
    print('selftest: %d errors planted, %d caught' % (planted, planted - len(missed)))
    for s, label in missed:
        print('  MISSED  %-50s %s' % (s, label))
    return 1 if missed else 0


# ---------------------------------------------------------------- fill / seed

def fill(path, binpath):
    build, head, rows = read_manifest(path)
    b = Bin(binpath)
    if build != b.build:
        sys.exit('%s is for build %s, %s is %s' % (path, build, binpath, b.build))
    for r in rows:
        if r['kind'] == 'fn':
            if r['addr'] not in b.fde_start:
                sys.exit('%s: no FDE starts at %#x' % (r['symbol'], r['addr']))
            r['size'] = b.fde_start[r['addr']]
            r['bytes'] = b.read(r['addr'], min(FINGERPRINT, r['size'])).hex()
        elif r['kind'] == 'rodata':
            if r['size'] is None:                             # a C string: through its NUL
                t = b.cstring(r['addr'])
                if t is None:
                    sys.exit('%s: no C string at %#x and no size given' % (r['symbol'], r['addr']))
                r['size'] = len(t) + 1
            r['bytes'] = b.read(r['addr'], r['size']).hex()
        else:
            r['size'], r['bytes'] = None, '-'
    write_manifest(path, head, rows)
    print('%s: %d rows filled from %s' % (path, len(rows), binpath))


def seed(targets):
    """targets.py's located rows as manifest rows; checks from the evidence that is
    mechanical (symtab, a string's users), the hand witnesses left for a person."""
    with open(targets, newline='') as f:
        rows = list(csv.DictReader(f, delimiter='\t', quoting=csv.QUOTE_NONE))
    print('\t'.join(COLUMNS))
    for r in rows:
        if r['verdict'] != 'located':
            continue
        texts = [json.loads(q) for q in
                 re.findall(r'string ("(?:[^"\\]|\\.)*"): 0x[0-9a-f]+ refers to it', r['witness'])]
        checks = ['in-str:' + (json.dumps(t) if ' ' in t else t) for t in texts]
        if 'symtab' in r['witness']:
            checks.append('symtab:TODO')
        kind = r['kind'] or 'TODO'
        print('\t'.join([r['upstream'], kind, r['linux'], '-', '-', ' '.join(dict.fromkeys(checks)),
                         r['witness'].replace('\t', ' ')]))


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    sub = ap.add_subparsers(dest='cmd', required=True)
    v = sub.add_parser('verify')
    v.add_argument('bin', nargs='?', default=GAME)
    v.add_argument('--manifest-dir', default=MANIFEST_DIR)
    v.add_argument('--decode-dump', default=DECODE_DUMP)
    v.add_argument('--no-probe', action='store_true', help='skip hook::probe (not an audit)')
    v.add_argument('--selftest', action='store_true')
    fl = sub.add_parser('fill')
    fl.add_argument('manifest')
    fl.add_argument('bin')
    sd = sub.add_parser('seed')
    sd.add_argument('targets')
    a = ap.parse_args()

    if a.cmd == 'fill':
        return fill(a.manifest, a.bin)
    if a.cmd == 'seed':
        return seed(a.targets)
    b = Bin(a.bin)
    path = find_manifest(b.build, a.manifest_dir)
    if not path:
        print('%s: build-id %s has no manifest: the loader stays passive on this build'
              % (a.bin, b.build))
        return 3
    tool = None if a.no_probe else a.decode_dump
    if tool and not os.access(tool, os.X_OK):
        sys.exit('%s not built (cmake --build build/loader), or pass --no-probe' % tool)
    build, _, rows = read_manifest(path)
    if build != b.build:
        sys.exit('%s: header says build-id %s' % (path, build))
    names = collections.Counter(r['symbol'] for r in rows)
    if max(names.values(), default=1) > 1:
        sys.exit('duplicate symbols: %s' % [n for n, c in names.items() if c > 1])
    print('%s\n  build-id %s, manifest %s, %d rows' % (a.bin, b.build, os.path.relpath(path), len(rows)))
    if a.selftest:
        if audit(b, rows, tool, quiet=True):
            print('selftest: the untouched manifest does not verify; run verify first')
            return 1
        print('selftest: untouched manifest verifies (%d rows)' % len(rows))
        return selftest(b, rows, tool)
    failed = audit(b, rows, tool)
    kinds = collections.Counter(r['kind'] for r in rows)
    print('%d of %d rows verify (%s)%s' % (len(rows) - failed, len(rows),
          ', '.join('%d %s' % (kinds[k], k) for k in ('fn', 'data', 'rodata') if kinds[k]),
          '' if tool else '; hook::probe NOT run'))
    return 1 if failed or not tool else 0


if __name__ == '__main__':
    sys.exit(main())
