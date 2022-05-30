// Copyright (c) 2016-2019 Andrew Kallmeyer <fsmv@sapium.net>
// Provided under the MIT License: https://mit-license.org
#ifndef UTILS_H_

#include "platform.h" // Exit
#include "print.h"

inline void _AssertFailed(int LineNum, const char *File, const char *Condition) {
    Print("ERROR: Assertion failed; %s:%u  Assert(%s)\n", File, LineNum, Condition);
    Exit(1);
}
#define Assert(cond) if (!(cond)) { _AssertFailed(__LINE__, __FILE__, #cond); }

#define ArrayLength(arr) (sizeof(arr) / sizeof((arr)[0]))
#define Max(v1, v2) ((v1) >= (v2) ? (v1) : (v2))
#define DivCeil(x, y) ((x) / (y) + ((x) % (y) > 0))

inline
void MemCopy(void *Dest, const void *Src, size_t NumBytes) {
    uint8_t *DestB = (uint8_t *) Dest;
    uint8_t *SrcB = (uint8_t *) Src;
    for (; NumBytes; --NumBytes) {
        *DestB++ = *SrcB++;
    }
}

#define UTILS_H_
#endif
