// Copyright (c) 2016-2017 Andrew Kallmeyer <fsmv@sapium.net>
// Provided under the MIT License: https://mit-license.org

#include <stddef.h>
#include <stdint.h>
#include <sys/syscall.h>
#include <sys/types.h>

#define IsError(err) ((uint32_t)(err) > (uint32_t)-4096)
#define Errno(err) (-(int32_t)(err))

extern "C" {
    uint32_t syscall1(uint32_t call, void *arg1);
    uint32_t syscall2(uint32_t call, void *arg1, void *arg2);
    uint32_t syscall3(uint32_t call, void *arg1, void *arg2, void *arg3);

    inline void exit(int retcode) {
        syscall1(SYS_exit, (void*)retcode);
        __builtin_unreachable();
    }

    inline uint32_t write(int fd, const void *buf, size_t length) {
        return syscall3(SYS_write, (void*)fd, (void*)buf, (void*)length);
    }

    inline int munmap(void *addr, size_t length) {
        return (int)syscall2(SYS_write, (void*)addr, (void*)length);
    }
    inline void *mmap(void *addr, size_t length, int prot,
                      int flags, int fd, off_t offset) {
        // Actually takes a pointer to the arguments on the stack
        return (void*)syscall1(SYS_mmap, (void*)&addr);
    }
    inline int mprotect(void *addr, size_t length, int prot) {
        return (int)syscall3(SYS_mprotect, (void*)addr, (void*)length,
                             (void*)prot);
    }
}

#define Write(str, len) write(1, (str), (len))
#define Exit(code) exit((code))

#include <sys/mman.h> // for flag constants
#include "nix32_mem_arena.cpp"

void *LoadCode(uint8_t *Code, size_t CodeWritten) {
    void *CodeExe = mmap(0, CodeWritten, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    // TODO: Report error
    if (IsError(CodeExe)) {
        Print("Could not reserve memory for code. errno = %u\n", Errno(CodeExe));
    }
    MemCopy(CodeExe, Code, CodeWritten);
    mprotect(CodeExe, CodeWritten, PROT_EXEC | PROT_READ);
    return CodeExe;
}
