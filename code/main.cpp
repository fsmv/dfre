// Copyright (c) 2016-2017 Andrew Kallmeyer <fsmv@sapium.net>
// Provided under the MIT License: https://mit-license.org

#if defined(DFRE_WIN32)
    #include "win32_platform.cpp"
#elif defined(DFRE_NIX32)
    #include "linux32_syscalls.cpp"
    #include "posix_platform.cpp"
#elif defined(DFRE_OSX32)
    #include "osx32_syscalls.cpp"
    #include "posix_platform.cpp"
#else
    #error "DFRE_WIN32, DFRE_NIX32, or DFRE_OSX32 must be defined to set the platform"
#endif

#include "parser.cpp"
#include "x86_codegen.cpp"
#include "printers.cpp"

#include "utils.h"
#include "print.h"
#include "mem_arena.h"

// Function pointer type for calling the compiled regex code
extern "C" typedef uint32_t (*dfreMatch)(char *Str);

void *LoadCode(uint8_t *Code, size_t CodeWritten);

bool RunCode(uint8_t *Code, size_t CodeWritten, char *WordPtr) {
    void *CodeLoc = LoadCode(Code, CodeWritten);
    dfreMatch Match = (dfreMatch)CodeLoc;
    uint32_t IsMatch = Match(WordPtr);
    return (IsMatch != 0);
}

void CompileAndMatch(char *Regex, char *Word) {
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
}

extern "C"
int main(int argc, char *argv[]) {
    if (argc < 2) {
        Print("Usage: %s [regex] [search string (optional)]\n", argv[0]);
        return 1;
    }

    char *Word = 0;
    if (argc > 2) {
        Word = argv[2];
    }

    CompileAndMatch(argv[1], Word);
    return 0;
}
