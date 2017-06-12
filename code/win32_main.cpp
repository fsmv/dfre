// Copyright (c) 2016-2017 Andrew Kallmeyer <fsmv@sapium.net>
// Provided under the MIT License: https://mit-license.org

/*
 * +-------------------+
 * |      NOTICE       |
 * +-------------------+
 *
 * This code is experimental and a work in progress.
 *
 * I am trying to avoid having the code do anything that isn't directly required
 * for accomplishing the current level of features.
 *
 * I intend to produce code which uses a minimal abstraction structure.
 *
 * In order to accomplish this, I am first writing code without creating
 * any abstraction systems. Once I see the code that needs to be executed, I
 * will design an appropriate system.
 *
 * Also in pursuit of this goal, I am attempting to avoid abstractions other
 * people have created. Although I will obviously require established
 * abstractions for outputting information for users and for CPUs. Finally, this
 * program also must acknowledge the fact that it runs on a real machine with
 * limited memory and inside an operating system which manages access to that
 * memory.
 */

#include <stdint.h> // int32_t etc
#define DFRE_WIN32

// TODO: Remove me
#define ArrayLength(arr) (sizeof(arr) / sizeof((arr)[0]))

#include <windows.h>

#include "print.h"
inline void _AssertFailed(int LineNum, const char *File, const char *Condition) {
    Print("ERROR: Assertion failed; %s:%u  Assert(%s)\n", File, LineNum, Condition);
    ExitProcess(1);
}
#define Assert(cond) if (!(cond)) { _AssertFailed(__LINE__, __FILE__, #cond); }

#include "win32_mem_arena.cpp"

size_t ParseArgs(char *CommandLine, size_t NumExpecting, ...) {
    va_list args;
    va_start(args, NumExpecting);
    char **Arg;

    size_t Count = 0;
    // Program name
    char *Curr = CommandLine;
    if (*Curr == '"') { // Quoted program name
        Curr += 1;
        for (; *Curr; ++Curr) {
            if (*Curr == '"') {
                Curr += 1;
                break;
            }
        }
    } else { // Not quoted
        for (; *Curr; ++Curr) {
            if (*Curr == ' ' && *(Curr - 1) != '\\') {
                break;
            }
        }
    }
    if (*Curr == '\0') {
        goto ParseArgs_ret;
    }
    *Curr++ = '\0';
    // Skip any extra spaces
    for (; *Curr && *Curr == ' '; ++Curr) {}

    // Arguments
    while (*Curr) {
        if (Count < NumExpecting) {
            Arg = va_arg(args, char**);
            *Arg = Curr;
        }
        Count += 1;
        // Find the end of the arg
        for (; *Curr; ++Curr) {
            if (*Curr == ' ' && *(Curr - 1) != '\\') {
                break;
            }
        }
        if (*Curr == ' ') {
            *Curr++ = '\0';
            // Skip any extra spaces
            for (; *Curr && *Curr == ' '; ++Curr) {}
        }
    }

ParseArgs_ret:
    // Set any unfilled args to 0
    for (size_t Idx = Count; Idx < NumExpecting; ++Idx) {
        Arg = va_arg(args, char**);
        *Arg = 0;
    }
    va_end(args);
    return Count;
}

void *LoadCode(uint8_t *Code, size_t CodeWritten) {
    void *CodeExe = VirtualAlloc(0, CodeWritten, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    // TODO: Report error
    if (!CodeExe) {
        DWORD Code = GetLastError();
        Print("%u\n", Code);
    }
    MemCopy(CodeExe, Code, CodeWritten);
    VirtualProtect(CodeExe, CodeWritten, PAGE_EXECUTE_READ, 0);
    return CodeExe;
}

#include "tui.cpp"

// Not using the CRT, for fun I guess. The binary is smaller!
// The entry point. For people who search: main(int argc, char *argv[])
void __stdcall mainCRTStartup() {
    // Get the file handle for the output stream
    Out = GetStdHandle(STD_OUTPUT_HANDLE);
    if (!Out || Out == INVALID_HANDLE_VALUE) {
        // we can't print the message, so do a message box (plus boxes are fun)
        MessageBox(0, "Could not get print to standard output", 0, MB_OK | MB_ICONERROR);
        ExitProcess(1);
    }

    char *CommandLine = GetCommandLineA();
    char *ProgName = CommandLine;
    char *Regex, *Word;
    ParseArgs(CommandLine, 2, &Regex, &Word);

    if (!Regex) { // if we didn't find an argument
        Print("Usage: %s [regex] [search string (optional)]\n", ProgName);
        ExitProcess(1);
    }

    CompileAndMatch(Regex, Word);

    ExitProcess(0);
}
