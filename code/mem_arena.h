// Copyright (c) 2016-2017 Andrew Kallmeyer <fsmv@sapium.net>
// Provided under the MIT License: https://mit-license.org

#ifndef MEM_ARENA_H_

#include "utils.h"

// Minimum size of a memory block that can be committed or reserved.
// Blocks are aligned to addresses that are multiples of PAGE_SIZE.
extern const size_t PAGE_SIZE;
// Minimum size of a memory block that can be reserved.
// Reservations are aligned to address that are multiples of RESERVE_GRANULARITY
extern const size_t RESERVE_GRANULARITY;

// Reserve address space without requiring physical memory backing it.
void *Reserve(void *addr, size_t size);
// Commit to requiring physical memory to back this reserved address space.
// The memory is initialized to zero.
bool Commit(void *addr, size_t size);
// Deallocate any pages in the range. Uncommits and Unreserves.
// addr and size must be muliple of PAGE_SIZE
//
// Free must be called on the entire result of each Reserve call (even if a
// contiguous range was created from multiple Reserve calls). This is due to
// Windows having this restriction, even though POSIX can Free any arbitary
// range of pages.
void Free(void *addr, size_t size);

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
bool Expand(mem_arena *Arena);

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

void MemCopy(void *Dest, void *Src, size_t NumBytes) {
    uint8_t *DestB = (uint8_t *) Dest;
    uint8_t *SrcB = (uint8_t *) Src;
    for (size_t Copied = 0; Copied < NumBytes; ++Copied) {
        *DestB++ = *SrcB++;
    }
}

#define MEM_ARENA_H_
#endif
