#!/usr/bin/env python3
"""Carry function names from the macOS x86_64 slice onto the Linux ELF.

  match.py LINUX_FEATURES MAC_FEATURES              match, write dumps/match/<ids>/
  match.py LINUX_FEATURES MAC_FEATURES --selftest   calibration only, nothing written
  match.py ... --selftest --holdout-re Noesis       the hard calibration (see below)

Both arguments are directories written by features.py.

What two builds of one source revision share, whatever the compiler did to the
code: the strings a function uses, the imports and named globals it touches, and
whom it calls, in order. Functions and the data addresses code refers to are both
"nodes" and are matched the same way (a vtable, a global: upstream's targets are
as often data as code). So:

  seeds      functions that carry the same symbol name on both sides (the Linux
             .symtab names ~43k third-party functions; the Mach-O names everything)
  votes      (a) a string / import / named global referenced by exactly one still
                 unmatched function on each side;
             (b) for a matched pair, its two event sequences are aligned on what is
                 already known (matched callees, shared tokens) and the unmatched
                 callees left in equal-sized gaps pair up by position;
             (c) a matched callee with exactly one unmatched caller on each side;
             (d) tables of function pointers in data (vtables, mostly: same ABI, so
                 same slot order). Two tables of equal length pair up when already
                 matched functions sit at the same slots in both, and then their
                 other slots pair by position. This is the road into virtual
                 functions, which nobody calls directly.
             (e) similarity: see Matcher.similar.
  accept     a pair that is each other's best candidate, scores >= --threshold and
             beats the runner-up on both sides; then vote again, to a fixpoint.
  refuse     a pair whose Linux node has a .symtab name that the macOS node does not
             carry, up to platform spelling (see Names). The Linux name is ground
             truth; such a pair is the "contains that one's body" error, an ICF twin
             or worse. Refused pairs go to refused.tsv, and the named Linux nodes
             that were not seeds give a second precision figure (`named_nonseed`).

Calibration, because a wrong name is worse than no name: a fifth of the name seeds
are withheld and asked for afterwards (right / wrong / none), and so are harvest.py's
`single` anchors, which are Larian functions and never seeds. --holdout-re withholds
a whole library instead, which is the honest test: Larian code has no seeds inside it.
"""
import argparse
import collections
import csv
import difflib
import json
import os
import re
import subprocess
import sys
import zlib

import numpy as np

csv.field_size_limit(1 << 30)
K_LEA = 3
W_UNIQUE, W_LEFTOVER, W_GAP1, W_GAP, W_CALLER, W_SLOT = 3, 1, 2, 1, 1, 2


def norm(name):
    """One spelling for both sides: libc++'s inline namespace is __2 on Linux and __1
    on macOS, and LTO numbers local symbols differently in each link."""
    name = name.replace('St3__2', 'St3__1')
    return re.sub(r'(\.(llvm\.)?\d+)+$', '', name)


class Names:
    """Do a Linux name and a macOS name denote the same function? Compared demangled,
    after what differs by platform and not by entity: int64_t is `long` on Linux and
    `long long` on macOS, thunk offsets follow the (different) layouts, libc++ ABI
    tags differ, C1/C2 and D1/D2 demangle alike, and an extern "C" Linux name meets
    a C++ one on macOS (`PxCreateFoundation`)."""

    def __init__(self):
        self.p = subprocess.Popen(['c++filt'], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                  text=True, bufsize=1)
        self.cache = {}

    def canon(self, name):
        d = self.cache.get(name)
        if d is None:
            self.p.stdin.write(name + '\n')
            self.p.stdin.flush()
            d = self.p.stdout.readline().rstrip('\n')
            d = re.sub(r'^(non-)?virtual thunk to ', '', d)
            d = re.sub(r'\[abi:[^\]]*\]', '', d).replace('std::__2::', 'std::__1::')
            d = d.replace('unsigned long long', 'unsigned long').replace('long long', 'long')
            self.cache[name] = d
        return d

    @staticmethod
    def callee(d):
        """the name without its argument list (and cv-qualifier)"""
        d = re.sub(r'\s*(const|volatile|&|&&)$', '', d)
        if not d.endswith(')'):
            return d
        depth = 0
        for i in range(len(d) - 1, -1, -1):
            depth += {')': 1, '(': -1}.get(d[i], 0)
            if depth == 0:
                return d[:i]
        return d

    def verdict(self, lnames, mnames):
        """None if they agree, else 'overload' (same callee, other arguments) or 'other'"""
        if set(lnames) & set(mnames):
            return None
        L = {self.canon(x) for x in lnames}
        M = {self.canon(x) for x in mnames}
        if L & M:
            return None
        bare = {self.callee(y).rsplit('::', 1)[-1] for y in M}
        if any(not x.startswith('_Z') and x in bare for x in lnames):
            return None
        if {self.callee(x) for x in L} & {self.callee(y) for y in M}:
            return 'overload'
        return 'other'


class Side:
    def __init__(self, d, vocab):
        self.dir = d
        z = np.load(os.path.join(d, 'features.npz'))
        self.fstart, self.fsize = z['fstart'], z['fsize']
        self.n = len(self.fstart)
        self.names, raw = {}, {}
        for r in self._tsv('names.tsv'):
            self.names[int(r[0])] = [norm(x) for x in r[2].split(' ')]
            for x in r[2].split(' '):
                raw[x] = int(r[0])
        # foo.cold.N is a piece the compiler cut out of foo: its events are foo's
        part = {}
        for x, i in raw.items():
            g = re.match(r'(.+)\.cold\.\d+$', x)
            if g and g.group(1) in raw:
                part[i] = raw[g.group(1)]
        self.parts = len(part)
        strings = {int(r[0], 16): 's:' + r[1] for r in self._tsv('strings.tsv')}
        imports = {int(r[0], 16): 'i:' + norm(r[1]) for r in self._tsv('imports.tsv')}
        self.summary = json.load(open(os.path.join(d, 'summary.json')))

        # Nodes are what gets matched: functions 0..n-1, then every data address the
        # code refers to (n..). Events, per function and in address order, are
        # references to nodes, -(node + 1), or shared tokens (strings, imports), >= 0.
        start_index = {a: i for i, a in enumerate(self.fstart.tolist())}
        node = {a: part.get(i, i) for a, i in start_index.items()}
        self.addr = self.fstart.tolist()
        self.events = [[] for _ in range(self.n)]
        ev = self.events
        for fn, tgt, kind in zip(z['ref_fn'].tolist(), z['ref_target'].tolist(),
                                 z['ref_kind'].tolist()):
            fn = part.get(fn, fn)
            tok = imports.get(tgt) if kind < K_LEA else strings.get(tgt) or imports.get(tgt)
            if tok is not None:
                ev[fn].append(vocab.setdefault(tok, len(vocab)))
                continue
            g = node.get(tgt)
            if g is None:
                if kind < K_LEA:
                    continue                          # a transfer into a function's middle
                g = node[tgt] = len(self.addr)
                self.addr.append(tgt)
            if g != fn:
                ev[fn].append(-(g + 1))
        for r in self._tsv('datasyms.tsv'):
            g = node.get(int(r[0], 16))
            if g is not None and g >= self.n:
                self.names.setdefault(g, []).append(norm(r[1]))
        self.node = node

        # tables: runs of consecutive data words that each hold a function's address
        self.dataname = sorted((int(r[0], 16), r[1]) for r in self._tsv('datasyms.tsv'))
        order = np.argsort(z['ptr_site'])
        site, tgt = z['ptr_site'][order].tolist(), z['ptr_target'][order].tolist()
        self.tables, self.table_addr = [], []
        for i, (a, t) in enumerate(zip(site, tgt)):
            if i == 0 or a != site[i - 1] + 8:
                self.tables.append([])
                self.table_addr.append(a)
            self.tables[-1].append(start_index[t])
        self.table_at = {a: t for t, a in enumerate(self.table_addr)}
        self.occ = collections.defaultdict(list)          # function -> [(table, slot)]
        for t, fns in enumerate(self.tables):
            if len(fns) > 1:
                for i, f in enumerate(fns):
                    self.occ[f].append((t, i))

    def table_label(self, t):
        import bisect
        a = self.table_addr[t]
        k = bisect.bisect_right(self.dataname, (a, '\x7f')) - 1
        if k < 0:
            return ''
        return '%s+%#x' % (self.dataname[k][1], a - self.dataname[k][0])

    def _tsv(self, name):
        with open(os.path.join(self.dir, name), newline='') as f:
            rd = csv.reader(f, delimiter='\t', quoting=csv.QUOTE_NONE)
            next(rd)
            yield from rd

    def index(self):
        """callers[node] (for data: the functions that refer to it) and
        token -> referring functions, after prune()."""
        self.callers = collections.defaultdict(set)
        self.token_fns = collections.defaultdict(set)
        self.out = [set() for _ in self.events]            # the nodes a function refers to
        for f, es in enumerate(self.events):
            for e in es:
                if e < 0:
                    self.callers[-e - 1].add(f)
                    self.out[f].add(-e - 1)
                else:
                    self.token_fns[e].add(f)


def prune(a, b):
    """Drop tokens only one side has: they can neither anchor nor vote."""
    ta = {e for es in a.events for e in es if e >= 0}
    tb = {e for es in b.events for e in es if e >= 0}
    both = ta & tb
    for s in (a, b):
        s.events = [[e for e in es if e < 0 or e in both] for es in s.events]
    return len(both)


def _best(items):
    """best and runner-up score per left and per right item, from ((l, m), score)"""
    bl, bm = {}, {}
    for (l, m), v in items:
        for d, k in ((bl, l), (bm, m)):
            b = d.get(k)
            if b is None:
                d[k] = [v, 0]
            elif v > b[0]:
                d[k] = [v, b[0]]
            elif v > b[1]:
                b[1] = v
    return bl, bm


class Matcher:
    """All state is incremental: a round costs what changed, not what is known.
    `dirty` are matched functions whose alignment must be redone because a node
    they refer to was matched since; `dx`/`dy` are unmatched nodes that gained a key."""

    def __init__(self, L, M, threshold, similarity, margin, check=None):
        self.L, self.M, self.T, self.S, self.margin = L, M, threshold, similarity, margin
        self.check, self.refused, self.agreed = check, {}, set()
        self.lm, self.ml, self.why = {}, {}, {}
        self.tlm, self.tml, self.twhy = {}, {}, {}        # tables
        self.dirty, self.avotes = set(), {}
        # similarity: node -> keys, key -> unmatched nodes holding it, candidate scores
        self.kl, self.km = collections.defaultdict(set), collections.defaultdict(set)
        self.il, self.im = collections.defaultdict(set), collections.defaultdict(set)
        for S, keys, idx in ((L, self.kl, self.il), (M, self.km, self.im)):
            for t, fns in S.token_fns.items():
                idx[t] = set(fns)
                for f in fns:
                    keys[f].add(t)
        self.dx, self.dy = set(self.kl), set(self.km)
        self.cx, self.cy = collections.defaultdict(dict), collections.defaultdict(set)
        self.wcache = {}

    def add(self, l, m, why):
        if l in self.lm or m in self.ml or (l < self.L.n) != (m < self.M.n):
            return False
        if why != 'name' and self.check:
            v = self.check(l, m)
            if v:
                self.refused.setdefault((l, m), (v, why))
                return False
            if v is None:
                self.agreed.add(l)
        self.lm[l], self.ml[m], self.why[l] = m, l, why
        if l < self.L.n:
            self.dirty.add(l)
        for S, node, done, keys, idx, dirty in ((self.L, l, self.lm, self.kl, self.il, self.dx),
                                                (self.M, m, self.ml, self.km, self.im, self.dy)):
            for q in keys.pop(node, ()):
                idx[q].discard(node)
            gain = [(r, ('o', l)) for r in S.callers.get(node, ())]
            if node < S.n:
                gain += [(g, ('i', l)) for g in S.out[node]]
            for r, key in gain:
                if r in done:
                    if key[0] == 'o':                      # a matched referrer: re-align it
                        self.dirty.add(r if S is self.L else self.ml[r])
                else:
                    keys[r].add(key)
                    idx[key].add(r)
                    dirty.add(r)
        for y in self.cx.pop(l, ()):
            self.cy[y].discard(l)
        for x in self.cy.pop(m, ()):
            self.cx[x].pop(m, None)
        return True

    # -- votes

    def _align(self, l, m):
        votes = []
        A, B = self.L.events[l], self.M.events[m]
        lm, ml = self.lm, self.ml
        # keys equal exactly where the two events are known to be the same thing
        ka = [('t', e) if e >= 0 else ('f', -e - 1) if -e - 1 in lm else ('ua', i)
              for i, e in enumerate(A)]
        kb = [('t', e) if e >= 0 else ('f', ml[-e - 1]) if -e - 1 in ml else ('ub', i)
              for i, e in enumerate(B)]
        if not any(k[0] == 'ua' for k in ka) or not any(k[0] == 'ub' for k in kb):
            return votes
        if len(ka) > 4000 or len(kb) > 4000:
            return votes                             # difflib is quadratic; a handful
        nl, nm = self.L.n, self.M.n
        sm = difflib.SequenceMatcher(None, ka, kb, autojunk=False)
        pa = pb = 0
        for blk in sm.get_matching_blocks():
            ga, gb = range(pa, blk.a), range(pb, blk.b)
            if len(ga) == len(gb) and ga:
                # every event in the gap unmatched, and function facing function
                if all(ka[i][0] == 'ua' for i in ga) and all(kb[j][0] == 'ub' for j in gb) \
                        and all((-A[i] - 1 < nl) == (-B[j] - 1 < nm) for i, j in zip(ga, gb)):
                    w = W_GAP1 if len(ga) == 1 else W_GAP
                    for i, j in zip(ga, gb):
                        votes.append(((-A[i] - 1, -B[j] - 1), w))
            pa, pb = blk.a + blk.size, blk.b + blk.size
        return votes

    def round(self):
        L, M, lm, ml = self.L, self.M, self.lm, self.ml
        votes = collections.Counter()
        for t, fl in L.token_fns.items():
            fm = M.token_fns.get(t)
            if not fm:
                continue
            ul = [f for f in fl if f not in lm]
            um = [f for f in fm if f not in ml]
            if len(ul) == 1 and len(um) == 1:
                unique = len(fl) == 1 and len(fm) == 1
                votes[(ul[0], um[0])] += W_UNIQUE if unique else W_LEFTOVER
        self.new_tables = self.match_tables()
        for t, t2 in self.tlm.items():
            for a, b in zip(L.tables[t], M.tables[t2]):
                if a not in lm and b not in ml:
                    votes[(a, b)] += W_SLOT
        for l in self.dirty:
            v = self._align(l, lm[l])
            if v:
                self.avotes[l] = v
            else:
                self.avotes.pop(l, None)
        self.dirty.clear()
        for lst in self.avotes.values():
            for (x, y), w in lst:
                if x not in lm and y not in ml:
                    votes[(x, y)] += w
        for l, m in lm.items():
            ul = [f for f in L.callers.get(l, ()) if f not in lm]
            if len(ul) == 1:
                um = [f for f in M.callers.get(m, ()) if f not in ml]
                if len(um) == 1:
                    votes[(ul[0], um[0])] += W_CALLER
        return votes

    def match_tables(self):
        """Pair tables of equal length on matched functions at equal slots."""
        L, M, lm = self.L, self.M, self.lm
        tv = collections.Counter()
        for l, m in lm.items():
            ol, om = L.occ.get(l), M.occ.get(m)
            if not ol or not om or len(ol) * len(om) > 250000:
                continue
            bucket = collections.defaultdict(list)
            for t, i in om:
                if t not in self.tml:
                    bucket[(i, len(M.tables[t]))].append(t)
            for t, i in ol:
                if t not in self.tlm:
                    for t2 in bucket.get((i, len(L.tables[t])), ()):
                        tv[(t, t2)] += 1
        best_l, best_m = _best(tv.items())
        new = 0
        # a matched pair of data addresses that both start a table: the constructor
        # pair said so (it stores the vtable pointer), which beats counting slots
        for l, m in lm.items():
            if l >= L.n:
                t, t2 = L.table_at.get(L.addr[l]), M.table_at.get(M.addr[m])
                if t is not None and t2 is not None and t not in self.tlm and t2 not in self.tml:
                    pairs = list(zip(L.tables[t], M.tables[t2]))
                    agree = sum(1 for x, y in pairs if lm.get(x) == y)
                    clash = sum(1 for x, y in pairs if x in lm and lm[x] != y and y in self.ml)
                    if 3 * clash <= agree + 1:
                        self.tlm[t], self.tml[t2], self.twhy[t] = t2, t, (agree, clash)
                        new += 1
        for (t, t2), v in sorted(tv.items(), key=lambda kv: -kv[1]):
            if v < 2 or t in self.tlm or t2 in self.tml:
                continue
            if v == best_l[t][0] and v == best_m[t2][0] and \
                    2 * v >= 3 * max(best_l[t][1], best_m[t2][1]):
                clash = sum(1 for a, b in zip(L.tables[t], M.tables[t2])
                            if a in lm and lm[a] != b and self.ml.get(b) is not None)
                if 3 * clash <= v:
                    self.tlm[t], self.tml[t2], self.twhy[t] = t2, t, (v, clash)
                    new += 1
        return new

    def _weight(self, q):
        """Rare keys weigh more. A key's frequency is how many nodes could hold it."""
        w = self.wcache.get(q)
        if w is None:
            L, M = self.L, self.M
            if isinstance(q, tuple):
                l = q[1]
                m = self.lm[l]
                if q[0] == 'o':
                    n = len(L.callers.get(l, ())) + len(M.callers.get(m, ()))
                else:
                    n = len(L.out[l]) + len(M.out[m])
            else:
                n = len(L.token_fns[q]) + len(M.token_fns[q])
            w = self.wcache[q] = 1.0 / np.log2(2 + n)
        return w

    def _score(self, x, y):
        a, b = self.kl.get(x), self.km.get(y)
        if not a or not b:
            return 0.0
        both = a & b
        if len(both) < 2:
            return 0.0
        w = self._weight
        wb = sum(w(q) for q in both)
        return wb / (sum(w(q) for q in a) + sum(w(q) for q in b) - wb)

    def similar(self, rnd):
        """Rules (a)-(c) want one candidate. Inlining differs between the builds, so
        often there are several; then compare everything known about a node at once:
        its keys are the shared tokens it uses, the matched nodes it refers to and
        the matched functions that refer to it. Candidates share a rare key; the
        score is the weighted Jaccard index of the key sets."""
        L, M, lm, ml, cx, cy = self.L, self.M, self.lm, self.ml, self.cx, self.cy
        floor = self.S / self.margin                  # below this not even a runner-up

        def candidates(node, keys, other_idx, left):
            rare = sorted((len(other_idx.get(q, ())), i, q) for i, q in enumerate(keys))[:3]
            return {o for n, _, q in rare if 0 < n <= 64 for o in other_idx[q]
                    if ((node < L.n) == (o < M.n) if left else (o < L.n) == (node < M.n))}

        def put(x, y):
            v = self._score(x, y)
            if v >= floor:
                cx[x][y] = v
                cy[y].add(x)
            elif y in cx.get(x, ()):
                del cx[x][y]
                cy[y].discard(x)

        for x in self.dx:
            k = self.kl.get(x)
            if x in lm or not k or len(k) < 2:
                continue
            for y in set(cx.get(x, ())) | candidates(x, k, self.im, True):
                put(x, y)
        for y in self.dy:
            k = self.km.get(y)
            if y in ml or not k or len(k) < 2:
                continue
            for x in set(cy.get(y, ())) | candidates(y, k, self.il, False):
                put(x, y)
        self.dx.clear()
        self.dy.clear()

        pairs = [((x, y), v) for x, d in cx.items() for y, v in d.items()]
        bl, bm = _best(pairs)
        take = [(v, x, y) for (x, y), v in pairs
                if v >= self.S and v == bl[x][0] and v == bm[y][0]
                and v >= self.margin * max(bl[x][1], bm[y][1])]
        new = 0
        for v, x, y in sorted(take, reverse=True):
            new += self.add(x, y, 's%d:%.2f' % (rnd, v))
        return new

    def accept(self, votes, rnd):
        bl, bm = _best(votes.items())
        new = 0
        for (l, m), v in sorted(votes.items(), key=lambda kv: -kv[1]):
            if v < self.T or l in self.lm or m in self.ml:
                continue
            if v == bl[l][0] and v == bm[m][0] and 2 * v >= 3 * max(bl[l][1], bm[m][1]):
                new += self.add(l, m, 'r%d:%d' % (rnd, v))
        return new

    def run(self, log):
        rnd = 0
        while True:
            rnd += 1
            new = self.accept(self.round(), rnd)
            sim = self.similar(rnd)
            log('  round %d: +%d by votes, +%d by similarity (%d nodes), +%d tables (%d)'
                % (rnd, new, sim, len(self.lm), self.new_tables, len(self.tlm)))
            if new + sim + self.new_tables < 25 or rnd >= 60:     # the tail is rounds of +5
                return rnd


# ---------------------------------------------------------------- ground truth

def name_pairs(L, M):
    """Linux node -> macOS node where a name is on exactly one of each; functions
    and data apart."""
    inv = collections.defaultdict(list)
    for i, ns in M.names.items():
        for x in ns:
            inv[x].append(i)
    cnt = collections.Counter(x for ns in L.names.values() for x in ns)
    fn, data = {}, {}
    for i, ns in L.names.items():
        for x in ns:
            if cnt[x] == 1 and len(inv.get(x, ())) == 1 and (i < L.n) == (inv[x][0] < M.n):
                (fn if i < L.n else data)[i] = (inv[x][0], x)
                break
    return fn, data, inv


def anchor_pairs(L, inv, harvest_dir):
    """harvest.py's `single` anchors: a Linux function whose name is known from a
    local static it owns. Never seeds."""
    path = os.path.join(harvest_dir, L.summary['id'], 'anchors.tsv')
    if not os.path.exists(path):
        return {}
    at = {a: i for i, a in enumerate(L.fstart.tolist())}
    out = {}
    with open(path, newline='') as f:
        for r in csv.DictReader(f, delimiter='\t', quoting=csv.QUOTE_NONE):
            if r['status'] != 'single':
                continue
            l, ms = at.get(int(r['function'], 16)), inv.get(norm(r['mangled']), ())
            if l is not None and len(ms) == 1 and l not in L.names:
                out[l] = (ms[0], r['demangled'])
    return out


def score(matcher, truth):
    right = wrong = twin = 0
    bad = []
    name_of = lambda i: matcher.M.names.get(i, ['?'])[0]
    for l, (m, name) in truth.items():
        got = matcher.lm.get(l)
        if got is None:
            continue
        if got == m:
            right += 1
        elif _same_entity(name_of(m), name_of(got)):
            twin += 1                # C1/C2, D1/D2: one constructor, two symbols
        else:
            wrong += 1
            bad.append((l, name, matcher.M.names.get(got, ['?'])[0], matcher.why.get(l)))
    n = len(truth)
    hit = right + twin
    return dict(asked=n, right=right, twin=twin, wrong=wrong, none=n - hit - wrong,
                precision=round(hit / (hit + wrong), 4) if hit + wrong else None,
                recall=round(hit / n, 4) if n else None), bad


def refusals(mt):
    """Named Linux nodes that were not seeds, matched other than by name: the pairs
    that kept their name (right) and the pairs refused (wrong), by evidence. A hard
    set by construction -- a name is left out of the seeds for being ambiguous (ICF
    twins, template copies) -- so its rate is not the matcher's overall one."""
    out = collections.defaultdict(lambda: collections.Counter())
    for l in mt.agreed:
        if l in mt.lm:
            out['votes' if mt.why[l][0] == 'r' else 'sim'][('function' if l < mt.L.n else 'data') + ' right'] += 1
    for (l, m), (v, why) in mt.refused.items():
        out['votes' if why[0] == 'r' else 'sim'][('function' if l < mt.L.n else 'data') + ' refused ' + v] += 1
    return {k: dict(sorted(c.items())) for k, c in sorted(out.items())}


def _same_entity(a, b):
    """Complete/base constructor and destructor variants are one function in source."""
    v = re.compile(r'([CD])[0-5](?=E)')
    return a != b and v.sub(r'\1', a) == v.sub(r'\1', b)


# ---------------------------------------------------------------- main

def main():
    here = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('linux')
    ap.add_argument('mac')
    ap.add_argument('--threshold', type=int, default=2)
    ap.add_argument('--similarity', type=float, default=0.8, help='least Jaccard score')
    ap.add_argument('--margin', type=float, default=1.25, help='best / runner-up, at least')
    ap.add_argument('--holdout-re', help='withhold seeds whose name matches (default: a hashed fifth)')
    ap.add_argument('--selftest', action='store_true')
    ap.add_argument('--no-calibration', action='store_true',
                    help='skip the withheld-seed run; only after --selftest on this pair')
    ap.add_argument('--harvest', default=os.path.join(here, 'dumps', 'harvest'))
    ap.add_argument('--out', default=os.path.join(here, 'dumps', 'match'))
    a = ap.parse_args()
    log = lambda *x: print(*x, file=sys.stderr, flush=True)

    vocab = {}
    L, M = Side(a.linux, vocab), Side(a.mac, vocab)
    shared = prune(L, M)
    L.index(), M.index()
    truth, dtruth, inv = name_pairs(L, M)
    anchors = anchor_pairs(L, inv, a.harvest)
    log('functions %d / %d, data nodes %d / %d, shared tokens %d, name pairs %d + %d data, '
        'anchors with a macOS name %d' % (L.n, M.n, len(L.addr) - L.n, len(M.addr) - M.n,
                                          shared, len(truth), len(dtruth), len(anchors)))

    names = Names()

    def run(held):
        def check(l, m):
            """False: nothing to compare (unnamed, or withheld for calibration)"""
            ln, mn = L.names.get(l), M.names.get(m)
            if not ln or not mn or l in held:
                return False
            return names.verdict(ln, mn)
        mt = Matcher(L, M, a.threshold, a.similarity, a.margin, check)
        for tr in (truth, dtruth):
            for l, (m, _) in tr.items():
                if l not in held:
                    mt.add(l, m, 'name')
        rounds = mt.run(log)
        return mt, rounds

    if a.no_calibration:
        held, dheld = {}, {}
    elif a.holdout_re:
        rx = re.compile(a.holdout_re)
        held = {l: v for l, v in truth.items() if rx.search(v[1])}
    else:
        held = {l: v for l, v in truth.items() if zlib.crc32(v[1].encode()) % 5 == 0}
    if not a.no_calibration:
        dheld = {l: v for l, v in dtruth.items() if zlib.crc32(v[1].encode()) % 5 == 0}
    mt, rounds = run({**held, **dheld})
    res = dict(threshold=a.threshold, holdout=a.holdout_re or 'crc32 % 5 == 0',
               seeds=len(truth) - len(held), matched=len(mt.lm), tables=len(mt.tlm),
               rounds=rounds)
    res['matched_functions'] = sum(1 for l in mt.lm if l < L.n)
    res['holdout_names'], bad_h = score(mt, held)
    res['holdout_data'], _ = score(mt, dheld)
    bands = collections.defaultdict(lambda: [0, 0])
    for l, (m, _) in held.items():
        if l in mt.lm:
            w = mt.why[l]
            band = 'votes' if w[0] == 'r' else 'sim %.1f' % (int(float(w.split(':')[1]) * 10) / 10)
            ok = mt.lm[l] == m or _same_entity(M.names[m][0], M.names.get(mt.lm[l], ['?'])[0])
            bands[band][0 if ok else 1] += 1
    res['holdout_names_by_evidence'] = {k: dict(right=v[0], wrong=v[1]) for k, v in sorted(bands.items())}
    res['anchors'], bad_a = score(mt, anchors)
    larian = {l: v for l, v in anchors.items()
              if re.match(r'(\(anonymous namespace\)::)?(ls|esv|ecl|eoc|ecs|stats|gui|ui|net|game|App)\b', v[1])}
    res['anchors_larian'], _ = score(mt, larian)
    res['named_nonseed'] = refusals(mt)
    print(json.dumps(res, indent=1))
    for title, bad in (('holdout', bad_h), ('anchors', bad_a)):
        for l, want, got, why in bad[:12]:
            log('  wrong (%s) %#x %s\n      want %s\n      got  %s' %
                (title, L.addr[l], why, want[:150], got[:150]))
    if a.selftest:
        return

    # the real run: every seed in, anchors still out (they stay the running check)
    if not a.no_calibration:
        mt, rounds = run({})
    final, _ = score(mt, anchors)
    out = os.path.join(a.out, '%s_%s' % (L.summary['id'][:12], M.summary['id'][:12]))
    os.makedirs(out, exist_ok=True)
    for fname, want_fn in (('matches.tsv', True), ('data.tsv', False)):
        with open(os.path.join(out, fname), 'w') as f:
            f.write('linux\tmac\tevidence\tmac_names\n')
            for l in sorted(mt.lm):
                if (l < L.n) == want_fn:
                    m = mt.lm[l]
                    f.write('%#x\t%#x\t%s\t%s\n' % (L.addr[l], M.addr[m], mt.why[l],
                                                   ' '.join(M.names.get(m, []))))
    with open(os.path.join(out, 'refused.tsv'), 'w') as f:
        f.write('linux\tmac\tevidence\tverdict\tlinux_names\tmac_names\n')
        for (l, m), (v, why) in sorted(mt.refused.items(), key=lambda kv: L.addr[kv[0][0]]):
            f.write('%#x\t%#x\t%s\t%s\t%s\t%s\n' % (L.addr[l], M.addr[m], why, v,
                    ' '.join(L.names[l]), ' '.join(M.names.get(m, []))))
    with open(os.path.join(out, 'tables.tsv'), 'w') as f:
        f.write('linux\tmac\tslots\tagree\tclash\tmac_symbol\n')
        for t in sorted(mt.tlm, key=lambda t: L.table_addr[t]):
            t2 = mt.tlm[t]
            f.write('%#x\t%#x\t%d\t%d\t%d\t%s\n' % (L.table_addr[t], M.table_addr[t2],
                    len(L.tables[t]), mt.twhy[t][0], mt.twhy[t][1], M.table_label(t2)))
    res = dict(calibration=res, final=dict(seeds=len(truth) + len(dtruth), matched=len(mt.lm),
                                           matched_functions=sum(1 for l in mt.lm if l < L.n),
                                           tables=len(mt.tlm),
                                           rounds=rounds, anchors=final,
                                           named_nonseed=refusals(mt)))
    with open(os.path.join(out, 'summary.json'), 'w') as f:
        json.dump(res, f, indent=1)
    log('wrote', out, json.dumps(res['final']))


if __name__ == '__main__':
    main()
