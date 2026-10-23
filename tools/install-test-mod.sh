#!/bin/bash
# Installs upstream's SampleMod (an unmodified Script Extender mod: BootstrapServer.lua
# prints a warning and registers two Osiris listeners) as the Phase 5 exit test: packs it
# into the profile's Mods folder and enables it with a modsettings.lsx entry, through
# tools/modsettings.py (backup first, refuses while the game runs). A written entry loads on
# this profile once config.lsf's one-time WasLarianModPolicyAccepted is set.
# --remove deletes the entry and the pak.
# Nothing here touches the game install.
#
# tools/test-mod-overlay/ is copied over upstream's tree before packing: it adds
# a BootstrapClient.lua, the client half of osi-sweep.sh's net-* probes. Upstream's own files
# are not replaced -- the overlay may only add.
#
#   UPSTREAM=<upstream bg3se checkout> tools/install-test-mod.sh [--remove]
set -eu
here=$(dirname "$(realpath "$0")")
upstream=${UPSTREAM:?set UPSTREAM to a checkout of github.com/Norbyte/bg3se}
src=$upstream/SampleMod
overlay=$here/test-mod-overlay
prof="$HOME/.local/share/Larian Studios/Baldur's Gate 3"
mods="$prof/Mods"
settings="$prof/PlayerProfiles/Public/modsettings.lsx"
pak="$mods/ExtenderSampleMod.pak"
uuid=787ce468-859f-4e07-83e2-61c31139e1bc
[ -f "$settings" ] || { echo "no $settings" >&2; exit 2; }
[ -d "$src/Mods/ExtenderSampleMod" ] || { echo "no SampleMod at $src" >&2; exit 2; }

if [ "${1:-}" = --remove ]; then
    python3 "$here/modsettings.py" disable "$uuid" --rm
    exit 0
fi

mkdir -p "$mods"
stage=$(mktemp -d)
trap 'rm -rf "$stage"' EXIT
cp -r "$src/." "$stage/"
if [ -d "$overlay" ]; then
    while IFS= read -r f; do
        rel=${f#"$overlay"/}
        if [ -e "$src/$rel" ]; then echo "overlay would replace upstream's $rel; refusing" >&2; exit 2; fi
        mkdir -p "$stage/$(dirname "$rel")"
        cp "$f" "$stage/$rel"
    done < <(find "$overlay" -type f)
fi
python3 "$here/lspk.py" pack "$pak" "$stage"
python3 "$here/lspk.py" list "$pak" | sed 's/^/  /'
python3 "$here/modsettings.py" enable "$pak"
echo "installed $pak"
echo "now: launch; look for 'testing???' in ~/.local/state/bg3-se-linux/extender.log"
