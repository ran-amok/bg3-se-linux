#!/usr/bin/env python3
"""Enable and disable mods by editing the profile's modsettings.lsx, the way BG3MM does.

A written ModuleShortDesc entry loads a mod on a profile that has accepted Larian's mod
policy (config.lsf's one-time WasLarianModPolicyAccepted), and the game keeps the entry
byte for byte.

  modsettings.py list                  the entries, in load order
  modsettings.py enable PAK            copy PAK into Mods/ (if it is not there) and add its
                                       entry after the last one; the attributes come from
                                       the pak's own Mods/<Folder>/meta.lsx ModuleInfo
  modsettings.py disable UUID [--rm]   remove the entry (--rm also deletes the pak that
                                       carries that UUID from Mods/)
  modsettings.py --selftest            calibration on a scratch profile (below)

Every edit is guarded: it refuses while a `bg3` process
is up, copies the file to ~/.local/state/bg3-se-linux/modsettings/<YYYYmmdd-HHMM>.lsx first,
touches ModuleShortDesc entries and nothing else, and restores the copy if the result does
not parse. `--profile DIR` points at another profile root (the selftest uses it).

Remember a crash: a launch that finds the ModCrashSanityCheck breadcrumb a crashed one left
disables every mod without touching this file, and the launch after
that replaces the file with a pre-mod copy (older mtime, fresh ctime)
-- so after any crash, `list` before launching and `enable` again.
"""
import os
import re
import shutil
import subprocess
import sys
import tempfile
import time
import xml.etree.ElementTree as ET

sys.path.insert(0, os.path.dirname(os.path.realpath(__file__)))
import lspk  # noqa: E402

PROFILE = os.path.expanduser("~/.local/share/Larian Studios/Baldur's Gate 3")
BACKUPS = os.path.join(os.environ.get('XDG_STATE_HOME', os.path.expanduser('~/.local/state')),
                       'bg3-se-linux', 'modsettings')
ATTRS = ('Folder', 'MD5', 'Name', 'PublishHandle', 'UUID', 'Version64')
TYPES = dict(Folder='LSString', MD5='LSString', Name='LSString', PublishHandle='uint64',
             UUID='guid', Version64='int64')


def settings_path(profile):
    return os.path.join(profile, 'PlayerProfiles', 'Public', 'modsettings.lsx')


def pak_meta(pak):
    """ModuleInfo's attributes out of the pak's one Mods/<Folder>/meta.lsx."""
    with open(pak, 'rb') as f:
        _, entries = lspk.read_table(f)
        metas = [e['name'] for e in entries if re.fullmatch(r'Mods/[^/]+/meta\.lsx', e['name'])]
        if len(metas) != 1:
            raise ValueError('%s: %d Mods/*/meta.lsx members, need exactly one' % (pak, len(metas)))
        root = ET.fromstring(lspk.extract(f, entries, metas[0]))
    info = root.find(".//node[@id='ModuleInfo']")
    if info is None:
        raise ValueError('%s: no ModuleInfo node in %s' % (pak, metas[0]))
    got = {a.get('id'): a.get('value') for a in info.findall('attribute')}
    missing = [k for k in ('Folder', 'Name', 'UUID', 'Version64') if not got.get(k)]
    if missing:
        raise ValueError('%s: ModuleInfo lacks %s' % (pak, ', '.join(missing)))
    return dict(Folder=got['Folder'], MD5='', Name=got['Name'], PublishHandle='0',
                UUID=got['UUID'], Version64=got['Version64'])


def entries(text):
    root = ET.fromstring(text)
    mods = root.find(".//node[@id='Mods']/children")
    return [{a.get('id'): a.get('value') for a in n.findall('attribute')}
            for n in (mods if mods is not None else [])]


def game_running():
    return subprocess.run(['pgrep', '-x', 'bg3'], capture_output=True).returncode == 0


def backup(path):
    os.makedirs(BACKUPS, exist_ok=True)
    stem = os.path.join(BACKUPS, time.strftime('%Y%m%d-%H%M'))
    dst, n = stem + '.lsx', 1
    while os.path.exists(dst):
        n += 1
        dst = '%s-%d.lsx' % (stem, n)
    shutil.copy2(path, dst)
    return dst


def write_checked(path, text, saved):
    with open(path, 'w') as f:
        f.write(text)
    try:
        ET.parse(path)
    except ET.ParseError as e:
        shutil.copy2(saved, path)
        raise SystemExit('%s did not parse (%s); restored %s' % (path, e, saved))


def add_entry(text, meta):
    m = re.search(r'<node id="Mods">\s*<children>.*?\n([ \t]*)</children>', text, re.S)
    if not m:
        raise ValueError('no <node id="Mods"><children> in modsettings.lsx')
    ind = m.group(1) + '    '
    block = ind + '<node id="ModuleShortDesc">\n'
    for k in ATTRS:
        block += '%s    <attribute id="%s" type="%s" value="%s"/>\n' % (ind, k, TYPES[k], meta[k])
    block += ind + '</node>\n'
    at = m.start(1)
    return text[:at] + block + text[at:]


def remove_entry(text, uuid):
    return re.sub(r'\n[ \t]*<node id="ModuleShortDesc">(?:(?!</node>).)*?' + re.escape(uuid)
                  + r'(?:(?!</node>).)*?</node>', '', text, flags=re.S)


def enable(pak, profile, check_game=True):
    if check_game and game_running():
        raise SystemExit('bg3 is running; modsettings.lsx is not edited under a live game')
    meta = pak_meta(pak)
    mods = os.path.join(profile, 'Mods')
    dst = os.path.join(mods, os.path.basename(pak))
    if os.path.realpath(pak) != os.path.realpath(dst):
        os.makedirs(mods, exist_ok=True)
        shutil.copy2(pak, dst)
        print('copied %s' % dst)
    path = settings_path(profile)
    text = open(path).read()
    if any(e.get('UUID') == meta['UUID'] for e in entries(text)):
        print('already listed: %s %s' % (meta['Folder'], meta['UUID']))
        return None
    saved = backup(path)
    write_checked(path, add_entry(text, meta), saved)
    print('enabled %s (%s, Version64 %s); backup %s' % (meta['Folder'], meta['UUID'], meta['Version64'], saved))
    return saved


def disable(uuid, profile, rm=False, check_game=True):
    if check_game and game_running():
        raise SystemExit('bg3 is running; modsettings.lsx is not edited under a live game')
    path = settings_path(profile)
    text = open(path).read()
    saved = None
    if any(e.get('UUID') == uuid for e in entries(text)):
        saved = backup(path)
        write_checked(path, remove_entry(text, uuid), saved)
        print('disabled %s; backup %s' % (uuid, saved))
    else:
        print('not listed: %s' % uuid)
    if rm:
        mods = os.path.join(profile, 'Mods')
        for n in sorted(os.listdir(mods)) if os.path.isdir(mods) else []:
            p = os.path.join(mods, n)
            try:
                if n.endswith('.pak') and pak_meta(p)['UUID'] == uuid:
                    os.remove(p)
                    print('removed %s' % p)
            except ValueError:
                pass
    return saved


# A modsettings.lsx in the game's own layout (written by the in-game manager), and the entry
# the game accepts when Mod Configuration Menu 1.41.0.0 is added. Given that version's pak in
# BG3_MCM_PAK, the selftest requires `enable` to produce that accepted edit byte for byte.
BASE = '''<?xml version="1.0" encoding="UTF-8"?>
<save>
    <version major="4" minor="8" revision="0" build="700"/>
    <region id="ModuleSettings">
        <node id="root">
            <children>
                <node id="Mods">
                    <children>
                        <node id="ModuleShortDesc">
                            <attribute id="Folder" type="LSString" value="GustavX"/>
                            <attribute id="MD5" type="LSString" value=""/>
                            <attribute id="Name" type="LSString" value="GustavX"/>
                            <attribute id="PublishHandle" type="uint64" value="0"/>
                            <attribute id="UUID" type="guid" value="cb555efe-2d9e-131f-8195-a89329d218ea"/>
                            <attribute id="Version64" type="int64" value="36028797018963968"/>
                        </node>
                    </children>
                </node>
            </children>
        </node>
    </region>
</save>
'''
MCM_ENTRY = '''                        <node id="ModuleShortDesc">
                            <attribute id="Folder" type="LSString" value="BG3MCM"/>
                            <attribute id="MD5" type="LSString" value=""/>
                            <attribute id="Name" type="LSString" value="Mod Configuration Menu"/>
                            <attribute id="PublishHandle" type="uint64" value="0"/>
                            <attribute id="UUID" type="guid" value="755a8a72-407f-4f0d-9a33-274ac0f0b53d"/>
                            <attribute id="Version64" type="int64" value="41799034041532416"/>
                        </node>
'''
MCM_PAK = os.environ.get('BG3_MCM_PAK', '')


def selftest():
    global BACKUPS
    fails = []

    def check(ok, what):
        print('  %s  %s' % ('ok  ' if ok else 'FAIL', what))
        if not ok:
            fails.append(what)

    with tempfile.TemporaryDirectory() as tmp:
        BACKUPS = os.path.join(tmp, 'backups')
        prof = os.path.join(tmp, 'profile')
        os.makedirs(os.path.join(prof, 'PlayerProfiles', 'Public'))
        path = settings_path(prof)
        open(path, 'w').write(BASE)
        # a mod of our own, so the test needs nothing fetched
        src = os.path.join(tmp, 'src', 'Mods', 'SelfTest')
        os.makedirs(src)
        open(os.path.join(src, 'meta.lsx'), 'w').write(
            '<save><region id="Config"><node id="root"><children><node id="ModuleInfo">'
            '<attribute id="Folder" type="LSString" value="SelfTest"/>'
            '<attribute id="Name" type="LSString" value="Self Test"/>'
            '<attribute id="UUID" type="FixedString" value="00000000-1111-2222-3333-444444444444"/>'
            '<attribute id="Version64" type="int64" value="36028797018963968"/>'
            '<children><node id="PublishVersion"><attribute id="Version64" type="int64" value="1"/>'
            '</node></children></node></children></node></region></save>')
        own = os.path.join(tmp, 'SelfTest.pak')
        lspk.pack(own, os.path.join(tmp, 'src'))

        if MCM_PAK and os.path.exists(MCM_PAK):
            enable(MCM_PAK, prof, check_game=False)
            want = BASE.replace('                    </children>\n                </node>\n            </children>',
                                MCM_ENTRY + '                    </children>\n                </node>\n            </children>', 1)
            check(open(path).read() == want, 'enable(MCM) reproduces the edit the game accepted, byte for byte')
            check(os.path.exists(os.path.join(prof, 'Mods', os.path.basename(MCM_PAK))), 'the pak is copied into Mods/')
            disable('755a8a72-407f-4f0d-9a33-274ac0f0b53d', prof, rm=True, check_game=False)
            check(open(path).read() == BASE, 'disable restores the file byte for byte')
            check(not os.listdir(os.path.join(prof, 'Mods')), '--rm deletes the pak carrying that UUID')
        else:
            print('  skip  BG3_MCM_PAK unset or missing (Mod Configuration Menu 1.41.0.0); byte-for-byte check not run')

        enable(own, prof, check_game=False)
        e = entries(open(path).read())
        check([x['Folder'] for x in e] == ['GustavX', 'SelfTest'], 'enable appends after the last entry')
        check(e[-1]['Version64'] == '36028797018963968',
              "Version64 is ModuleInfo's own, not a child node's")
        n = len(os.listdir(BACKUPS))
        check(enable(own, prof, check_game=False) is None and len(entries(open(path).read())) == 2
              and len(os.listdir(BACKUPS)) == n, 'enabling twice adds nothing and backs up nothing')
        check(n >= 1 and all(f.endswith('.lsx') for f in os.listdir(BACKUPS)), 'every edit left a backup')

        # planted: a pak with no meta.lsx is refused, and the file is untouched
        os.makedirs(os.path.join(tmp, 'bare', 'Public'))
        open(os.path.join(tmp, 'bare', 'Public', 'x.txt'), 'w').write('x')
        bare = os.path.join(tmp, 'bare.pak')
        lspk.pack(bare, os.path.join(tmp, 'bare'))
        before = open(path).read()
        try:
            enable(bare, prof, check_game=False)
            check(False, 'a pak without meta.lsx is refused')
        except ValueError:
            check(open(path).read() == before, 'a pak without meta.lsx is refused, file untouched')
        # planted: an unparseable result is restored from the backup
        saved = backup(path)
        try:
            write_checked(path, before.replace('</save>', ''), saved)
            check(False, 'an unparseable write is restored')
        except SystemExit:
            check(open(path).read() == before, 'an unparseable write is restored from the backup')

    print('selftest: %s' % ('FAILED (%d)' % len(fails) if fails else 'ok'))
    return 1 if fails else 0


def main():
    args = sys.argv[1:]
    profile = PROFILE
    if '--profile' in args:
        i = args.index('--profile')
        profile = args[i + 1]
        del args[i:i + 2]
    if args == ['--selftest']:
        return selftest()
    if args == ['list']:
        for e in entries(open(settings_path(profile)).read()):
            print('%-24s %s  %s' % (e.get('Folder'), e.get('UUID'), e.get('Version64')))
        return 0
    if len(args) == 2 and args[0] == 'enable':
        enable(args[1], profile)
        return 0
    if len(args) in (2, 3) and args[0] == 'disable' and args[2:] in ([], ['--rm']):
        disable(args[1], profile, rm=args[2:] == ['--rm'])
        return 0
    print(__doc__.strip(), file=sys.stderr)
    return 2


if __name__ == '__main__':
    sys.exit(main())
