// Copyright (c) 2016-2019 Andrew Kallmeyer <fsmv@sapium.net>
// Provided under the MIT License: https://mit-license.org

#include "platform.h"

#include <sys/syscall.h>
#include <sys/types.h>

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
#if defined(DFRE_NIX32)
        // Actually takes a pointer to the arguments on the stack for some reason
        return (void*)syscall1(SYS_mmap, (void*)&addr);
#elif defined(DFRE_OSX32)
        return (void*)syscall6(SYS_mmap, addr, (void*)length, (void*)prot,
                               (void*)flags, (void*)fd, (void*)offset);
#endif
    }

    inline int mprotect(void *addr, size_t length, int prot) {
        return (int)syscall3(SYS_mprotect, (void*)addr, (void*)length,
                             (void*)prot);
    }
}

inline uint32_t Write(const char *Str, size_t Len) {
    return write(1, Str, Len);
}

inline void Exit(int Code) {
    exit(Code);
}

#include "utils.h" // MemCopy

// Clang puts in calls to these and there's apparently no way to turn it off
#if defined(DFRE_OSX32)
extern "C" {
    void* memset(void *addr, int val, size_t length) {
        uint8_t *d = (uint8_t*)addr;
        for (; length; length--, d++) {
            *d = val;
        }
        return addr;
    }

    void* memcpy(void *dest, const void *src, size_t length) {
        MemCopy(dest, src, length);
        return dest;
    }
}
#endif

#include <sys/mman.h> // for mmap flag constants
#include "print.h"

// TODO: call the function `sysconf(_SC_PAGE_SIZE)` to get page size
// Have to link glibc, not sure exactly how it works in OSX though.
// I think the only platform where it might change is ARM64.
const size_t PAGE_SIZE = (4 * 1024);
const size_t RESERVE_GRANULARITY = PAGE_SIZE;

void *LoadCode(uint8_t *Code, size_t CodeWritten) {
    void *CodeExe = mmap(0, CodeWritten, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (IsError(CodeExe)) {
        Print("Failed to load code. errno = %u\n", Errno(CodeExe));
        return 0;
    }
    MemCopy(CodeExe, Code, CodeWritten);
    // TODO: check error
    mprotect(CodeExe, CodeWritten, PROT_EXEC | PROT_READ);
    return CodeExe;
}

void *Reserve(void *addr, size_t size) {
    // > [man 2 mmap]
    // > MAP_PRIVATE
    // >     Create a private copy-on-write mapping.
    // > ...
    // > MAP_ANONYMOUS
    // >    The mapping is not backed by any file; its contents are
    // >    initialized to zero.  The fd argument is ignored; however,
    // >    some implementations require fd to be -1 if MAP_ANONYMOUS (or
    // >    MAP_ANON) is specified, and portable applications should
    // >    ensure this.  The offset argument should be zero.
    //
    // Because mappings are copy on write (meaning the kernel doesn't actually
    // find physical memoconstry for the page until you write), disallowing writes
    // with PROT_NONE prevents the reserved pages from taking up space.
    void *Ret = mmap(addr, size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (IsError(Ret)) {
        Print("Failed to reserve memory (%u bytes). errno = %u\n",
              size, Errno(Ret));
        return 0;
    }
    return Ret;
}

bool Commit(void *addr, size_t size) {
    // On linux, this is just allowing read and write. This is because virtual
    // memory in linux is copy on write. That means technically the memory doesn't
    // actually get committed until it gets written.
    //
    // Also, as mentioned under MAP_ANONYMOUS, this data is initialized to zero
    int err = mprotect(addr, size, PROT_READ | PROT_WRITE);
    if (IsError(err)) {
        Print("Failed to commit memory (%u bytes). errno = %u\n",
              size, Errno(err));
        return false;
    }
    return true;
}

void Free(void *addr, size_t size) {
    // > [man 2 munmap]
    // > The address addr must be a multiple of the page size (but length need
    // > not be). All pages containing a part of the indicated range are
    // > unmapped, and subsequent references to these pages will generate
    // > SIGSEGV. It is not an error if the indicated range does not contain any
    // > mapped pages.
    int err = munmap(addr, size);
    if (IsError(err)) {
        Print("Failed to free memory (%u bytes). errno = %u\n",
              size, Errno(err));
    }
}

