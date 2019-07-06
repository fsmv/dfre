// Copyright (c) 2016-2017 Andrew Kallmeyer <fsmv@sapium.net>
// Provided under the MIT License: https://mit-license.org

#include <stddef.h>
#include <stdint.h>
#include <sys/syscall.h>
#include <sys/types.h>

// FIXME: Currently inline calls aren't linking properly
// when using -nostdlib, -mmacosx-version-min=10.6. Removing
// the inline specifier for their declarations is a temporary
// workaround
#define inline

#define IsError(err) ((uint32_t)(err) > (uint32_t)-4096)
#define Errno(err) (-(int32_t)(err))

extern "C" {
    uint32_t syscall1(uint32_t call, void*);
    uint32_t syscall2(uint32_t call, void*, void*);
    uint32_t syscall3(uint32_t call, void*, void*, void*);
    uint32_t syscall4(uint32_t call, void*, void*, void*, void*);
    uint32_t syscall5(uint32_t call, void*, void*, void*, void*, void*);
    uint32_t syscall6(uint32_t call, void*, void*, void*, void*, void*, void*);

    inline void exit(int retcode) {
        syscall1(SYS_exit, (void*)retcode);
        __builtin_unreachable();
    }

    inline int32_t write(int fd, const void *buf, size_t length) {
        return syscall3(SYS_write, (void*)fd, (void*)buf, (void*)length);
    }

    inline int munmap(void *addr, size_t length) {
        return (int)syscall2(SYS_munmap, (void*)addr, (void*)length);
    }

    inline void *mmap(void *addr, size_t length, int prot,
                      int flags, int fd, off_t offset) {
        return (void*)syscall6(SYS_mmap, addr, (void*)length, (void*)prot,
                               (void*)flags, (void*)fd, (void*)offset);
    }

    inline int mprotect(void *addr, size_t length, int prot) {
        return (int)syscall3(SYS_mprotect, (void*)addr, (void*)length,
                             (void*)prot);
    }

    void* memset(void *addr, int val, size_t length) {
        unsigned char *d = (unsigned char*)addr;

        for (; length; length--, d++)
            *d = val;

        return addr;
    }

    void* memcpy(void *dest, const void *src, size_t length) {
        unsigned char *d = (unsigned char*)dest;
        const unsigned char *s = (const unsigned char*)src;

        for (; length; length--)
            *d++ = *s++;

        return dest;
    }
}
