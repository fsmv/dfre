// Copyright (c) 2016-2019 Andrew Kallmeyer <fsmv@sapium.net>
// Provided under the MIT License: https://mit-license.org

#ifndef UTILS_H_
#include "print.h"

#if !defined(Exit)
    #error "Exit((int) code) macro must be defined (in the *_platform file)"
#endif

inline void _AssertFailed(int LineNum, const char *File, const char *Condition) {
    Print("ERROR: Assertion failed; %s:%u  Assert(%s)\n", File, LineNum, Condition);
    Exit(1);
}
#define Assert(cond) if (!(cond)) { _AssertFailed(__LINE__, __FILE__, #cond); }

#define ArrayLength(arr) (sizeof(arr) / sizeof((arr)[0]))
#define Max(v1, v2) ((v1) >= (v2) ? (v1) : (v2))

#define UTILS_H_
#endif
