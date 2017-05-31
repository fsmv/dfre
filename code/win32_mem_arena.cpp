// Copyright (c) 2016-2017 Andrew Kallmeyer <fsmv@sapium.net>
// Provided under the MIT License: https://mit-license.org

#include "mem_arena.h"
#include "print.h"

// TODO: Call GetSystemInfo to get page size and allocation granularity
// 4k pages (you can get this with an api call)
#define PAGE_SIZE (4 * 1024)
// 64k alignment
#define ALIGNMENT (64 * 1024)

mem_arena ArenaInit() {
    mem_arena Result = {};
    size_t InitialReserved = ALIGNMENT;

    Result.Base = (uint8_t *)VirtualAlloc(0, InitialReserved, MEM_RESERVE, PAGE_NOACCESS);
    // TODO: Report error
    if (!Result.Base) {
        DWORD Code = GetLastError();
        Print("%u\n", Code);
    }
    Assert(Result.Base != 0);

    Result.Reserved = InitialReserved;

    VirtualAlloc(Result.Base, PAGE_SIZE, MEM_COMMIT, PAGE_READWRITE);
    Result.Committed = PAGE_SIZE;

    return Result;
}

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
