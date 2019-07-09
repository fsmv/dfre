// Copyright (c) 2016-2017 Andrew Kallmeyer <fsmv@sapium.net>
// Provided under the MIT License: https://mit-license.org

#include <sys/syscall.h>

// Note that clang will insert the underscores into our function names
// https://stackoverflow.com/a/5908708

.intel_syntax noprefix
.globl start
.globl _syscall0, _syscall1, _syscall2, _syscall3, _syscall4, _syscall5, _syscall6
.text
#if !defined(NO_START)
    start:
        xor ebp, ebp       // mark the base stack frame
        pop esi
        mov ecx,esp
        push ecx
        push esi
        call _main
        mov ebx, eax       // move retern value into arg1 of syscall
        mov eax, SYS_exit  // calling exit()
        int 0x80           // syscall
#endif

    // OSX, *BSD use C calling convention for syscalls
    // with 4 byte offset from stack arguments
    _syscall0:
    _syscall1:
    _syscall2:
    _syscall3:
    _syscall4:
    _syscall5:
    _syscall6:
        mov eax, [esp+4] // syscall num
        add esp, 4
        int 0x80
        sub esp, 4
        ret
