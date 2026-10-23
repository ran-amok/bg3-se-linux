#!/bin/bash
# bg3se-launch.sh — Steam launch-option wrapper that preloads the extender.
#
# Steam launch options:   /path/to/bg3-se-linux/tools/bg3se-launch.sh %command%
#   with the /proc probe: BG3SE_PROBE=1 /path/to/bg3-se-linux/tools/bg3se-launch.sh %command%
#
# Runs OUTSIDE the container. It only exports environment and execs "$@" untouched
# (Valve: never rewrite argv). pressure-vessel carries LD_PRELOAD entries into the
# container and rewrites their paths; $HOME is shared with the container, so the
# build tree is reachable from inside as it is.
#
# Every process of the launch chain loads the object; its constructor returns at
# once unless the process is bin/bg3. Log: ~/.local/state/bg3-se-linux/bg3se.log (or under
# the XDG_STATE_HOME a launch.conf beside this script sets -- the play-box case, below); the
# extender's own is extender.log beside it, its console tools/bg3se-console; the game's
# own stdout and stderr (Larian's crash reporter among them) go to game-stdout.log.
# If the object is missing the game launches without it; BG3SE_NO_EXT=1 keeps the
# extender out (loader only, as in Phases 2-4).
#
# Headless mode: with the flag file $state/headless present, the chain is run inside
# a nested gamescope compositor with no output -- the game still renders on the GPU,
# but it owns no display and steals no focus, so a session can drive it with xdotool
# while the desktop is in use. The knob is a file and not an environment variable
# because Steam spawns this wrapper with the Steam client's environment, so nothing
# exported by the caller reaches it.

here=$(dirname "$(realpath "$0")")

# Per-machine configuration: $here/launch.conf, one KEY=VALUE per line, BG3SE_* names and
# XDG_STATE_HOME only (ignored by git; written by hand for a play box).
# It exists for a box where the person playing is not the account that collects the
# logs: the player launches on one account, a staging account collects, so
# XDG_STATE_HOME points every log at a shared drop directory -- ours (the loader and the
# extender read it too, preload.cpp), and the game's own (`bin/bg3` reads XDG_STATE_HOME
# for `Larian Studios/Baldur's Gate 3/{logs,db}`, its gold logs and the Crashpad
# database). BG3SE_SO/BG3SE_EXT point at the staged objects. Unlike
# launch-env this is not one-shot: it describes the machine, not the next launch.
conf_state=
conf_kept=
conf_bad=
if [ -f "$here/launch.conf" ]; then
    while IFS= read -r line; do
        case $line in
            ''|\#*) ;;
            XDG_STATE_HOME=*) export "$line"; conf_state=${line#*=}; conf_kept="$conf_kept XDG_STATE_HOME" ;;
            BG3SE_[A-Z0-9_]*=*) export "$line"; conf_kept="$conf_kept ${line%%=*}" ;;
            *) conf_bad="$conf_bad | $line" ;;
        esac
    done < "$here/launch.conf"
fi

state=${XDG_STATE_HOME:-$HOME/.local/state}/bg3-se-linux
if [ -n "$conf_state" ]; then
    # Shared drop directory: the collector is another account, so nothing here is private.
    umask 022
    mkdir -p "$state"
    echo "$(date '+%Y-%m-%d %H:%M:%S') launch: launch.conf sets$conf_kept; state $state" >> "$state/bg3se.log"
    [ -n "$conf_bad" ] && echo "$(date '+%Y-%m-%d %H:%M:%S') launch: launch.conf ignores$conf_bad (BG3SE_* and XDG_STATE_HOME only)" >> "$state/bg3se.log"
    # Crashpad creates its database directories 0700 and the minidumps 0600 regardless of
    # the umask (db/completed drwx------, *.dmp -rw-------), and the console
    # socket needs write permission to connect to. So, as the launching user: open up
    # whatever the previous session left (a dump written seconds before an exit), then a
    # sidecar does the same every few seconds while the chain runs and once more after it
    # exits. It watches $$ -- this shell's pid, which exec below hands to the chain, so it
    # is gone exactly when the game is -- and runs in its own session (setsid) because the
    # headless test showed the sidecar dying with the chain: gamescope's reaper kills
    # its whole process group. Steam's steam-launch-wrapper and
    # reaper leave a background child alone (measured the same day). BG3SE_SHARE_INTERVAL is
    # the poll period, for the selftest.
    open_up() {
        [ -d "$1/Larian Studios" ] && chmod -R go+rX "$1/Larian Studios" 2>/dev/null
        chmod -R go+rX "$2" 2>/dev/null
        [ -S "$2/console.sock" ] && chmod a+rw "$2/console.sock" 2>/dev/null
        return 0
    }
    # The game's graphicSettings.lsx stays in the player's home (ls::Path root 4, not
    # XDG_STATE_HOME), out of the collector's reach; a copy before the launch and one after it
    # says what a session did to it (the resolution reset).
    snap_settings() {
        local f=${XDG_DATA_HOME:-$HOME/.local/share}/Larian\ Studios/Baldur\'s\ Gate\ 3/graphicSettings.lsx
        [ -f "$f" ] && mkdir -p "$1/settings" && cp -p "$f" "$1/settings/graphicSettings.$2.$3.lsx" 2>/dev/null
        return 0
    }
    launch_tag=$(date '+%Y%m%d-%H%M%S')
    snap_settings "$state" "$launch_tag" before
    open_up "$conf_state" "$state"
    setsid -f bash -c '
        chain=$1 root=$2 state=$3 every=$4 tag=$5
        '"$(declare -f open_up snap_settings)"'
        while kill -0 "$chain" 2>/dev/null; do sleep "$every"; open_up "$root" "$state"; done
        snap_settings "$state" "$tag" after
        open_up "$root" "$state"
        echo "$(date "+%Y-%m-%d %H:%M:%S") launch: chain $chain exited; shared state opened up" >> "$state/bg3se.log"
    ' sidecar "$$" "$conf_state" "$state" "${BG3SE_SHARE_INTERVAL:-10}" "$launch_tag" > /dev/null 2>&1 < /dev/null
else
    mkdir -p "$state" && chmod 700 "$state"
    [ -n "$conf_kept$conf_bad" ] && echo "$(date '+%Y-%m-%d %H:%M:%S') launch: launch.conf sets$conf_kept; ignores$conf_bad" >> "$state/bg3se.log"
fi

# One-shot environment for the next launch: $state/launch-env, one KEY=VALUE per line,
# BG3SE_* names only. Steam spawns this wrapper with the Steam client's environment, so
# a knob the caller wants (BG3SE_OSI, BG3SE_PROBE, BG3SE_NO_EXT) has nowhere else to go
# -- the same reason the headless flag is a file. Consumed like that flag, so it can
# never swallow a launch the player starts later; the file is kept as .used for the record.
if [ -f "$state/launch-env" ]; then
    kept=""
    while IFS= read -r line; do
        case $line in
            ''|\#*) ;;
            BG3SE_[A-Z0-9_]*=*) export "$line"; kept="$kept ${line%%=*}" ;;
            *) echo "$(date '+%Y-%m-%d %H:%M:%S') launch: launch-env ignores $line (BG3SE_* only)" >> "$state/bg3se.log" ;;
        esac
    done < "$state/launch-env"
    mv -f "$state/launch-env" "$state/launch-env.used"
    echo "$(date '+%Y-%m-%d %H:%M:%S') launch: env from launch-env:$kept" >> "$state/bg3se.log"
fi

so=${BG3SE_SO:-$here/../build/loader/libbg3se.so}
ext=${BG3SE_EXT:-$here/../build/ext/libbg3se-ext.so}

if [ -f "$so" ]; then
    so=$(realpath "$so")
    export LD_PRELOAD="${LD_PRELOAD:+$LD_PRELOAD:}$so"
    echo "$(date '+%Y-%m-%d %H:%M:%S') launch: preloading $so" >> "$state/bg3se.log"
    # the extender proper (ext/), dlopened by the loader at deferred init on a known build
    if [ -f "$ext" ] && [ -z "${BG3SE_NO_EXT:-}" ]; then
        export BG3SE_EXT=$(realpath "$ext")
        echo "$(date '+%Y-%m-%d %H:%M:%S') launch: extender $BG3SE_EXT" >> "$state/bg3se.log"
    else
        echo "$(date '+%Y-%m-%d %H:%M:%S') launch: no extender ($ext), loader only" >> "$state/bg3se.log"
    fi
else
    echo "$(date '+%Y-%m-%d %H:%M:%S') launch: $so not built, launching without it" >> "$state/bg3se.log"
fi

# The chain's own stdout/stderr, which Steam otherwise drops (headless.sh sends the
# Steam client's output to /dev/null, and a desktop launch buries it in Steam's log).
# Larian's LinuxCrashReporter writes its account of a crash here and nowhere else, so
# without this a crash the game catches leaves no trace at all.
# One file per launch, previous kept as .prev.
if [ -z "${BG3SE_NO_STDOUT_LOG:-}" ]; then
    gamelog=$state/game-stdout.log
    [ -f "$gamelog" ] && mv -f "$gamelog" "$gamelog.prev"
    exec >> "$gamelog" 2>&1
    echo "$(date '+%Y-%m-%d %H:%M:%S') launch: the chain's stdout and stderr land here"
fi

if [ -n "${BG3SE_PROBE:-}" ]; then
    set -- "$here/launch-probe.sh" "$@"
fi

# Nested compositor, no output. Geometry and frame cap come from the flag file
# ("W H R", one line); a cap keeps the game off the whole GPU while the desktop is
# in use. SDL is pinned to x11 because gamescope is started without
# --expose-wayland: its Xwayland is the surface the game gets, and XTEST reaches
# clients there (it does not under KWin).
if [ -f "$state/headless" ] && command -v gamescope > /dev/null; then
    read -r gs_w gs_h gs_r _ < "$state/headless"
    rm -f "$state/headless"     # one-shot: a stale flag must never swallow a launch the player started
    export SDL_VIDEODRIVER=x11
    # gamescope loads libpipewire-module-rt out of its PipeWire capture path, and that
    # module calls setrlimit(RLIMIT_RTTIME, {0,0}) -- per-process, so every descendant
    # including bin/bg3 inherits it, and the kernel SIGKILLs the game's WT/High threads
    # the first jiffy one spends in the RT class (an intermittent death at LoadMenu).
    # A hard limit cannot be raised again from below,
    # so the module must not load: pipewire/client.conf is /usr/share's file with
    # module.rt = false, reached only here. PIPEWIRE_CONFIG_DIR replaces the whole conf
    # search, so nothing else on the box sees it (measured: with it, zero RTTIME writes
    # and module-rt never opened; without it, one {0,0} write).
    if [ -d "$here/pipewire" ]; then
        export PIPEWIRE_CONFIG_DIR="$here/pipewire"
        echo "$(date '+%Y-%m-%d %H:%M:%S') launch: PIPEWIRE_CONFIG_DIR=$here/pipewire (no module-rt, keeps RLIMIT_RTTIME unlimited)" >> "$state/bg3se.log"
    else
        echo "$(date '+%Y-%m-%d %H:%M:%S') launch: WARNING no $here/pipewire -- gamescope will drop RLIMIT_RTTIME to 0 and the game may be SIGKILLed at LoadMenu" >> "$state/bg3se.log"
    fi
    echo "$(date '+%Y-%m-%d %H:%M:%S') launch: headless ${gs_w:-1920}x${gs_h:-1080}@${gs_r:-30}" >> "$state/bg3se.log"
    exec gamescope --backend headless \
        -W "${gs_w:-1920}" -H "${gs_h:-1080}" -r "${gs_r:-30}" \
        --force-windows-fullscreen -- "$@"
fi

exec "$@"
