// Copyright (c) 2016-2019 Andrew Kallmeyer <fsmv@sapium.net>
// Provided under the MIT License: https://mit-license.org

#ifndef MEM_ARENA_H_

#include "platform.h"
#include "utils.h"

// An expanding contiguous memory block allocator
//
// First it Reserves a "large" chunk of address space (64k currently) and
// Commits a single Page (usually 4k).
// Then when it Expands 
//
// Garunteed to be zero initialized on all current platforms (not sure if we'll
// keep this in the long run)
struct mem_arena {
    uint8_t *Base;
    size_t Used;
    size_t Committed;
    size_t Reserved;
};

// Reserve a signficant chunk of address space and Commit one page
// The Base pointer will be NULL if there was an error (the whole struct will be
// zero value)
//
// Reseserved address space does not take up physical memory. Reserving
// a large chunk which we linearly commit 
mem_arena ArenaInit() {
    // Reserve at least 16 pages on systems with high resolution reserves (linux)
    const size_t InitReserveSize = Max(16*PAGE_SIZE, RESERVE_GRANULARITY);
    Assert(InitReserveSize > PAGE_SIZE && InitReserveSize % PAGE_SIZE == 0);

    mem_arena Result = {};
    Result.Base = (uint8_t *)Reserve(0, InitReserveSize);
    if (!Result.Base) {
        return Result;
    }
    if (!Commit(Result.Base, PAGE_SIZE)) {
        Result.Base = 0;
        return Result;
    }
    Result.Reserved = InitReserveSize;
    Result.Committed = PAGE_SIZE;
    return Result;
}

void ArenaFree(mem_arena *Arena) {
    // Note: In Windows the entire Arena is always a single Reserve block
    Free(Arena->Base, Arena->Reserved);
    *Arena = {};
}

// Increase the available empty committed memory by one page.
// Returns false when there was an error allocating, true means okay.
//
// If there's reserved space, just commits one page. If there's no reserved
// space left, double the reserved space. First attempt to just reserve the
// space after the existing data to avoid a copy. If that fails, reserve a new
// block and copy over the data.
//
// Implemented in the platform layer since POSIX platforms can sometimes avoid
// copies where Windows can't.
//
// POSIX version allows us to sometimes avoid a copy if we can just reserve the
// space right after what we already have.
//
// On Windows we always have to copy when we run out of reserved space. The
// entire arena always has to be one Reservation because of the behavior of
// VirtualFree.
//
// TODO: This should probably accept an amount to expand by and allocate enough
//       pages to fit that in one go
bool Expand(mem_arena *Arena) {
    const size_t AmountToCommit = PAGE_SIZE;
    size_t NewCommitted = Arena->Committed + AmountToCommit;

    // If we have room, just commit the next page
    if (NewCommitted <= Arena->Reserved) {
        if (!Commit((Arena->Base + Arena->Committed), AmountToCommit)) {
            return false;
        }
        Arena->Committed = NewCommitted;
        return true;
    }

    // We want to have twice the reservation size we have now
    size_t NewReserved = Arena->Reserved * 2;
#if DFRE_NIX32 || DFRE_OSX32
    // Try to reserve the extra amount needed immediately after the existing pointer
    void *AppendAddr = (void*)(Arena->Base + Arena->Reserved);
    void *AppendedReserve = Reserve(AppendAddr, Arena->Reserved);
    if (AppendedReserve == AppendAddr) {
        // It worked! Now commit the next page.
        Arena->Reserved = NewReserved;
        if (!Commit((Arena->Base + Arena->Committed), AmountToCommit)) {
            return false;
        }
        Arena->Committed = NewCommitted;
        return true;
    } // Could not append reserved pages, must do a copy

    // It's possible we got a reservation but not at the exact address we
    // wanted. So free it because we can't use it.
    //
    // [man 2 mmap]
    // > If addr is not NULL, then the kernel takes it as a hint
    // > about where to place the mapping; on Linux, the mapping
    // > will be created at a nearby page boundary.
    if (AppendedReserve) {
        Free(AppendedReserve, Arena->Reserved);
    }
#endif

    // No more options, we're doing a whole new reserve and copy
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
    return true;
}

// Allocate space for NumBytes at the end of the data block in the mem_arena.
// Returns a pointer to the start of the newly allocated portion.
// Returns NULL if there was an error allocating.
//
// Mark the next available NumBytes as used in the mem_arena (a single add), or
// if needed, expand the arena to store the additional NumBytes (a couple syscalls, possibly a copy).
void *Alloc(mem_arena *Arena, size_t NumBytes) {
    while (NumBytes + Arena->Used > Arena->Committed) {
        if (!Expand(Arena)) {
            return 0;
        }
    }
    void *Result = (void*)(Arena->Base + Arena->Used);
    Arena->Used += NumBytes;
    return Result;
}

#define MEM_ARENA_H_
#endif
