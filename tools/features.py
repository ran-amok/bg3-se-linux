#!/usr/bin/env python3
"""Per-function features by disassembly, the same way for both sides of the
name-transfer matcher.

  features.py BIN              ELF (bin/bg3) or Mach-O (thin, or fat with an x86_64 slice)
  features.py ELF --selftest   calibration: the extractor against .rela.text, which
                               says independently where every reference in .text is

Writes dumps/features/<build-id or LC_UUID>/ (never in the repo's history):

  features.npz    fstart, fsize        function table (ELF: FDEs; Mach-O: LC_FUNCTION_STARTS
                                       plus .symtab, which alone has the static initialisers)
                  ptr_site, ptr_target data words that hold a function's address (vtables...)
                  ref_site, ref_fn, ref_target, ref_kind
                                       every reference that leaves its instruction:
                                       direct transfers out of the function and every
                                       rip-relative operand, in address order; one that
                                       names a GOT slot is resolved to the slot's content
  names.tsv       function index -> symbol name(s); Mach-O names lose their extra '_'
  datasyms.tsv    named data symbols (the Mach-O names most of them, the ELF few)
  strings.tsv     referenced address -> the C string there, where there is one
  imports.tsv     stub/PLT address -> imported symbol
  summary.json    counts, and with --selftest the calibration figures

The Linux side could take its references from relocations alone, but then the two
sides would be measured with different instruments. Instead both are disassembled
and the relocations calibrate the instrument.
"""
import argparse
import json
import mmap
import multiprocessing
import os
import re
import struct
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import harvest  # noqa: E402

K_CALL, K_JMP, K_JCC, K_LEA, K_MEM, K_CALLMEM, K_JMPMEM, K_GOT = range(8)
KIND_NAMES = ('call', 'jmp', 'jcc', 'lea', 'mem', 'callmem', 'jmpmem', 'got')
RIP = re.compile(r'\[rip(?: ([+-]) (0x[0-9a-f]+|\d+))?\]')
REF = np.dtype([('site', '<u8'), ('fn', '<i4'), ('target', '<u8'), ('kind', 'u1')])


# ---------------------------------------------------------------- Mach-O

class MachO:
    """Thin x86_64 Mach-O, or the x86_64 slice of a fat one."""

    def __init__(self, path):
        self.f = open(path, 'rb')
        self.m = mmap.mmap(self.f.fileno(), 0, access=mmap.ACCESS_READ)
        m, base = self.m, 0
        if m[:4] in (b'\xca\xfe\xba\xbe', b'\xca\xfe\xba\xbf'):
            wide = m[:4] == b'\xca\xfe\xba\xbf'
            n, = struct.unpack_from('>I', m, 4)
            for i in range(n):
                if wide:
                    cpu, _, off, _, _, _ = struct.unpack_from('>IIQQII', m, 8 + i * 32)
                else:
                    cpu, _, off, _, _ = struct.unpack_from('>IIIII', m, 8 + i * 20)
                if cpu == 0x01000007:
                    base = off
                    break
            else:
                sys.exit('fat Mach-O without an x86_64 slice')
        magic, cpu, _, _, ncmds, _, _, _ = struct.unpack_from('<IIIIIIII', m, base)
        if magic != 0xfeedfacf or cpu != 0x01000007:
            sys.exit('not an x86_64 Mach-O')
        self.base = base
        self.sections, self.uuid = [], None       # sections in ordinal order (n_sect - 1)
        p = base + 32
        for _ in range(ncmds):
            cmd, size = struct.unpack_from('<II', m, p)
            if cmd == 0x19:                                         # LC_SEGMENT_64
                seg = m[p + 8:p + 24].rstrip(b'\0').decode()
                vmaddr, = struct.unpack_from('<Q', m, p + 24)
                if seg == '__TEXT':
                    self.text_vmaddr = vmaddr
                nsects, = struct.unpack_from('<I', m, p + 64)
                for i in range(nsects):
                    q = p + 72 + i * 80
                    addr, sz, off = struct.unpack_from('<QQI', m, q + 32)
                    r1, r2 = struct.unpack_from('<II', m, q + 68)
                    self.sections.append(dict(
                        name=seg + ',' + m[q:q + 16].rstrip(b'\0').decode(),
                        addr=addr, size=sz, off=base + off, reserved1=r1, reserved2=r2))
            elif cmd == 0x2:                                        # LC_SYMTAB
                self.symoff, self.nsyms, self.stroff, self.strsize = \
                    struct.unpack_from('<IIII', m, p + 8)
            elif cmd == 0xb:                                        # LC_DYSYMTAB
                self.indoff, self.nind = struct.unpack_from('<II', m, p + 56)
            elif cmd == 0x26:                                       # LC_FUNCTION_STARTS
                self.fsoff, self.fssize = struct.unpack_from('<II', m, p + 8)
            elif cmd == 0x29:                                       # LC_DATA_IN_CODE
                if struct.unpack_from('<II', m, p + 8)[1]:
                    sys.exit('LC_DATA_IN_CODE is not empty: a linear sweep would decode data')
            elif cmd == 0x1b:                                       # LC_UUID
                self.uuid = m[p + 8:p + 24].hex()
            p += size

    def sec(self, name):
        return next((s for s in self.sections if s['name'] == name), None)

    def function_starts(self):
        m, p = self.m, self.base + self.fsoff
        end, a, out = p + self.fssize, self.text_vmaddr, []
        while p < end:
            d = shift = 0
            while True:
                b = m[p]
                p += 1
                d |= (b & 0x7f) << shift
                shift += 7
                if not b & 0x80:
                    break
            if d == 0:
                break
            a += d
            out.append(a)
        return np.array(out, dtype=np.uint64)

    def symbols(self):
        """[(address, section ordinal, name)] of every defined non-debug symbol, and
        the whole table's names by index for the indirect symbols."""
        NL = np.dtype([('strx', '<u4'), ('type', 'u1'), ('sect', 'u1'),
                       ('desc', '<u2'), ('value', '<u8')])
        tab = np.frombuffer(self.m, NL, count=self.nsyms, offset=self.base + self.symoff)
        so = self.base + self.stroff
        blob = self.m[so:so + self.strsize]

        def name(i):
            o = int(tab['strx'][i])
            return blob[o:blob.index(b'\0', o)].decode('utf-8', 'replace')

        defined = np.nonzero(((tab['type'] & 0xe0) == 0) & ((tab['type'] & 0x0e) == 0x0e))[0]
        return [(int(tab['value'][i]), int(tab['sect'][i]), name(i)) for i in defined.tolist()], name

    def stubs(self, name_of):
        s = self.sec('__TEXT,__stubs')
        if not s or not s['reserved2']:
            return {}
        n = s['size'] // s['reserved2']
        ind = np.frombuffer(self.m, '<u4', count=n,
                            offset=self.base + self.indoff + 4 * s['reserved1'])
        return {s['addr'] + i * s['reserved2']: name_of(int(x))
                for i, x in enumerate(ind.tolist()) if x < self.nsyms}


def unmac(n):
    """Mach-O C symbols carry one extra leading underscore."""
    return n[1:] if n.startswith('_') else n


# ---------------------------------------------------------------- loading

def load(path):
    """Everything the sweep and the tables need, in one format-neutral dict."""
    with open(path, 'rb') as f:
        magic = f.read(4)
    if magic == b'\x7fELF':
        e = harvest.Elf(path)
        fd, _ = e.fdes()
        t = e.sec('.text')
        tab, names = e.symbols()
        typ = tab['info'] & 0xf
        fnames, dnames = {}, []
        for i in np.nonzero((typ == harvest.STT_FUNC) | (typ == harvest.STT_OBJECT))[0].tolist():
            a = int(tab['value'][i])
            if not a or not names[i]:
                continue
            if typ[i] == harvest.STT_FUNC:
                fnames.setdefault(a, []).append(names[i])
            elif not names[i].startswith('.L'):
                dnames.append((a, names[i]))
        imports = {}
        plt, rp, ds = e.sec('.plt'), e.rela('.rela.plt'), e.sec('.dynsym')
        if plt and rp is not None and ds:
            dt = np.frombuffer(e.m, harvest.SYM, count=ds['size'] // 24, offset=ds['off'])
            st = [v for v in e.sections.values() if v['index'] == ds['link']][0]
            for i, info in enumerate(rp['info'].tolist()):
                o = st['off'] + int(dt['name'][info >> 32])
                imports[plt['addr'] + 16 * (i + 1)] = e.m[o:e.m.find(b'\0', o)].decode()
        secs = [dict(name=n, addr=s['addr'], size=s['size'], off=s['off'])
                for n, s in e.sections.items() if s['addr'] and s['type'] != 8]
        # .got: slot -> the address the loader puts there, or the imported name
        got = {}
        g = e.sec('.got')
        if g and ds:
            rd_all = e.rela('.rela.dyn')
            ing = rd_all[(rd_all['off'] >= g['addr']) & (rd_all['off'] < g['addr'] + g['size'])]
            for off, info, add in ing.tolist():
                if info & 0xffffffff == 8:
                    got[off] = add
                elif info >> 32:
                    o = st['off'] + int(dt['name'][info >> 32])
                    got[off] = e.m[o:e.m.find(b'\0', o)].decode()
        # function pointers in data: R_X86_64_RELATIVE words whose addend is a function
        # start. .init_array is left out: its order is link order, not a shared fact.
        rd = e.rela('.rela.dyn')
        rel = rd[(rd['info'] & 0xffffffff) == 8]
        skip = [e.sec(n) for n in ('.init_array', '.fini_array') if e.sec(n)]
        keep = np.isin(rel['addend'].astype(np.uint64), fd[:, 0])
        for k in skip:
            keep &= ~((rel['off'] >= k['addr']) & (rel['off'] < k['addr'] + k['size']))
        ptrs = (rel['off'][keep].astype(np.uint64), rel['addend'][keep].astype(np.uint64))
        return dict(id=e.build_id(), fmt='elf', path=path, fstart=fd[:, 0], fsize=fd[:, 1], ptrs=ptrs, got=got,
                    text=(t['addr'], t['size'], t['off']), fnames=fnames, dnames=dnames,
                    imports=imports, sections=secs, strsecs=('.rodata',))
    mo = MachO(path)
    syms, name_of = mo.symbols()
    t = mo.sec('__TEXT,__text')
    tord = mo.sections.index(t) + 1
    # LC_FUNCTION_STARTS leaves out the static initialisers (___cxx_global_var_init*,
    # __GLOBAL__sub_I_*: 212k of 482k here), so the table is its union with .symtab.
    fs = np.union1d(mo.function_starts(),
                    np.array([a for a, sect, _ in syms if sect == tord], dtype=np.uint64))
    fs = fs[(fs >= t['addr']) & (fs < t['addr'] + t['size'])]
    fsize = np.diff(np.append(fs, np.uint64(t['addr'] + t['size'])))
    fnames, dnames = {}, []
    for a, sect, n in syms:
        if sect == tord:
            fnames.setdefault(a, []).append(unmac(n))
        else:
            dnames.append((a, unmac(n)))
    imports = {a: unmac(n) for a, n in mo.stubs(name_of).items()}
    secs = [dict(name=s['name'], addr=s['addr'], size=s['size'], off=s['off'])
            for s in mo.sections if s['off'] != mo.base]
    # __got: a slot holds the unslid address of a local definition, or is bound to an
    # import named by the indirect symbol table. Weak definitions (template statics:
    # every ls::TypeId) are reached this way, so without it those references are lost.
    got = {}
    g = mo.sec('__DATA_CONST,__got') or mo.sec('__DATA,__got')
    if g:
        n = g['size'] // 8
        val = np.frombuffer(mo.m, '<u8', count=n, offset=g['off']).tolist()
        ind = np.frombuffer(mo.m, '<u4', count=n,
                            offset=mo.base + mo.indoff + 4 * g['reserved1']).tolist()
        for i in range(n):
            if val[i]:
                got[g['addr'] + 8 * i] = val[i]
            elif ind[i] < mo.nsyms:
                got[g['addr'] + 8 * i] = unmac(name_of(ind[i]))
    # function pointers in data: this binary rebases the classic way (LC_DYLD_INFO),
    # so a pointer is on disk as its unslid address
    sites, tgts = [], []
    for sct in mo.sections:
        seg, _, sn = sct['name'].partition(',')
        if seg not in ('__DATA', '__DATA_CONST') or sct['off'] == mo.base or \
                sn in ('__mod_init_func', '__got', '__la_symbol_ptr'):
            continue
        pad = -sct['addr'] % 8
        w = np.frombuffer(mo.m, '<u8', count=(sct['size'] - pad) // 8, offset=sct['off'] + pad)
        hit = np.nonzero(np.isin(w, fs))[0]
        sites.append(sct['addr'] + pad + 8 * hit.astype(np.uint64))
        tgts.append(w[hit])
    ptrs = (np.concatenate(sites), np.concatenate(tgts))
    return dict(id=mo.uuid, fmt='macho', path=path, fstart=fs, fsize=fsize, ptrs=ptrs, got=got,
                text=(t['addr'], t['size'], t['off']), fnames=fnames, dnames=dnames,
                imports=imports, sections=secs,
                strsecs=('__TEXT,__cstring', '__TEXT,__const'))


# ---------------------------------------------------------------- the sweep

_W = {}


def _init(path, text):
    import capstone
    _W['f'] = open(path, 'rb')
    _W['m'] = mmap.mmap(_W['f'].fileno(), 0, access=mmap.ACCESS_READ)
    _W['md'] = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
    _W['text'] = text


def _sweep(job):
    """Linear sweep of functions [lo, hi); on an undecodable byte skip one and go on."""
    lo, starts, sizes = job
    m, md = _W['m'], _W['md']
    taddr, _, toff = _W['text']
    out, bad, insns, tables = [], 0, 0, 0
    for k, (start, size) in enumerate(zip(starts, sizes)):
        fn, end, pos = lo + k, start + size, start
        # Mach-O x86_64 keeps jump tables in __text, after the function's code, and
        # this binary's LC_DATA_IN_CODE is stripped. A lea of an address further on
        # in the same function is such a table: the code ends there.
        limit = end
        while pos < limit:
            o = toff + pos - taddr
            last = pos
            for addr, sz, mn, ops in md.disasm_lite(m[o:o + limit - pos], pos):
                if addr >= limit:
                    break
                last = addr + sz
                insns += 1
                c = mn[0]
                if (c == 'j' or mn == 'call') and ops.startswith('0x'):
                    tgt = int(ops, 16)
                    if mn == 'call':
                        out.append((addr, fn, tgt, K_CALL))
                    elif not start <= tgt < end:
                        out.append((addr, fn, tgt, K_JMP if mn == 'jmp' else K_JCC))
                elif 'rip' in ops:
                    g = RIP.search(ops)
                    if g:
                        d = int(g.group(2), 0) if g.group(2) else 0
                        tgt = addr + sz + (-d if g.group(1) == '-' else d)
                        if mn == 'lea' and last <= tgt < limit:
                            limit = tgt
                            tables += 1
                            continue
                        kind = (K_LEA if mn == 'lea' else K_CALLMEM if mn == 'call'
                                else K_JMPMEM if mn == 'jmp' else K_MEM)
                        out.append((addr, fn, tgt & 0xffffffffffffffff, kind))
            if last < limit:
                bad += 1
                pos = last + 1
            else:
                pos = limit
    return np.array(out, dtype=REF), bad, insns, tables


def sweep(b, jobs):
    fs, sz = b['fstart'].tolist(), b['fsize'].tolist()
    step = max(1, len(fs) // (jobs * 16))
    work = [(i, fs[i:i + step], sz[i:i + step]) for i in range(0, len(fs), step)]
    with multiprocessing.Pool(jobs, _init, (b['path'], b['text'])) as pool:
        parts = pool.map(_sweep, work)
    refs = np.concatenate([p[0] for p in parts])
    return refs, sum(p[1] for p in parts), sum(p[2] for p in parts), sum(p[3] for p in parts)


def resolve_got(b, refs):
    """A reference to a GOT slot becomes a K_GOT reference to what the slot holds;
    a slot bound to an import stays where it is and joins the import table."""
    got = b['got']
    if not got:
        return 0
    slots = np.array(sorted(got), dtype=np.uint64)
    hit = np.nonzero((refs['kind'] >= K_LEA) & np.isin(refs['target'], slots))[0]
    for i in hit.tolist():
        v = got[int(refs['target'][i])]
        if isinstance(v, str):
            b['imports'][int(refs['target'][i])] = v
        else:
            refs['target'][i] = v
            refs['kind'][i] = K_GOT
    return len(hit)


# ---------------------------------------------------------------- strings

def strings_at(b, targets):
    """target address -> C string, for targets inside the string-bearing sections
    whose bytes up to the NUL are printable text of length >= 4."""
    out = {}
    with open(b['path'], 'rb') as f:
        m = mmap.mmap(f.fileno(), 0, access=mmap.ACCESS_READ)
        for s in b['sections']:
            if s['name'] not in b['strsecs']:
                continue
            lo, hi = s['addr'], s['addr'] + s['size']
            for t in targets[(targets >= lo) & (targets < hi)].tolist():
                o = s['off'] + t - lo
                e = m.find(b'\0', o, min(o + 4096, s['off'] + s['size']))
                if e - o < 4:
                    continue
                try:
                    txt = m[o:e].decode('utf-8')
                except UnicodeDecodeError:
                    continue
                if all(c.isprintable() or c in '\n\t\r' for c in txt):
                    out[t] = txt
    return out


# ---------------------------------------------------------------- selftest

def selftest(b, refs):
    """The extractor against the relocations of an --emit-relocs ELF.

    .rela.text lists every place in .text that refers to something else. A direct
    transfer is an R_X86_64_PLT32 whose field ends its instruction, so site and
    target are both exact; for PC32 data references the target is exact only when
    no immediate follows the field, so those are compared by site alone."""
    h = harvest.Harvest(b['path'])
    lo, hi = h.text
    res = {}

    # Direct transfers. External calls are left out on both sides: their relocation
    # names an undefined symbol (no address), the sweep sees a .plt slot.
    tin = (h.target >= lo) & (h.target < hi)
    call = (h.rtype == harvest.R_PLT32) & (h.site_fde >= 0) & tin
    # the field sits 1 byte into call/jmp rel32 and 2 bytes into jcc rel32
    truth = set(zip(h.site[call].tolist(), h.target[call].tolist()))
    d = refs[(refs['kind'] <= K_JCC) & (refs['target'] >= lo) & (refs['target'] < hi)]
    off = np.where(d['kind'] == K_JCC, 2, 1).astype(np.uint64)
    mine = set(zip((d['site'] + off).tolist(), d['target'].tolist()))
    extra = np.array([t for _, t in mine - truth], dtype=np.uint64)
    i = np.minimum(np.searchsorted(h.fstart, extra), len(h.fstart) - 1)
    res['direct_transfers_in_text'] = dict(
        relocs=len(truth), swept=len(mine), both=len(truth & mine),
        reloc_only=len(truth - mine), sweep_only=len(mine - truth),
        sweep_only_to_function_start=int((h.fstart[i] == extra).sum()),
        note='sweep_only: transfers the assembler resolved itself, so no relocation exists')

    # rip-relative operands. The sweep computes the target from the instruction, the
    # relocation gives S+A+4, which is short by the size of a trailing immediate.
    pc = (h.rtype == harvest.R_PC32) & (h.site_fde >= 0)
    sites = h.site[pc]
    r = refs[refs['kind'] >= K_LEA]
    order = np.argsort(r['site'])
    rs = r['site'][order]
    j = np.searchsorted(rs, sites, side='right') - 1
    inside = (j >= 0) & (sites - rs[np.maximum(j, 0)] < 15)
    diff = (r['target'][order][j[inside]].astype(np.int64)
            - h.target[pc][inside].astype(np.int64))
    v, c = np.unique(diff, return_counts=True)
    top = sorted(zip(c.tolist(), v.tolist()), reverse=True)[:8]
    res['pc32_data_refs'] = dict(
        relocs=int(pc.sum()), site_in_a_swept_riprel_insn=int(inside.sum()),
        sweep_minus_reloc_target={str(val): n for n, val in top},
        note='0 = agree; 1, 2, 4 = an imm8/16/32 follows the field; anything else is an error')
    # Function pointers in data: .rela.dyn (what the loader applies) against the
    # --emit-relocs copy for .data.rel.ro (what the linker was told), vtables.py's source.
    r = h.elf.rela('.rela.data.rel.ro')
    tgt = (h.tab['value'][(r['info'] >> 32).astype(np.uint32)].astype(np.int64)
           + r['addend']).astype(np.uint64)
    ok = ((r['info'] & 0xffffffff) == 1) & np.isin(tgt, h.fstart)
    truth = set(zip(r['off'][ok].tolist(), tgt[ok].tolist()))
    ro = h.elf.sec('.data.rel.ro')
    ps, pt = b['ptrs']
    inro = (ps >= ro['addr']) & (ps < ro['addr'] + ro['size'])
    mine = set(zip(ps[inro].tolist(), pt[inro].tolist()))
    res['function_pointers_data_rel_ro'] = dict(
        emit_relocs=len(truth), rela_dyn=len(mine), both=len(truth & mine),
        emit_only=len(truth - mine), dyn_only=len(mine - truth))
    return res


# ---------------------------------------------------------------- main

def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('binary')
    ap.add_argument('--out', default=os.path.join(os.path.dirname(os.path.dirname(
        os.path.abspath(__file__))), 'dumps', 'features'))
    ap.add_argument('--jobs', type=int, default=max(1, (os.cpu_count() or 2) - 2))
    ap.add_argument('--selftest', action='store_true')
    a = ap.parse_args()

    b = load(a.binary)
    refs, bad, insns, tables = sweep(b, a.jobs)
    refs = refs[np.argsort(refs['site'], kind='stable')]
    via_got = resolve_got(b, refs) if not a.selftest else 0
    summary = dict(binary=a.binary, id=b['id'], format=b['fmt'], functions=len(b['fstart']),
                   named_functions=sum(1 for x in b['fstart'].tolist() if x in b['fnames']),
                   instructions=insns, undecodable_resyncs=bad, in_function_tables=tables,
                   refs={KIND_NAMES[k]: int((refs['kind'] == k).sum()) for k in range(8)},
                   got_slots=len(b['got']), refs_through_got=via_got)

    if a.selftest:
        if b['fmt'] != 'elf':
            sys.exit('--selftest needs the --emit-relocs ELF')
        summary['selftest'] = selftest(b, refs)
        print(json.dumps(summary, indent=1))
        return

    out = os.path.join(a.out, b['id'])
    os.makedirs(out, exist_ok=True)
    data = refs[refs['kind'] >= K_LEA]
    strs = strings_at(b, np.unique(data['target']))
    summary['strings'] = len(strs)
    summary['imports'] = len(b['imports'])
    summary['data_symbols'] = len(b['dnames'])
    summary['function_pointers_in_data'] = len(b['ptrs'][0])
    np.savez_compressed(os.path.join(out, 'features.npz'), fstart=b['fstart'], fsize=b['fsize'],
                        ref_site=refs['site'], ref_fn=refs['fn'], ref_target=refs['target'],
                        ref_kind=refs['kind'], ptr_site=b['ptrs'][0], ptr_target=b['ptrs'][1])
    with open(os.path.join(out, 'names.tsv'), 'w') as f:
        f.write('function\tstart\tnames\n')
        for i, s in enumerate(b['fstart'].tolist()):
            if s in b['fnames']:
                f.write('%d\t%#x\t%s\n' % (i, s, ' '.join(b['fnames'][s])))
    with open(os.path.join(out, 'datasyms.tsv'), 'w') as f:
        f.write('address\tname\n')
        for ad, n in sorted(b['dnames']):
            f.write('%#x\t%s\n' % (ad, n))
    with open(os.path.join(out, 'strings.tsv'), 'w') as f:
        f.write('address\tstring\n')
        for ad in sorted(strs):
            f.write('%#x\t%s\n' % (ad, json.dumps(strs[ad], ensure_ascii=False)))
    with open(os.path.join(out, 'imports.tsv'), 'w') as f:
        f.write('address\tname\n')
        for ad in sorted(b['imports']):
            f.write('%#x\t%s\n' % (ad, b['imports'][ad]))
    with open(os.path.join(out, 'summary.json'), 'w') as f:
        json.dump(summary, f, indent=1)
    print(json.dumps(summary, indent=1))
    print('wrote', out)


if __name__ == '__main__':
    main()
