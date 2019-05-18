// Copyright (c) 2016-2019 Andrew Kallmeyer <fsmv@sapium.net>
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
#include <memoryapi.h>

static HANDLE Out; // Initialized in mainCRTStartup()
static DWORD TempCharsWritten;
#define Write(str, len) (WriteConsoleA(Out, (str), (DWORD)(len), &TempCharsWritten, 0), \
                         (uint32_t)TempCharsWritten)
#define Exit(code) ExitProcess((code))

// https://msdn.microsoft.com/en-us/library/windows/desktop/aa366887(v=vs.85).aspx
//
// > If the memory is being reserved, the specified address is rounded down to
// > the nearest multiple of the allocation granularity. If the memory is already
// > reserved and is being committed, the address is rounded down to the next page
// > boundary.
const size_t PAGE_SIZE;
const size_t RESERVE_GRANULARITY;

// See mem_arena.h for documentation
void *Reserve(void *addr, size_t size) {
    void *Result = VirtualAlloc(addr, size, MEM_RESERVE, PAGE_NOACCESS);
    if (!Result) {
        DWORD Code = GetLastError();
        Print("Failed to reserve memory (%u bytes). Windows Error Code: %u\n",
              size, Code);
        return 0;
    }
    return Result;
}

// See mem_arena.h for documentation
bool Commit(void *addr, size_t size) {
    void *Result = VirtualAlloc(addr, size, MEM_COMMIT, PAGE_READWRITE);
    if (!Result) {
        DWORD Code = GetLastError();
        Print("Failed to commit memory (%u bytes). Windows Error Code: %u\n",
              size, Code);
        return true;
    }
    return false;
}

// See mem_arena.h for documentation
void Free(void *addr, size_t size) {
    // https://docs.microsoft.com/en-us/windows/desktop/api/memoryapi/nf-memoryapi-virtualfree
    //
    // > If the dwFreeType parameter is MEM_RELEASE, this parameter must be 0
    // > (zero). The function frees the entire region that is reserved in the
    // > initial allocation call to VirtualAlloc.
    if (!VirtualFree(addr, 0, MEM_RELEASE)) {
        DWORD Code = GetLastError();
        Print("Failed to free memory (%u bytes). Windows Error Code: %u\n",
              size, Code);
    }
}

#include "mem_arena.h"

// See mem_arena.h for documentation
//
// On Windows we always have to copy when we run out of reserved space. The
// entire arena alwasy has to be one Reservation because of the behavior of
// VirtualFree.
bool Expand(mem_arena *Arena) {
    const size_t AmountToCommit = PAGE_SIZE;
    size_t NewCommitted = Arena->Committed + AmountToCommit;
    if (NewCommitted > Arena->Reserved) {
        // Reserve twice the size
        size_t NewReserved = Arena->Reserved * 2;
        uint8_t *NewBase = (uint8_t*)Reserve(0, NewReserved);
        if (!NewBase) {
            return false;
        }
        // Commit the new size we want committed
        if (!Commit(NewBase, NewCommitted)) {
            Free(NewBase, NewReserved);
            return false;
        }
        // Copy over existing data and clean up
        MemCopy(NewBase, Arena->Base, Arena->Used);
        Free(Arena->Base, Arena->Reserved);
        Arena->Base = NewBase;
        Arena->Reserved = NewReserved;
        Arena->Committed = NewCommitted;
    } else {
        // We have room, just commit the next page
        if (!Commit((Arena->Base + Arena->Committed), AmountToCommit)) {
            return false;
        }
        Arena->Committed = NewCommitted;
    }
    return true;
}

void *LoadCode(uint8_t *Code, size_t CodeWritten) {
    void *CodeExe = VirtualAlloc(0, CodeWritten, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (!CodeExe) {
        DWORD Code = GetLastError();
        Print("Failed to load code. Windows Error Code: %u\n", Code);
        return 0;
    }
    MemCopy(CodeExe, Code, CodeWritten);
    // TODO: check error
    VirtualProtect(CodeExe, CodeWritten, PAGE_EXECUTE_READ, 0);
    return CodeExe;
}

#define MaxNumArgs 10
static char *Argv[MaxNumArgs];
char **ParseArgs(char *CommandLine, size_t *NumArgs) {
    *NumArgs = 0;
    char *Curr = CommandLine;
    while (*Curr && *NumArgs < MaxNumArgs) {
        // Check if it starts with a quote
        bool Quoted = false;
        if (*Curr == '"') {
            Curr += 1;
            Quoted = true;
        }
        // Save the pending arg start (without the quote if one was there)
        Argv[(*NumArgs)++] = Curr;
        // Find the end of the arg
        for (; *Curr; ++Curr) {
            bool FoundEndChar = (!Quoted && *Curr == ' ') || (Quoted && *Curr == '"');
            if (FoundEndChar && *(Curr - 1) != '\\') {
                break;
            }
        }
        // Fill in \0 at the end of the arg if needed
        if (*Curr == ' ' || *Curr == '"') {
            *Curr++ = '\0';
            // Skip any extra spaces
            for (; *Curr && *Curr == ' '; ++Curr) {}
        }
        if (Argv[*NumArgs-1] == Curr) { // The pending arg was empty
            *NumArgs -= 1;
        }
    }
    return Argv;
}

extern "C" int main(int argc, char *argv[]);

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
    // Get allocation constants
    SYSTEM_INFO SysInfo;
    GetSystemInfo(&SysInfo);
    PAGE_SIZE = SysInfo.dwPageSize;
    RESERVE_GRANULARITY = SysInfo.dwAllocationGranularity;
    // Run main
    size_t argc;
    char **argv = ParseArgs(GetCommandLineA(), &argc);
    int ExitCode = main((int)argc, argv);
    ExitProcess(ExitCode);
}
