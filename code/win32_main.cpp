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
#define Assert(cond) if (!(cond)) { *((int*)0) = 0; }
#define ArrayLength(arr) (sizeof(arr) / sizeof((arr)[0]))

#include "parser.cpp"
#include "x86_codegen.cpp"

// TODO: Linux version
#include <windows.h>

#include "win32_mem_arena.cpp"
#include "printers.cpp"
#include "print.h"

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
    uint8_t *CodeDest = (uint8_t*) CodeExe;
    for (size_t i = 0; i < CodeWritten; ++i) {
        *CodeDest++ = Code[i];
    }
    VirtualProtect(CodeExe, CodeWritten, PAGE_EXECUTE_READ, 0);
    return CodeExe;
}

bool RunCode(uint8_t *Code, size_t CodeWritten, char *WordPtr) {
    void *CodeLoc = LoadCode(Code, CodeWritten);
    uint32_t IsMatch = 0;
    __asm {
        // Use the registers used in the code so the compiler knows we use them
        xor ebx, ebx
        xor ecx, ecx
        xor edx, edx

        mov eax, WordPtr
        call CodeLoc
        mov IsMatch, ebx
    }
    return (IsMatch != 0);
}

int main() {
    // Get the file handle for the output stream
    Out = GetStdHandle(STD_OUTPUT_HANDLE);
    if (!Out || Out == INVALID_HANDLE_VALUE) {
        // we can't print the message, so do a message box (plus boxes are fun)
        MessageBox(0, "Could not get print to standard output", 0, MB_OK | MB_ICONERROR);
        return 1;
    }

    char *CommandLine = GetCommandLineA();
    char *ProgName = CommandLine;
    char *Regex, *Word;
    ParseArgs(CommandLine, 2, &Regex, &Word);

    if (!Regex) { // if we didn't find an argument
        Print("Usage: %s [regex] [search string (optional)]\n", ProgName);
        return 1;
    }

    Print("-------------------- Regex --------------------\n\n");
    PrintRegex(Regex);

    // Allocate storage for and then run each stage of the compiler in order
    mem_arena ArenaA = ArenaInit();
    mem_arena ArenaB = ArenaInit();

    // Convert regex to NFA
    nfa *NFA = RegexToNFA(Regex, &ArenaA);

    Print("\n--------------------- NFA ---------------------\n\n");
    PrintArena("Arena A", &ArenaA);
    PrintNFA(NFA);

    // Note: this is all x86-specific after this point
    // TODO: ARM support

    // Convert the NFA into an intermediate code representation
    size_t InstructionsGenerated = GenerateInstructions(NFA, &ArenaB);
    instruction *Instructions = (instruction *)ArenaB.Base;

    Print("\n----------------- Instructions ----------------\n\n");
    PrintArena("Arena B", &ArenaB);
    PrintInstructions(Instructions, InstructionsGenerated);

    // Allocate storage for the unpacked x86 opcodes
    NFA = (nfa*)0;
    ArenaA.Used = 0;
    Alloc(&ArenaA, sizeof(opcode_unpacked) * InstructionsGenerated);
    opcode_unpacked *UnpackedOpcodes = (opcode_unpacked*)ArenaA.Base;

    // Turn the instructions into x86 op codes and resolve jump destinations
    AssembleInstructions(Instructions, InstructionsGenerated, UnpackedOpcodes);
    // Note: no more return count here, this keeps the same number of instructions

    Print("\n----------------- x86 Unpacked ----------------\n\n");
    PrintArena("Arena A", &ArenaA);
    PrintUnpackedOpcodes(UnpackedOpcodes, InstructionsGenerated);

    // Allocate storage for the actual byte code
    // TODO: Make PackCode allocate a tighter amount of space
    Instructions = (instruction*)0;
    ArenaB.Used = 0;
    size_t UpperBoundCodeSize = sizeof(opcode_unpacked) * InstructionsGenerated; 
    Alloc(&ArenaB, UpperBoundCodeSize);
    uint8_t *Code = (uint8_t*)ArenaB.Base;

    size_t CodeWritten = PackCode(UnpackedOpcodes, InstructionsGenerated, Code);

    Print("\n--------------------- Code --------------------\n\n");
    PrintArena("Arena B", &ArenaB);
    PrintByteCode(Code, CodeWritten);

    if (Word) {
        bool IsMatch = RunCode(Code, CodeWritten, Word);

        Print("\n\n-------------------- Result -------------------\n\n");
        Print("Search Word: %s\n", Word);
        if (IsMatch) {
            Print("Match\n");
        }else{
            Print("No Match\n");
        }
    }

    return 0;
}

// Not using the CRT, for fun I guess. The binary is smaller!
void __stdcall mainCRTStartup() {
    int Result;
    Result = main();
    ExitProcess(Result);
}
