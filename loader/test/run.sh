#!/bin/bash
# Smoke test of libbg3se.so without the game.   loader/test/run.sh [build-dir]
#
# Runs the stand-in "bg3" under LD_PRELOAD and checks the log for the lines the real
# launch should also produce: attach, the fork, deferred init, the C++ probe, and the
# Osiris hooks (stub libOsiris.so; the stand-in exits non-zero if any value that
# crossed an interposer came back changed).
# Then the negative: the same preload in a process not named bg3 must write nothing.
set -u
build=$(realpath "${1:-$(dirname "$0")/../../build/loader}")
so=$build/libbg3se.so
# the negative checks below pass on an empty log, so nothing missing may get that far
for f in "$so" "$build/fake/bg3" "$build/fake/libSDL2.so" "$build/fake/libOsiris.so" "$build/hook-test" \
         "$build/decode-dump" "$build/manifest-check"; do
    [ -f "$f" ] || { echo "FAIL  not built: $f"; exit 2; }
done
log=$(mktemp)
osilog=$(mktemp)
export BG3SE_OSI_LOG=$osilog
fail=0

check() {   # $1 = description, $2 = pattern
    if grep -q -- "$2" "$log"; then echo "ok    $1"; else echo "FAIL  $1"; fail=1; fi
}

BG3SE_LOG=$log BG3SE_ALLOW_UNKNOWN=1 LD_PRELOAD=$so "$build/fake/bg3" || { echo "FAIL  fake bg3 exit status"; fail=1; }
check "constructor attached"            'attach: exe=.*/fake/bg3 build-id=[0-9a-f]\{40\} (UNKNOWN'
check "fork seen in the child"          'fork: child of'
check "deferred init on first hook"     'deferred init via SDL_Init'
check "C++ throw/catch inside the .so"  'c++ runtime: thrown and caught inside the game'
check "real SDL_Init reached"           'SDL_Init(0x20) = 42'

# Osiris: first that the stand-in is a fair one — without the preload it must pass too,
# or a failure above says nothing about the interposers
"$build/fake/bg3" > /dev/null || { echo "FAIL  fake bg3 fails on its own, without the preload"; fail=1; }
check "DivFunctions shim installed"     'osiris: RegisterDIVFunctions(self=0x[0-9a-f]* *) get_handle=(nil) call=0x'
check "function mappings captured"      'osiris: GetFunctionMappings: 3 functions, 3 distinct ids'
check "InitGame seen, result kept"      'osiris: InitGame(self=0x[0-9a-f]*) = 1'
check "Merge path read as 4-byte wchar" 'osiris: Merge(self=0x[0-9a-f]*, "/story/gôal.div") = 0'
ocheck() { if grep -q -- "$2" "$osilog"; then echo "ok    $1"; else echo "FAIL  $1"; fail=1; fi; }
ocheck "event logged by name"           'event CombatStarted("S_Player_Karlach_2c76687d-93a2-477b-8b18-8a14b549304c")$'
ocheck "unmapped id logged by number"   'event #999("S_Player'
ocheck "error callback logged"          'error stub story error'
if grep -q '\] call  \|\] query ' "$osilog"; then echo "FAIL  calls/queries are off by default"; fail=1
else echo "ok    calls/queries are off by default"; fi
: > "$osilog"
BG3SE_OSI=calls,queries BG3SE_LOG=/dev/null BG3SE_ALLOW_UNKNOWN=1 LD_PRELOAD=$so "$build/fake/bg3" > /dev/null
ocheck "call logged, alias type as string" 'call  ApplyStatus("S_Player_Karlach_[0-9a-f-]*", 7)$'
ocheck "query logged with out-parameter"   'query GetHitpoints("S_Player_Karlach_[0-9a-f-]*", 7) = true$'
if grep -q ' event ' "$osilog"; then echo "FAIL  BG3SE_OSI=calls,queries silences events"; fail=1
else echo "ok    BG3SE_OSI=calls,queries silences events"; fi

# build-id read from memory must equal what readelf reads from the file
want=$(readelf -n "$build/fake/bg3" | awk '/Build ID/{print $3}')
check "build-id matches readelf ($want)" "build-id=$want"

# unknown build without the override: attach is logged, nothing else happens
: > "$log"
BG3SE_LOG=$log LD_PRELOAD=$so "$build/fake/bg3" > /dev/null
if grep -q 'attach:' "$log" && ! grep -q 'deferred init\|osiris' "$log"; then echo "ok    unknown build stayed passive"
else echo "FAIL  unknown build stayed passive"; fail=1; fi

# any other process name: silent — and the preload must really have been loaded
: > "$log"
err=$(BG3SE_LOG=$log LD_PRELOAD=$so /bin/true 2>&1)
if [ -n "$err" ]; then echo "FAIL  preload did not load cleanly into /bin/true: $err"; fail=1
elif [ -s "$log" ]; then echo "FAIL  silent outside bg3"; fail=1
else echo "ok    silent outside bg3"; fi

# ... and the same binary under a name that is not bg3, to show the name is the gate
cp "$build/fake/bg3" "$build/fake/notbg3"
BG3SE_LOG=$log LD_PRELOAD=$so "$build/fake/notbg3" > /dev/null
if [ -s "$log" ]; then echo "FAIL  silent under another exe name"; fail=1; else echo "ok    silent under another exe name"; fi
rm -f "$build/fake/notbg3"

# the offset manifest: tools/manifest.py writes one for the stand-in and audits it, then
# the loader verifies it in memory. A build with a manifest is known without the override.
repo=$(realpath "$(dirname "$0")/../..")
fb=$build/fake/bg3
mdir=$(mktemp -d)
mf=$mdir/$want.tsv
addr() { nm "$fb" | awk -v s="$1" '$3 == s {print "0x" $1}'; }
{
    echo "# build-id $want"
    printf 'symbol\tkind\taddr\tsize\tbytes\tchecks\tnote\n'
    printf 'fake_fn\tfn\t%s\t-\t-\tsymtab:fake_manifest_fn\thookable\n' "$(addr fake_manifest_fn)"
    printf 'fake_leaf\tfn\t%s\t-\t-\tsymtab:fake_manifest_leaf\ttoo short to hook\n' "$(addr fake_manifest_leaf)"
    printf 'fake_global\tdata\t%s\t-\t-\tsymtab:fake_manifest_global\t.data\n' "$(addr fake_manifest_global)"
    printf 'fake_str\trodata\t%s\t-\t-\tsymtab:fake_manifest_str text-re:[0-9]+(\\.[0-9]+){3}\t.rodata string\n' "$(addr fake_manifest_str)"
} > "$mf"
python3 "$repo/tools/manifest.py" fill "$mf" "$fb" > /dev/null || { echo "FAIL  manifest.py fill on the stand-in"; fail=1; }
audit=$(python3 "$repo/tools/manifest.py" verify "$fb" --manifest-dir "$mdir" --decode-dump "$build/decode-dump")
if [ $? = 1 ] && grep -q '^ok    fake_fn ' <<<"$audit" && grep -q '^ok    fake_global ' <<<"$audit" \
   && grep -A1 '^FAIL  fake_leaf ' <<<"$audit" | grep -q 'hook::probe: '; then
    echo "ok    audit: stand-in rows verify, the short one fails hook::probe"
else sed 's/^/      /' <<<"$audit"; echo "FAIL  audit of the stand-in's manifest"; fail=1; fi

: > "$log"
BG3SE_LOG=$log BG3SE_MANIFEST=$mf LD_PRELOAD=$so "$fb" > /dev/null || { echo "FAIL  fake bg3 with a manifest"; fail=1; }
check "a build with a manifest is known"     'attach: .*build-id=[0-9a-f]* (known)'
check "manifest rows verified in memory"     "manifest: build $want ($mf): 4 rows, 4 verified, 0 off; 1 of 2 functions hookable"
check "short function callable, not hookable" 'manifest: fake_leaf: callable, not hookable'

bad=$mdir/bad.tsv                  # one fingerprint byte changed, a global placed in code, a string placed in .data
awk -F'\t' -v OFS='\t' '$1 == "fake_fn" {$5 = substr($5, 1, 30) (substr($5, 31, 2) == "00" ? "01" : "00")}
                        $1 == "fake_global" {$3 = "'"$(addr fake_manifest_fn)"'"}
                        $1 == "fake_str" {$3 = "'"$(addr fake_manifest_global)"'"} {print}' "$mf" > "$bad"
: > "$log"
BG3SE_LOG=$log BG3SE_MANIFEST=$bad LD_PRELOAD=$so "$fb" > /dev/null
check "changed bytes: the row is off"        'manifest: fake_fn fn 0x[0-9a-f]*: bytes differ — off'
check "a global in code: the row is off"     'manifest: fake_global data 0x[0-9a-f]*: not in a writable segment — off'
check "a string in .data: the row is off"    'manifest: fake_str rodata 0x[0-9a-f]*: not in a read-only segment — off'
check "... and the rest still verifies"      "manifest: build $want ($bad): 4 rows, 1 verified, 3 off"

: > "$log"
BG3SE_LOG=$log BG3SE_ALLOW_UNKNOWN=1 LD_PRELOAD=$so "$fb" > /dev/null
check "no manifest: every address off"       "manifest: none for build $want (embedded)"
if grep -a -q '# build-id c8da62b137caebd03b57c23b65ba2547745ce851$' "$so"; then echo "ok    game manifest embedded"
else echo "FAIL  game manifest embedded"; fail=1; fi
rm -rf "$mdir"

# the crash reader (src/crash.cpp): the stand-in installs a handler through sigaction(2)
# the way bin/bg3 installs LinuxCrashReporter, then faults at a known address. Ours has to
# come first, name the fault, and still hand the signal on -- Larian's reporter is what
# makes a real crash unreadable, so breaking it here would be worse than no reader at all.
: > "$log"
BG3SE_LOG=$log BG3SE_ALLOW_UNKNOWN=1 BG3SE_FAKE_CRASH=1 LD_PRELOAD=$so "$fb" > /dev/null
st=$?
check "crash reader: armed before the hooks"   'crash reader: armed over [0-9]* signals'
check "crash reader: kept the game's handler"  "crash reader: kept the game's handler for SIGSEGV"
check "crash reader: names the fault"          'CRASH SIGSEGV at rip=bg3+0x[0-9a-f]*, si_addr=0x1234'
check "crash reader: dumps the registers"      'CRASH  rax=0x[0-9a-f]* '
check "crash reader: scans the stack"          'CRASH  stack\[+0x[0-9a-f]*\] bg3+0x[0-9a-f]*'
if [ "$st" = 70 ]; then echo "ok    crash reader: chained to the game's handler"
else echo "FAIL  crash reader: chained to the game's handler (exit $st, want 70)"; fail=1; fi

: > "$log"
BG3SE_LOG=$log BG3SE_ALLOW_UNKNOWN=1 BG3SE_FAKE_CRASH=1 BG3SE_FAKE_CRASH_SIGNAL=1 LD_PRELOAD=$so "$fb" > /dev/null
st=$?
check "crash reader: signal(2) is the other door"  "crash reader: kept the game's signal() handler for SIGSEGV"
check "crash reader: names the fault after signal()" 'CRASH SIGSEGV at rip=bg3+0x[0-9a-f]*, si_addr=0x1234'
if [ "$st" = 70 ]; then echo "ok    crash reader: chained to a signal() handler"
else echo "FAIL  crash reader: chained to a signal() handler (exit $st, want 70)"; fail=1; fi

# A stack overflow is the case the alternate stacks exist for: SA_ONSTACK alone does
# nothing on a thread that never called sigaltstack(2), so on a thread with no room left
# the handler cannot run and the process dies silently. The stand-in's thread has a
# 128 kB stack and recurses through it. Two-sided:
# BG3SE_NO_ALT_STACK=1 disables the alternate stacks and must produce no block at all.
: > "$log"
BG3SE_LOG=$log BG3SE_ALLOW_UNKNOWN=1 BG3SE_FAKE_CRASH=1 BG3SE_FAKE_CRASH_OVERFLOW=1 LD_PRELOAD=$so "$fb" > /dev/null
st=$?
check "crash reader: an alt stack per thread"   'crash reader: alt signal stack for tid [0-9]* (512 kB'
check "crash reader: reads a stack overflow"    'CRASH SIGSEGV at rip=bg3+0x[0-9a-f]*'
check "crash reader: and its frames"            'CRASH  stack\[+0x[0-9a-f]*\] bg3+0x[0-9a-f]*'
if [ "$st" = 70 ]; then echo "ok    crash reader: chained after an overflow"
else echo "FAIL  crash reader: chained after an overflow (exit $st, want 70)"; fail=1; fi
# and the offset it prints has to be the one tools/disasm.py takes -- an offset from the
# image base, not from the executable segment. The two differ by 16 MB in this stand-in
# and by 35 MB in bin/bg3, so a segment-relative offset resolves to nothing. eat_stack is
# where it faults.
rip=$(grep -a -m1 'CRASH SIGSEGV at rip=bg3+0x' "$log" | sed 's/.*rip=bg3+0x\([0-9a-f]*\).*/\1/')
want=$(nm "$fb" | awk '$3 == "eat_stack" {print $1}')
if [ -n "$rip" ] && [ -n "$want" ] && [ $((16#$rip)) -ge $((16#$want)) ] && [ $((16#$rip)) -lt $((16#$want + 0x200)) ]
then echo "ok    crash reader: rip=bg3+0x$rip resolves inside eat_stack"
else echo "FAIL  crash reader: rip=bg3+0x${rip:-?} is not inside eat_stack (0x${want:-?})"; fail=1; fi

# The game's threads arrive with an 8 kB alt stack of their own, and ensure_alt_stack()
# keeps it, so the kernel's signal frame (AT_MINSIGSTKSZ 3632 on the dev box), our handler and the
# game's chained one all share 8 kB. Measured on the stand-in: 6 kB is enough; at 5 kB
# where()'s snprintf runs off the end, the nested fault re-enters the handler (SA_NODEFER),
# the chained handler faults too, and the process *hangs* rather than dying -- hence the
# kill timeout.
: > "$log"
BG3SE_LOG=$log BG3SE_ALLOW_UNKNOWN=1 BG3SE_FAKE_CRASH=1 BG3SE_FAKE_CRASH_OVERFLOW=1 \
    BG3SE_FAKE_ALT_KB=8 LD_PRELOAD=$so timeout -s KILL 20 "$fb" > /dev/null 2>&1
st=$?
check "crash reader: keeps a thread's own 8 kB alt stack" 'already had an alt stack (8 kB'
check "crash reader: reads an overflow on 8 kB"           'CRASH SIGSEGV at rip=bg3+0x[0-9a-f]*'
if [ "$st" = 70 ]; then echo "ok    crash reader: chained from an 8 kB alt stack"
else echo "FAIL  crash reader: chained from an 8 kB alt stack (exit $st, want 70)"; fail=1; fi

: > "$log"
BG3SE_LOG=$log BG3SE_ALLOW_UNKNOWN=1 BG3SE_FAKE_CRASH=1 BG3SE_FAKE_CRASH_OVERFLOW=1 \
    BG3SE_NO_ALT_STACK=1 LD_PRELOAD=$so "$fb" > /dev/null
st=$?
if grep -aq 'CRASH' "$log"; then echo "FAIL  calibration: no alt stack must read nothing"; fail=1
elif [ "$st" = 70 ]; then echo "FAIL  calibration: no alt stack must not reach the game's handler"; fail=1
else echo "ok    calibration: without an alt stack the overflow is unreadable (exit $st)"; fi

# The exit interposers: the LoadMenu crash kills the game with no signal any of three
# handlers sees, so `exit` and friends are exported and log their caller. The claim is
# the *return address*, so the check is that it resolves inside fake_exit_caller and not
# merely somewhere in the image.
: > "$log"
BG3SE_LOG=$log BG3SE_ALLOW_UNKNOWN=1 BG3SE_FAKE_EXIT=77 LD_PRELOAD=$so "$fb" > /dev/null
st=$?
check "exit reader: names exit(3) and its status" 'EXIT exit(77) from bg3+0x[0-9a-f]*, tid=[0-9]*'
if [ "$st" = 77 ]; then echo "ok    exit reader: the status is still the program's"
else echo "FAIL  exit reader: the status is still the program's (exit $st, want 77)"; fail=1; fi
eret=$(grep -a -m1 'EXIT exit(77) from bg3+0x' "$log" | sed 's/.*from bg3+0x\([0-9a-f]*\).*/\1/')
ewant=$(nm "$fb" | awk '$3 == "fake_exit_caller" {print $1}')
if [ -n "$eret" ] && [ -n "$ewant" ] && [ $((16#$eret)) -ge $((16#$ewant)) ] && [ $((16#$eret)) -lt $((16#$ewant + 0x80)) ]
then echo "ok    exit reader: the return address is inside fake_exit_caller (bg3+0x$eret)"
else echo "FAIL  exit reader: bg3+0x${eret:-?} is not inside fake_exit_caller (0x${ewant:-?})"; fail=1; fi
# and calibration: with no exit knob the line must not appear at all, or every clean
# quit would read as one.
: > "$log"
BG3SE_LOG=$log BG3SE_ALLOW_UNKNOWN=1 LD_PRELOAD=$so "$fb" > /dev/null
if grep -aq 'EXIT exit(' "$log"; then echo "FAIL  calibration: a run that does not call exit() logged one"; fail=1
else echo "ok    calibration: a run that does not call exit() logs no EXIT line"; fi

# The kill interposers: a process that kills itself leaves no signal any handler sees
# and no exit call either. Terminating signals are
# logged with the caller; anything else must not be, or the game's ordinary signal
# traffic would bury the one line that matters.
: > "$log"
BG3SE_LOG=$log BG3SE_ALLOW_UNKNOWN=1 BG3SE_FAKE_KILL=1 LD_PRELOAD=$so "$fb" > /dev/null
check "kill reader: names a self-kill and its caller" 'KILL kill([0-9]*, 15) from bg3+0x[0-9a-f]*, tid=[0-9]*'
kret=$(grep -a -m1 'KILL kill(' "$log" | sed 's/.*from bg3+0x\([0-9a-f]*\).*/\1/')
kwant=$(nm "$fb" | awk '$3 == "fake_kill_caller" {print $1}')
if [ -n "$kret" ] && [ -n "$kwant" ] && [ $((16#$kret)) -ge $((16#$kwant)) ] && [ $((16#$kret)) -lt $((16#$kwant + 0x80)) ]
then echo "ok    kill reader: the return address is inside fake_kill_caller (bg3+0x$kret)"
else echo "FAIL  kill reader: bg3+0x${kret:-?} is not inside fake_kill_caller (0x${kwant:-?})"; fail=1; fi
if grep -aq 'KILL kill([0-9-]*, 28)' "$log"; then echo "FAIL  calibration: a non-terminating signal was logged"; fail=1
else echo "ok    calibration: SIGWINCH is not logged"; fi
# The parent's view: bin/bg3 forks and the outer process waits, so it is the only thing
# that learns how the game process died. The stand-in forks the same way.
check "wait reader: the parent reports a clean child" 'WAIT waitpid: pid [0-9]* exited with [0-9]*'
: > "$log"
BG3SE_LOG=$log BG3SE_ALLOW_UNKNOWN=1 BG3SE_FAKE_KILL=2 LD_PRELOAD=$so "$fb" > /dev/null
check "wait reader: and one killed by a signal" 'WAIT waitpid: pid [0-9]* killed by signal 15'

# The other half of the stack-overflow case: log_line's buffer is per-thread scratch and not a
# local, because an 8488-byte frame does not fit on some of the game's threads. The
# stand-in walks a thread down to 4 kB left and logs from there; and the frame itself is
# checked, since that is what regresses if someone puts a buffer back on the stack.
: > "$log"
BG3SE_LOG=$log BG3SE_ALLOW_UNKNOWN=1 BG3SE_FAKE_THIN_LOG=1 LD_PRELOAD=$so "$fb" > /dev/null
st=$?
if [ "$st" = 0 ] && awk '/SDL_Init\(0x20\) = 42/{split($3,a,"[]/[]"); if (a[2] != a[3]) found=1} END{exit !found}' "$log"
then echo "ok    log_line: a thread with 4 kB of stack left can log"
else echo "FAIL  log_line: a thread with 4 kB of stack left can log (exit $st)"; fail=1; fi
frames=$(objdump -d -C "$so" \
    --disassemble='bg3se::log_line(char const*, ...)' 2>/dev/null |
    awk '/sub .*0x[0-9a-f]*,%rsp/{sub(/.*sub .*\$0x/,""); sub(/,%rsp.*/,""); print strtonum("0x" $0); exit}')
if [ -n "$frames" ] && [ "$frames" -lt 1024 ]; then echo "ok    log_line: $frames bytes of frame (was 8488)"
else echo "FAIL  log_line: frame is ${frames:-unreadable} bytes, want under 1024"; fail=1; fi

: > "$log"
BG3SE_LOG=$log BG3SE_ALLOW_UNKNOWN=1 BG3SE_FAKE_CRASH=1 BG3SE_NO_CRASH_READER=1 LD_PRELOAD=$so "$fb" > /dev/null
st=$?
if grep -q 'CRASH' "$log"; then echo "FAIL  BG3SE_NO_CRASH_READER leaves the reader out"; fail=1
elif [ "$st" != 70 ]; then echo "FAIL  BG3SE_NO_CRASH_READER leaves the reader out (exit $st, want 70)"; fail=1
else echo "ok    BG3SE_NO_CRASH_READER leaves the reader out"; fi

# the extender proper (ext/, libbg3se-ext.so), if built: dlopened at deferred init through
# the host api, with an empty manifest here, so every game-dependent feature must come
# up off rather than crash. Its own log and console socket land in the state directory,
# which is the log's directory.
ext=$(realpath -m "$build/../ext/libbg3se-ext.so")
repo=$(realpath "$(dirname "$0")/../..")
if [ -f "$ext" ]; then
    sdir=$(mktemp -d)
    BG3SE_LOG=$sdir/bg3se.log BG3SE_ALLOW_UNKNOWN=1 BG3SE_EXT=$ext LD_PRELOAD=$so "$fb" > /dev/null || { echo "FAIL  fake bg3 with the extender loaded (exit status)"; fail=1; }
    log=$sdir/bg3se.log
    check "extender loaded and initialised"      'ext: bg3se_ext_init = 0$'
    # The extender is dlopened after crash::arm(), so the reader only knows it if
    # load_extender() notes it; without the note a frame inside the extender is dropped
    # by the stack scan.
    check "crash reader: the extender is in the image table" \
        'crash reader: libbg3se-ext.so base 0x[0-9a-f]*, text 0x[0-9a-f]*-0x[0-9a-f]*'
    # the other side: with the reader off the dlopen still happens and the note does not
    nlog=$(mktemp)
    BG3SE_LOG=$nlog BG3SE_ALLOW_UNKNOWN=1 BG3SE_NO_CRASH_READER=1 BG3SE_EXT=$ext LD_PRELOAD=$so "$fb" > /dev/null
    if grep -q 'ext: bg3se_ext_init = 0$' "$nlog" && ! grep -q 'crash reader: libbg3se-ext.so' "$nlog"
    then echo "ok    crash reader off: the extender loads and is not noted"
    else echo "FAIL  crash reader off: the extender loads and is not noted"; fail=1; fi
    rm -f "$nlog"
    echeck() { if grep -q -- "$2" "$sdir/extender.log" 2> /dev/null; then echo "ok    $1"; else echo "FAIL  $1"; fail=1; fi; }
    echeck "extender log: symbols from the manifest"  'FindLibraries(): 0 of [0-9]* symbols from the manifest'
    echeck "extender log: critical init failed, no crash" 'allocator or string table missing from the manifest'
    echeck "extender log: initialisation skipped cleanly" 'skipping scripting extension initialization'
    # the second module: the stand-in's libOsiris.so is found by soname; it has no build-id note, so it stays off
    check "libOsiris.so found by soname, no build-id: off" 'manifest: libOsiris.so at 0x[0-9a-f]* has no build-id note: its addresses stay off'

    if [ -S "$sdir/console.sock" ]; then echo "ok    console socket created"; else echo "FAIL  console socket created"; fail=1; fi
    : > "$log"
    BG3SE_LOG=$log BG3SE_ALLOW_UNKNOWN=1 BG3SE_NO_EXT=1 LD_PRELOAD=$so "$fb" > /dev/null
    check "BG3SE_NO_EXT leaves the extender out"  'ext: BG3SE_NO_EXT set'
    if "$repo/tools/check-floor.sh" "$ext" > /dev/null 2>&1; then echo "ok    extender within the glibc floor"
    else "$repo/tools/check-floor.sh" "$ext" | sed 's/^/      /'; echo "FAIL  extender within the glibc floor"; fail=1; fi
    log=$(mktemp)
    rm -rf "$sdir"
else
    echo "skip  extender: $ext not built"
fi

# the embedded manifest against the installed game binary, by the loader's own code (the
# game is not started), and a copy with two planted errors that must both come back off
game=${BG3_BIN:-$HOME/.local/share/Steam/steamapps/common/Baldurs Gate 3/bin/bg3}
if [ ! -f "$game" ]; then echo "skip  manifest against the game: no $game"
elif ! gid=$(readelf -n "$game" | awk '/Build ID/{print $3}') || [ ! -f "$repo/manifest/$gid.tsv" ]; then
    echo "skip  manifest against the game: build $gid has no manifest (the loader stays passive)"
else
    if out=$("$build/manifest-check" "$game"); then echo "ok    game: $(grep -o '[0-9]* rows, .*' <<<"$out")"
    else sed 's/^/      /' <<<"$out"; echo "FAIL  embedded manifest against the game"; fail=1; fi
    planted=$(mktemp)
    awk -F'\t' -v OFS='\t' 'NF >= 5 && $2 == "fn" && !f++ {$5 = substr($5, 1, length($5) - 2) (substr($5, length($5) - 1) == "00" ? "01" : "00")}
                            NF >= 5 && $2 == "data" && !d++ {$3 = "0x1000"} {print}' "$repo/manifest/$gid.tsv" > "$planted"
    out=$(BG3SE_MANIFEST=$planted "$build/manifest-check" "$game")
    if [ $? = 1 ] && [ "$(grep -c ' — off$' <<<"$out")" = 2 ]; then echo "ok    game: a changed byte and a global in the headers are both off"
    else sed 's/^/      /' <<<"$out"; echo "FAIL  planted errors against the game"; fail=1; fi
    rm -f "$planted"
fi

# the second module: libOsiris.so beside the game, its own build-id and manifest (the Osiris
# globals), run through the same code path the loader takes in deferred_init
osi=$(dirname "$game")/libOsiris.so
if [ ! -f "$osi" ]; then echo "skip  manifest against libOsiris.so: no $osi"
elif ! oid=$(readelf -n "$osi" | awk '/Build ID/{print $3}') || [ ! -f "$repo/manifest/$oid.tsv" ]; then
    echo "skip  manifest against libOsiris.so: build $oid has no manifest (its addresses stay off)"
else
    if out=$("$build/manifest-check" "$osi"); then echo "ok    libOsiris.so: $(grep -o '[0-9]* rows, .*' <<<"$out")"
    else sed 's/^/      /' <<<"$out"; echo "FAIL  embedded manifest against libOsiris.so"; fail=1; fi
fi

# the hooking engine on hand-written prologues (its decoder is calibrated separately,
# against capstone on the game binary: tools/hook-decode-check.py)
if out=$("$build/hook-test"); then echo "ok    hook engine: $(grep -c '^ok' <<<"$out") checks"
else grep -v '^ok' <<<"$out" | sed 's/^/      /'; echo "FAIL  hook engine"; fail=1; fi

rm -f "$log" "$osilog"
exit $fail
