// Copyright (c) 2016-2017 Andrew Kallmeyer <fsmv@sapium.net>
// Provided under the MIT License: https://mit-license.org

#ifndef PRINT_H_
#include <stdarg.h> // varargs defines

#if defined(DFRE_WIN32)
    static HANDLE Out; // Assigned at the top of win32_main.cpp:main()
    static DWORD TempCharsWritten;

    #define Write(str, len) (WriteConsoleA(Out, (str), (DWORD)(len), &TempCharsWritten, 0), \
                             (uint32_t)TempCharsWritten)
#elif defined(DFRE_NIX32)
    #define Write(str, len) write(1, (str), (len))
#else
    #error "DFRE_WIN32 or DFRE_NIX32 must be defined to set the platform"
#endif

// Write an integer to a string in the specified base (not using CRT)
size_t WriteInt(uint32_t A, char *Str, uint32_t base = 16) {
    size_t CharCount = 0;

    // Write the string backwards
    do {
        uint32_t digit = A % base;
        if (digit < 10) {
            *Str++ = '0' + (char) digit;
        } else {
            *Str++ = 'A' + (char) (digit - 10);
        }
        A /= base;
        CharCount += 1;
    } while (A > 0);

    // Reverse the string
    for (char *Start = Str - CharCount, *End = Str - 1;
         Start < End;
         ++Start, --End)
    {
        char Temp = *Start;
        *Start = *End;
        *End = Temp;
    }

    return CharCount;
}

/**
 * A printf clone with less features (not using CRT)
 * Supports:
 *     %s - null terminated char array
 *     %c - char
 *     %u - unsigned int32, in base 10
 *     %% - literal '%'
 *     %. - Anything else is ignored silently
 */
uint32_t Print(const char *FormatString, ...) {
    va_list args;
    va_start(args, FormatString);

    uint32_t CharsWritten = 0;
    size_t Idx = 0;
    for (; FormatString[Idx]; ++Idx) {
        if (FormatString[Idx] == '%') {
            switch (FormatString[Idx + 1]) {
            case '%': {
                // Write the string up to and including the first percent
                CharsWritten += Write(FormatString, Idx + 1);
            } goto next;
            case 's': {
                // Write the string up to the percent
                CharsWritten += Write(FormatString, Idx);

                char *Str = va_arg(args, char*);
                DWORD Len = 0;
                for (; Str[Len]; ++Len) {}
                CharsWritten += Write(Str, Len);
            } goto next;
            case 'c': {
                CharsWritten += Write(FormatString, Idx);

                char Char = va_arg(args, char);
                CharsWritten += Write(&Char, 1);
            } goto next;
            case 'u': {
                CharsWritten += Write(FormatString, Idx);

                uint32_t Int = va_arg(args, uint32_t);
                char Buffer[10];
                DWORD Len = (DWORD) WriteInt(Int, Buffer, 10);
                CharsWritten += Write(Buffer, Len);
            } goto next;
            next: // Act like we're doing a new call skipping to after the '%.'
                FormatString += Idx + 2;
                Idx = 0;
            default: ;
            }
        }
    }

    if (Idx != 0) {
        CharsWritten += Write(FormatString, Idx);
    }

    va_end(args);
    return CharsWritten;
}

#define PRINT_H_
#endif
