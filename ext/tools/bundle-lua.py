#!/usr/bin/env python3
"""Packs a directory of Lua scripts into upstream's resource bundle (ResourceBundler.cpp):
a sequence of {uint32 FileNameSize, uint32 FileSize, name, body}, read by
Lua/Shared/LuaBundle.cpp. On Windows the bundle is a PE resource; here it is linked in
(ext/linux/lua_bundle.S).

  bundle-lua.py SRC_DIR OUT_FILE [OUT_S]

OUT_S, if given, is an assembly source that .incbin's OUT_FILE, written with the
bundle's hash in a comment: zig caches compiled objects by source text and command
line, not by what .incbin pulled in, so the source has to change when the bundle does.
"""
import hashlib
import os
import struct
import sys


def main():
    src, out = sys.argv[1], sys.argv[2]
    parts = []
    for root, dirs, files in os.walk(src):
        dirs.sort()
        for f in sorted(files):
            p = os.path.join(root, f)
            rel = os.path.relpath(p, src).replace(os.sep, '/')
            body = open(p, 'rb').read()
            name = rel.encode()
            parts.append(struct.pack('<II', len(name), len(body)) + name + body)
    blob = b''.join(parts)
    tmp = out + '.tmp'
    with open(tmp, 'wb') as f:
        f.write(blob)
    os.replace(tmp, out)
    print('%s: %d files, %d bytes' % (out, len(parts), len(blob)))
    if len(sys.argv) > 3:
        sha = hashlib.sha1(blob).hexdigest()
        asm = ('/* The builtin Lua scripts (BG3Extender/LuaScripts), packed by ext/tools/bundle-lua.py;\n'
               '   linked in where Windows uses a PE resource, read by Lua/Shared/LuaBundle.cpp.\n'
               '   Generated: bundle sha1 %s */\n'
               '    .section .rodata\n    .globl bg3se_lua_bundle\n    .globl bg3se_lua_bundle_end\n    .balign 16\n'
               'bg3se_lua_bundle:\n    .incbin "%s"\nbg3se_lua_bundle_end:\n    .byte 0\n'
               '    .section .note.GNU-stack,"",@progbits\n') % (sha, out)
        with open(sys.argv[3] + '.tmp', 'w') as f:
            f.write(asm)
        os.replace(sys.argv[3] + '.tmp', sys.argv[3])


if __name__ == '__main__':
    main()
