// Copyright (c) 2016-2019 Andrew Kallmeyer <fsmv@sapium.net>
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

int CompileAndMatch(bool Verbose, char *Regex, char *Word) {
    if (Verbose) {
        Print("-------------------- Regex --------------------\n\n");
        PrintRegex(Regex);
        Print("\n"); // Goes here because NFA is conditionally the first section
    }

    // Allocate storage for and then run each stage of the compiler in order
    mem_arena ArenaA = ArenaInit();
    mem_arena ArenaB = ArenaInit();

    // Convert regex to NFA
    nfa *NFA = RegexToNFA(Regex, &ArenaA);

    if (!Word || Verbose) {
        Print("--------------------- NFA ---------------------\n\n");
        if (Verbose) {
            PrintArena("Arena A", &ArenaA);
        }
        PrintNFA(NFA);
    }

    // Note: this is all x86-specific after this point
    // TODO: ARM support

    // Convert the NFA into an intermediate code representation
    size_t InstructionsGenerated = GenerateInstructions(NFA, &ArenaB);
    instruction *Instructions = (instruction *)ArenaB.Base;

    if (!Word || Verbose) {
        Print("\n----------------- Instructions ----------------\n\n");
        if (Verbose) {
            PrintArena("Arena B", &ArenaB);
        }
        PrintInstructions(Instructions, InstructionsGenerated);
    }

    // Allocate storage for the unpacked x86 opcodes
    NFA = (nfa*)0;
    ArenaA.Used = 0;
    Alloc(&ArenaA, sizeof(opcode_unpacked) * InstructionsGenerated);
    opcode_unpacked *UnpackedOpcodes = (opcode_unpacked*)ArenaA.Base;

    // Turn the instructions into x86 op codes and resolve jump destinations
    AssembleInstructions(Instructions, InstructionsGenerated, UnpackedOpcodes);
    // Note: no more return count here, this keeps the same number of instructions

    if (Verbose) {
        Print("\n----------------- x86 Unpacked ----------------\n\n");
        PrintArena("Arena A", &ArenaA);
        PrintUnpackedOpcodes(UnpackedOpcodes, InstructionsGenerated);
    }

    // Allocate storage for the actual byte code
    // TODO: Make PackCode allocate a tighter amount of space
    Instructions = (instruction*)0;
    ArenaB.Used = 0;
    size_t UpperBoundCodeSize = sizeof(opcode_unpacked) * InstructionsGenerated;
    Alloc(&ArenaB, UpperBoundCodeSize);
    uint8_t *Code = (uint8_t*)ArenaB.Base;

    size_t CodeWritten = PackCode(UnpackedOpcodes, InstructionsGenerated, Code);

    if (!Word || Verbose) {
        Print("\n--------------------- Code --------------------\n\n");
        PrintArena("Arena B", &ArenaB);
        PrintByteCode(Code, CodeWritten);
    }

    if (Word) {
        bool IsMatch = RunCode(Code, CodeWritten, Word);

        if (Verbose) {
            Print("\n-------------------- Result -------------------\n\n");
            Print("Search Word: %s\n", Word);
        }
        if (IsMatch) {
            Print("Match\n");
            return 0;
        }else{
            Print("No Match\n");
            return 1;
        }
    }
    return 0;
}

extern "C"
int main(int argc, char *argv[]) {
    if (argc < 2) { // program name and the required regex
        Print("Usage: %s (-v) [regex] (optional search string)\n", argv[0]);
        return 1;
    }

    // TODO: Real flag parser (this is pretty hacky)
    bool Verbose = false;
    if (argv[1][0] == '-' && argv[1][1] == 'v' && argv[1][2] == '\0') {
        Verbose = true;
        argv += 1;
        argc += 1;
    }

    char *Word = 0;
    if (argc > 2) {
        Word = argv[2];
    }

    return CompileAndMatch(Verbose, argv[1], Word);
}
