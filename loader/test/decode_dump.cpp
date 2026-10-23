// decode-dump: the hook decoder over records from tools/hook-decode-check.py.
// stdin: records of (u64 address, u32 length, bytes). stdout, one line per
// instruction, decoded linearly through each record:
//   address  length  kind  disp_off  target      or      address  FAIL
// With --probe, one line per record instead: address  status (hook::probe, where
// a record is one whole function).
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <vector>

#include "hook.h"

int main(int argc, char** argv)
{
    bool probe = argc > 1 && !strcmp(argv[1], "--probe");
    uint64_t addr;
    uint32_t len;
    std::vector<uint8_t> buf;
    while (fread(&addr, 8, 1, stdin) == 1 && fread(&len, 4, 1, stdin) == 1) {
        buf.assign(len + 16, 0xCC);   // padding: a decoder may read past the end
        if (fread(buf.data(), 1, len, stdin) != len) return 1;
        if (probe) {
            printf("%lx\t%s\n", (unsigned long)addr,
                   bg3se::hook::describe(bg3se::hook::probe(buf.data(), len, addr)));
            continue;
        }
        uint32_t o = 0;
        while (o < len) {
            bg3se::hook::Insn x;
            if (!bg3se::hook::decode(buf.data() + o, addr + o, &x)) {
                printf("%lx\tFAIL\n", (unsigned long)(addr + o));
                break;
            }
            if (o + x.len > len) break;   // runs past the record: capstone cannot say either
            printf("%lx\t%u\t%u\t%d\t%lx\n", (unsigned long)(addr + o), x.len, (unsigned)x.kind,
                   x.disp_off, (unsigned long)x.target);
            o += x.len;
        }
    }
    return 0;
}
