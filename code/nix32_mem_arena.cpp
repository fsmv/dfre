// Copyright (c) 2016-2017 Andrew Kallmeyer <fsmv@sapium.net>
// Provided under the MIT License: https://mit-license.org

#include "mem_arena.h"
#include "print.h"

// TODO: call the syscall to get page size
#define PAGE_SIZE (4 * 1024)
#define INIT_SIZE (64 * 1024)

#define mmap_Reserve(addr, size) mmap((addr), (size), PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0)
#define mprotect_Commit(addr, size) mprotect((void*)(addr), (size), PROT_READ | PROT_WRITE)

mem_arena ArenaInit() {
    mem_arena Result = {};

    Result.Base = (uint8_t *)mmap_Reserve(0, INIT_SIZE);
    if (IsError(Result.Base)) {
        Print("Could not reserve memory. errno = %u\n", Errno(Result.Base));
    }
    Assert(!IsError(Result.Base));
    Result.Reserved = INIT_SIZE;

    int err = mprotect_Commit(Result.Base, PAGE_SIZE);
    Assert(!IsError(err));
    Result.Committed = PAGE_SIZE;

    return Result;
}

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
