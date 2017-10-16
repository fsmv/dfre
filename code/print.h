// Copyright (c) 2016-2017 Andrew Kallmeyer <fsmv@sapium.net>
// Provided under the MIT License: https://mit-license.org

#ifndef PRINT_H_
#include <stdarg.h> // varargs defines

#if !defined(Write)
    #error "Write(char *str, int len) macro must be defined (in the *_platform file)"
#endif

// If we ever need signed, add one more for the sign
#define BASE10_MAX_INT_STR 10
#define BASE16_MAX_INT_STR  8

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
 *     %x - unsigned int32, in base 16
 *     %% - literal '%'
 *     %. - Anything else is ignored silently
 */
uint32_t Print(const char *FormatString, ...) {
    va_list args;
    va_start(args, FormatString);

    char IntBuffer[BASE10_MAX_INT_STR+1];

    uint32_t CharsWritten = 0;
    const char *SectionStart = FormatString;
    const char *Curr = FormatString;
    for (; *Curr; ++Curr) {
        if (*Curr == '%') {
            const size_t SectionLen = Curr - SectionStart;
            Curr += 1;
            //TODO: Add padding with spaces and with zeros
            switch (*Curr) {
            case '%': {
                // Write the string up to and including the first percent
                CharsWritten += Write(SectionStart, SectionLen + 1);
            } goto next;
            case 's': {
                // Write the string up to the percent
                CharsWritten += Write(SectionStart, SectionLen);

                char *Str = va_arg(args, char*);
                size_t Len = 0;
                for (; Str[Len]; ++Len) {}
                CharsWritten += Write(Str, Len);
            } goto next;
            case 'c': {
                CharsWritten += Write(SectionStart, SectionLen);

                char Char = (char)va_arg(args, int);
                CharsWritten += Write(&Char, 1);
            } goto next;
            case 'u': {
                CharsWritten += Write(SectionStart, SectionLen);

                uint32_t Int = va_arg(args, uint32_t);
                size_t Len = WriteInt(Int, IntBuffer, 10);
                CharsWritten += Write(IntBuffer, Len);
            } goto next;
            case 'x': {
                CharsWritten += Write(SectionStart, SectionLen);

                uint32_t Int = va_arg(args, uint32_t);
                size_t Len = WriteInt(Int, IntBuffer, 16);
                CharsWritten += Write(IntBuffer, Len);
            } goto next;
            next: // Reset the section string state
                SectionStart = Curr + 1; // +1 for the char after %
            default: ;
            }
        }
    }
    if (Curr != SectionStart) {
        CharsWritten += Write(SectionStart, (Curr - SectionStart));
    }

    va_end(args);
    return CharsWritten;
}

#define PRINT_H_
#endif
