// Copyright (c) 2016-2017 Andrew Kallmeyer <fsmv@sapium.net>
// Provided under the MIT License: https://mit-license.org

// Thanks to Franc[e]sco (weeb.ddns.net) for his nolibc projects which I used
// as an example for this.

#include <sys/syscall.h>

.intel_syntax noprefix
.globl _start
.globl syscall0, syscall1, syscall2, syscall3, syscall4, syscall5, syscall6
.text
#if !defined(NO_START)
    _start:
        xor ebp, ebp       // mark the base stack frame
        pop esi
        mov ecx,esp
        push ecx
        push esi
        call main
        mov ebx, eax       // move retern value into arg1 of syscall
        mov eax, SYS_exit  // calling exit()
        int 0x80           // syscall
#endif

    syscall0:
        mov eax, [esp+4] // syscall num
        int 0x80
        ret

    syscall1:
        push ebx           // callee save
        mov eax, [esp+4+4] // syscall num
        mov ebx, [esp+8+4] // arg1
        int 0x80
        pop ebx
        ret

    syscall2:
        push ebx            // callee save
        mov eax, [esp+ 4+4] // syscall num
        mov ebx, [esp+ 8+4] // arg 1
        mov ecx, [esp+12+4] // arg 2
        int 0x80
        pop ebx
        ret

    syscall3:
        push ebx            // callee save
        mov eax, [esp+ 4+4] // syscall num
        mov ebx, [esp+ 8+4] // arg 1
        mov ecx, [esp+12+4] // arg 2
        mov edx, [esp+16+4] // arg 3
        int 0x80
        pop ebx
        ret

    syscall4:
        push ebx            // callee save
        push esi            // callee save
        mov eax, [esp+ 4+8] // syscall num
        mov ebx, [esp+ 8+8] // arg 1
        mov ecx, [esp+12+8] // arg 2
        mov edx, [esp+16+8] // arg 3
        mov esi, [esp+20+8] // arg 4
        int 0x80
        pop esi
        pop ebx
        ret

    syscall5:
        push ebx             // callee save
        push esi             // callee save
        push edi             // callee save
        mov eax, [esp+ 4+12] // syscall num
        mov ebx, [esp+ 8+12] // arg 1
        mov ecx, [esp+12+12] // arg 2
        mov edx, [esp+16+12] // arg 3
        mov esi, [esp+20+12] // arg 4
        mov edi, [esp+24+12] // arg 5
        int 0x80
        pop edi
        pop esi
        pop ebx
        ret

    syscall6:
        push ebx             // callee save
        push esi             // callee save
        push edi             // callee save
        push ebp             // callee save
        mov eax, [esp+ 4+16] // syscall num
        mov ebx, [esp+ 8+16] // arg 1
        mov ecx, [esp+12+16] // arg 2
        mov edx, [esp+16+16] // arg 3
        mov esi, [esp+20+16] // arg 4
        mov edi, [esp+24+16] // arg 5
        mov ebp, [esp+28+16] // arg 6
        int 0x80
        pop ebp
        pop edi
        pop esi
        pop ebx
        ret
