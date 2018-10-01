// Copyright (c) 2016-2017 Andrew Kallmeyer <fsmv@sapium.net>
// Provided under the MIT License: https://mit-license.org

#include "mem_arena.h"
#include "print.h"
#include "utils.h"

// > [man 2 mmap] on Linux, the mapping will be created at a nearby page boundary.
// TODO: call the syscall `sysconf(_SC_PAGE_SIZE)` to get page size
#define PAGE_SIZE (4 * 1024)
// Must be a multiple of the page size so that we can try to
// reserve the memory directly after the end of the current reserved capacity
// when we need to expand.
//
// Set at 64k to match windows which has alignment requriements.
// Note: we could align to 64k on linux if we needed to for some reason by
//       munmap'ing pages at the beginning to get to 64k alignment.
#define INIT_RESERVE_SIZE (64 * 1024)

// > [man 2 mmap]
//   Note: A sharing option must be chosen, we don't want to share
// > MAP_PRIVATE
// >     Create a private copy-on-write mapping.
// > ...
// > MAP_ANONYMOUS
// >    The mapping is not backed by any file; its contents are
// >    initialized to zero.  The fd argument is ignored; however,
// >    some implementations require fd to be -1 if MAP_ANONYMOUS (or
// >    MAP_ANON) is specified, and portable applications should
// >    ensure this.  The offset argument should be zero.

// Reserve address space without requiring physical memory backing it.
//
// Because mappings are copy on write (meaning the kernel doesn't actually find
// physical memory for the page until you write), disallowing writes with
// PROT_NONE prevents the reserved pages from taking up space.
#define mmap_Reserve(addr, size) mmap((addr), (size), PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0)
// Commit to requiring physical memory to back this address space.
//
// On linux, this is just allowing read and write. This is because virtual
// memory in linux is copy on write. That means technically the memory doesn't
// actually get committed until it gets written.
//
// Also, as mentioned under MAP_ANONYMOUS, this data is initialized to zero
#define mmap_Commit(addr, size) mprotect((void*)(addr), (size), PROT_READ | PROT_WRITE)

// See mem_arena.h for documentation
mem_arena ArenaInit() {
    mem_arena Result = {};

    Result.Base = (uint8_t *)mmap_Reserve(0, INIT_RESERVE_SIZE);
    if (IsError(Result.Base)) {
        Print("Could not reserve memory. errno = %u\n", Errno(Result.Base));
    }
    Assert(!IsError(Result.Base));
    Result.Reserved = INIT_RESERVE_SIZE;

    int err = mprotect_Commit(Result.Base, PAGE_SIZE);
    Assert(!IsError(err));
    Result.Committed = PAGE_SIZE;

    return Result;
}

// See mem_arena.h for documentation
void Expand(mem_arena *Arena) {
    size_t AmountToCommit = PAGE_SIZE;
    size_t NewCommitted = Arena->Committed + AmountToCommit;
    if (NewCommitted > Arena->Reserved) {
        // Try to reserve the reserved size after the existing pointer
        size_t NewReserved = Arena->Reserved * 2;
        void *AppendAddr = (void*)(Arena->Base + Arena->Reserved);
        void *AppendedReserve = mmap_Reserve(AppendAddr, Arena->Reserved);
        if (AppendedReserve == AppendAddr) {
            Arena->Reserved = NewReserved;
        } else { // Could not append reserved pages, must do a copy
            if (IsError(AppendedReserve)) {
                munmap(AppendedReserve, Arena->Reserved);
            }
            uint8_t *NewBase = (uint8_t*)mmap_Reserve(0, NewReserved);

            if (IsError(NewBase)) {
                Print("Could not expand memory\n");
            }
            Assert(!IsError(NewBase));

            mprotect_Commit(NewBase, Arena->Committed);
            MemCopy(NewBase, Arena->Base, Arena->Used);

            Arena->Base = NewBase;
            Arena->Reserved = NewReserved;
        }
    }

    // Commit the extra pages
    mprotect_Commit((Arena->Base + Arena->Committed), AmountToCommit);
    Arena->Committed = NewCommitted;
}
