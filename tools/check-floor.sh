#!/bin/bash
# check-floor.sh — does a shared object load on the sniper runtime's glibc 2.31?
#
#   tools/check-floor.sh build/loader/libbg3se.so     exit 0 = meets the floor
#   tools/check-floor.sh --selftest                   calibration, see below
#
# "It loads on the dev box" proves nothing: the game gets the host's glibc here.
# What decides it is the highest GLIBC_x.y version the object
# requires and the libraries it NEEDs, both read from the file.
#
# --selftest builds two objects with the host compiler: one calling only ancient
# libc, one calling a function newer than the floor (pidfd_spawn is 2.39; falls back
# to closefrom, 2.34). The checker must pass the first and fail the second, or its
# verdicts mean nothing.
set -u
floor=2.31
allowed_needed='^(libc\.so\.6|libm\.so\.6|libdl\.so\.2|libpthread\.so\.0|librt\.so\.1|ld-linux-x86-64\.so\.2)$'

check() {
    local so=$1 bad=0 max needed
    [ -f "$so" ] || { echo "no such file: $so"; return 2; }
    max=$(readelf --version-info -W "$so" | grep -o 'GLIBC_[0-9.]*' | sed 's/GLIBC_//' | sort -uV | tail -n 1)
    if [ -z "$max" ]; then
        echo "$so: no GLIBC version requirements found (not dynamically linked against glibc?)"
        return 2
    fi
    if [ "$(printf '%s\n%s\n' "$max" "$floor" | sort -V | tail -n 1)" != "$floor" ]; then
        echo "$so: needs GLIBC_$max > $floor:"
        readelf --dyn-syms -W "$so" | grep -E "@GLIBC_$max( |\$)" | awk '{print "    " $8}'
        bad=1
    fi
    needed=$(readelf -d "$so" | sed -n 's/.*(NEEDED).*\[\(.*\)\]/\1/p')
    while read -r lib; do
        [ -z "$lib" ] && continue
        if ! [[ $lib =~ $allowed_needed ]]; then
            echo "$so: NEEDED $lib is not part of the floor (link it statically)"
            bad=1
        fi
    done <<< "$needed"
    [ $bad -eq 0 ] && echo "$so: ok (max GLIBC_$max, floor $floor)"
    return $bad
}

selftest() {
    local d rc=0 fn
    d=$(mktemp -d)
    echo 'int f(void){return 1;} int g(const char*s){extern unsigned long strlen(const char*);return strlen(s);}' > "$d/old.c"
    gcc -shared -fPIC -o "$d/old.so" "$d/old.c" || return 2
    local built=""
    for fn in pidfd_spawn closefrom; do
        echo "int $fn(int, ...); int f(void){return $fn(3,0,0,0,0,0);}" > "$d/new.c"   # C23: () means (void)
        rm -f "$d/new.so"
        gcc -shared -fPIC -w -o "$d/new.so" "$d/new.c" &&
            readelf --dyn-syms -W "$d/new.so" | grep -q "$fn@GLIBC_2\.\(3[2-9]\|[4-9]\)" && { built=$fn; break; }
    done
    if [ -z "$built" ]; then
        echo "SELFTEST FAIL: could not build an object that needs a glibc above $floor"
        rm -rf "$d"
        return 1
    fi
    echo "selftest: old.so must pass"
    check "$d/old.so" || { echo "SELFTEST FAIL: clean object rejected"; rc=1; }
    echo "selftest: new.so (calls $built) must be rejected, with exit status 1 exactly"
    check "$d/new.so"
    [ $? -eq 1 ] || { echo "SELFTEST FAIL: object above the floor not rejected as such"; rc=1; }
    rm -rf "$d"
    [ $rc -eq 0 ] && echo "selftest ok"
    return $rc
}

case ${1:-} in
    --selftest) selftest ;;
    "") echo "usage: $0 <file.so> | --selftest"; exit 2 ;;
    *) check "$1" ;;
esac
