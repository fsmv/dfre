// Copyright (c) 2016-2019 Andrew Kallmeyer <fsmv@sapium.net>
// Provided under the MIT License: https://mit-license.org

#define Write(str, len) write(1, (str), (len))
#define Exit(code) exit((code))

#include <sys/mman.h> // for mmap flag constants
#include "print.h"

// TODO: call the function `sysconf(_SC_PAGE_SIZE)` to get page size
// Have to link glibc, not sure exactly how it works in OSX though.
// I think the only platform where it might change is ARM64.
const size_t PAGE_SIZE = (4 * 1024);
const size_t RESERVE_GRANULARITY = PAGE_SIZE;

// See mem_arena.h for documentation
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

// See mem_arena.h for documentation
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

// See mem_arena.h for documentation
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

#include "mem_arena.h"

// See mem_arena.h for documentation
//
// POSIX version allows us to sometimes avoid a copy if we can just reserve the
// space right after what we already have.
bool Expand(mem_arena *Arena) {
    const size_t AmountToCommit = PAGE_SIZE;
    size_t NewCommitted = Arena->Committed + AmountToCommit;
    if (NewCommitted > Arena->Reserved) {
        // Try to reserve the reserved size after the existing pointer
        size_t NewReserved = Arena->Reserved * 2;
        void *AppendAddr = (void*)(Arena->Base + Arena->Reserved);
        void *AppendedReserve = Reserve(AppendAddr, Arena->Reserved);
        if (AppendedReserve == AppendAddr) {
            Arena->Reserved = NewReserved;
            // Fallthrough to the code to commit one more page
        } else { // Could not append reserved pages, must do a copy
            if (AppendedReserve) {
                // [man 2 mmap]
                // > If addr is not NULL, then the kernel takes it as a hint
                // > about where to place the mapping; on Linux, the mapping
                // > will be created at a nearby page boundary.
                //
                // It's possible we got a reservation but not at the exact
                // address we wanted. So free it because we can't use it.
                Free(AppendedReserve, Arena->Reserved);
            }
            uint8_t *NewBase = (uint8_t*)Reserve(0, NewReserved);
            if (!NewBase) {
                return false;
            }
            if (!Commit(NewBase, NewCommitted)) {
                Free(NewBase, NewReserved);
                return false;
            }
            MemCopy(NewBase, Arena->Base, Arena->Used);
            Free(Arena->Base, Arena->Reserved);
            Arena->Base = NewBase;
            Arena->Reserved = NewReserved;
            Arena->Committed = NewCommitted;
            return true; // Saved a syscall by doing the whole commit in one go
        }
    }
    // We have room, just commit the next page
    if (!Commit((Arena->Base + Arena->Committed), AmountToCommit)) {
        return false;
    }
    Arena->Committed = NewCommitted;
    return true;
}

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
