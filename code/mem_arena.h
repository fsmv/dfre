// Copyright (c) 2016-2017 Andrew Kallmeyer <fsmv@sapium.net>
// Provided under the MIT License: https://mit-license.org

#ifndef MEM_ARENA_H_

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

// TODO: I think it's possible to implement ArenaInit and Expand in a cross
// platform way. We just need platform layer reserve and commit functions (which
// will require a unified error handling system, coincides with dropping Alert).
// Then the *_mem_arena.cpp files can go away (and this file can be called
// .cpp?), the PAGE size getting syscalls and constants can be done in the main
// platform files.

// Reserve a signficant chunk of address space and Commit one page
//
// Reseserved address space does not take up physical memory. Reserving
// a large chunk which we linearly commit 
mem_arena ArenaInit();

// Increase the available empty committed memory by one page.
//
// If there's reserved space, just commits one page. If there's no reserved
// space left, double the reserved space. First attempt to just reserve the
// space after the existing data to avoid a copy. If that fails, reserve a new
// block and copy over the data.
void Expand(mem_arena *Arena);

// Allocate space for NumBytes at the end of the data block in the mem_arena.
//
// Mark the next available NumBytes as used in the mem_arena (a single add).
// Expand the arena if needed to store the additional NumBytes (a couple syscalls, possibly a copy).
size_t Alloc(mem_arena *Arena, size_t NumBytes) {
    while (NumBytes + Arena->Used > Arena->Committed) {
        Expand(Arena);
    }

    size_t Offset = Arena->Used;
    Arena->Used += NumBytes;
    return Offset;
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
