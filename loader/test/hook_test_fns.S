# Prologues for hook_test.cpp, one per case the engine relocates or refuses.
# Each takes x in rdi and returns a value in rax the test knows in advance.
        .text
        .globl fn_plain, fn_riprel, fn_jcc8, fn_jmp32, fn_call, fn_cx16
        .globl fn_short, fn_internal, fn_jrcxz, fn_unaligned

        .p2align 4
fn_plain:                       # push; mov; lea: three instructions displaced -> x + 1
        push %rbp
        mov %rsp, %rbp
        lea 1(%rdi), %rax
        pop %rbp
        ret

        .p2align 4
fn_riprel:                      # rip-relative load in the patch -> x + 1000
        mov value(%rip), %rax
        add %rdi, %rax
        ret

        .p2align 4
fn_jcc8:                        # jcc rel8 in the patch, widened -> x ? x + 2 : -1
        test %rdi, %rdi
        je 1f
        lea 2(%rdi), %rax
        ret
1:      mov $-1, %rax
        ret

        .p2align 4
fn_jmp32:                       # starts with jmp rel32 -> x + 3
        .byte 0xe9
        .long 1f - (. + 4)
        ud2
1:      lea 3(%rdi), %rax
        ret

        .p2align 4
fn_call:                        # call rel32 in the patch -> x + 5
        push %rbx
        call helper
        pop %rbx
        ret
helper:
        lea 5(%rdi), %rax
        ret

        .p2align 4
        .skip 6
fn_cx16:                        # 5 bytes at +6: fits no 8-byte word, one 16-byte word -> x + 6
        mov $6, %eax                    # 5 bytes: no instruction boundary inside the patch
        add %rdi, %rax
        ret

        .p2align 4
fn_short:                       # ret first: refused (too_short)
        ret
        .skip 8, 0xcc

        .p2align 4
fn_internal:                    # loops back into its own first bytes: refused
1:      dec %rdi
        jnz 1b
        mov $9, %eax
        ret

        .p2align 4
fn_jrcxz:                       # jrcxz in the patch: refused (not_relocatable)
        mov %rdi, %rcx
        jrcxz 1f
        mov $1, %eax
1:      ret

        .p2align 4
        .skip 13
fn_unaligned:                   # 5 bytes at +13 cross a 16-byte word: refused
        lea 7(%rdi), %rax
        nop
        ret

        .data
        .p2align 3
value:  .quad 1000
        .section .note.GNU-stack,"",@progbits
