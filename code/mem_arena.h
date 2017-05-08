// Copyright (c) 2016-2017 Andrew Kallmeyer <fsmv@sapium.net>
// Provided under the MIT License: https://mit-license.org

#ifndef MEM_ARENA_H_

struct mem_arena {
    uint8_t *Base;
    size_t Used;
    size_t Committed;
    size_t Reserved;
};

void MemCopy(void *Dest, void *Src, size_t NumBytes) {
    uint8_t *DestB = (uint8_t *) Dest;
    uint8_t *SrcB = (uint8_t *) Src;
    for (size_t Copied = 0; Copied < NumBytes; ++Copied) {
        *DestB++ = *SrcB++;
    }
}

mem_arena ArenaInit();
size_t Alloc(mem_arena *Arena, size_t NumBytes);

#define MEM_ARENA_H_
#endif
