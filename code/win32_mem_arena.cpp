// Copyright (c) 2016-2017 Andrew Kallmeyer <fsmv@sapium.net>
// Provided under the MIT License: https://mit-license.org

#include "mem_arena.h"
#include "print.h"
#include "utils.h"

// VirtualAlloc documentation [VA]:
//   https://msdn.microsoft.com/en-us/library/windows/desktop/aa366887(v=vs.85).aspx

// > [VA] If the memory is being reserved, the specified address is rounded down to
// > the nearest multiple of the allocation granularity. If the memory is already
// > reserved and is being committed, the address is rounded down to the next page
// > boundary.
// TODO: Call GetSystemInfo to get page size and allocation granularity
#define PAGE_SIZE (4 * 1024)
#define ALLOCATION_GRANULARITY (64 * 1024)
// Must be a multiple of the allocation granularity so that we can try to
// reserve the memory directly after the end of the current reserved capacity
// when we need to expand
#define INIT_RESERVE_SIZE ALLOCATION_GRANULARITY

// See mem_arena.h for documentation
mem_arena ArenaInit() {
    mem_arena Result = {};

    Result.Base = (uint8_t *)VirtualAlloc(0, INIT_RESERVE_SIZE, MEM_RESERVE, PAGE_NOACCESS);
    // TODO: Report error
    if (!Result.Base) {
        DWORD Code = GetLastError();
        Print("%u\n", Code);
    }
    Assert(Result.Base != 0);
    Result.Reserved = INIT_RESERVE_SIZE;

    void *Result = VirtualAlloc(Result.Base, PAGE_SIZE, MEM_COMMIT, PAGE_READWRITE);
    if (!Result) {
        DWORD Code = GetLastError();
        Print("%u\n", Code);
    }
    Assert(Result);
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
        void *AppendedReserve = VirtualAlloc(Arena->Base + Arena->Reserved,
                                             Arena->Reserved, MEM_RESERVE, PAGE_NOACCESS);
        if (AppendedReserve) {
            Arena->Reserved = NewReserved;
        } else { // Could not append reserved pages, must do a copy
            uint8_t *NewBase = (uint8_t*)VirtualAlloc(0, NewReserved,
                                                      MEM_RESERVE, PAGE_NOACCESS);
            // TODO: Report error
            Assert(NewBase != 0);
            if (!NewBase) {
                DWORD Code = GetLastError();
                Print("%u\n", Code);
            }

            VirtualAlloc(NewBase, Arena->Committed, MEM_COMMIT, PAGE_READWRITE);
            MemCopy(NewBase, Arena->Base, Arena->Used);

            Arena->Base = NewBase;
            Arena->Reserved = NewReserved;
        }
    }

    // Commit the extra pages
    VirtualAlloc(Arena->Base + Arena->Committed, AmountToCommit, MEM_COMMIT, PAGE_READWRITE);
    Arena->Committed = NewCommitted;
}
