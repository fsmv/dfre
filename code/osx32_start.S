// Copyright (c) 2016-2017 Andrew Kallmeyer <fsmv@sapium.net>
// Provided under the MIT License: https://mit-license.org

// Thanks to Franc[e]sco (weeb.ddns.net) for his nolibc projects which I used
// as an example for this.

#include <sys/syscall.h>

.intel_syntax noprefix
.globl _start
.globl syscall0, syscall1, syscall2, syscall3, syscall4, syscall5, syscall6
.text
    _start:
        xor ebp, ebp       // mark the base stack frame
        pop esi
        mov ecx,esp
        push ecx
        push esi
        call _main
        mov ebx, eax       // move retern value into arg1 of syscall
        mov eax, SYS_exit  // calling exit()
        int 0x80           // syscall

    // OSX, *BSD use C calling convention for syscalls
    // with 4 byte offset from stack arguments
    syscall0:
    syscall1:
    syscall2:
    syscall3:
    syscall4:
    syscall5:
    syscall6:
        mov eax, [esp+4] // syscall num
        add esp, 4
        int 0x80
        sub esp, 4
        ret
