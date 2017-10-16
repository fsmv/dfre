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
#include <windows.h>

// TODO: Maybe put Assert and ArrayLength in a utils header
#include "print.h"
inline void _AssertFailed(int LineNum, const char *File, const char *Condition) {
    Print("ERROR: Assertion failed; %s:%u  Assert(%s)\n", File, LineNum, Condition);
    ExitProcess(1);
}
#define Assert(cond) if (!(cond)) { _AssertFailed(__LINE__, __FILE__, #cond); }
#define ArrayLength(arr) (sizeof(arr) / sizeof((arr)[0]))

#include "win32_mem_arena.cpp"

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

#define MaxNumArgs 10
static char *Argv[MaxNumArgs];
char **ParseArgs(char *CommandLine, size_t *NumArgs) {
    *NumArgs = 0;
    // Program name
    char *Curr = CommandLine;
    Argv[(*NumArgs)++] = Curr;
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
        return Argv;
    }
    *Curr++ = '\0';
    // Skip any extra spaces
    for (; *Curr && *Curr == ' '; ++Curr) {}

    // Arguments
    while (*Curr && *NumArgs < MaxNumArgs) {
        Argv[(*NumArgs)++] = Curr;
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
        if (Argv[*NumArgs-1] == Curr) {
            // The last arg was empty
            *NumArgs -= 1;
        }
    }

    return Argv;
}

extern "C"
int main(int argc, char *argv[]);

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

    size_t argc;
    char **argv = ParseArgs(GetCommandLineA(), &argc);
    int ExitCode = main((int)argc, argv);
    ExitProcess(ExitCode);
}
