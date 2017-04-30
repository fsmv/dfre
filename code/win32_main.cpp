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
#include <stdarg.h> // varargs defines

#define Assert(cond) if (!(cond)) { *((int*)0) = 0; }
#define ArrayLength(arr) (sizeof(arr) / sizeof((arr)[0]))

#include "parser.cpp"
#include "x86_codegen.cpp"

// TODO: Linux version
#include <windows.h>

HANDLE Out;

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

// A printf clone with less features (not using CRT)
void Print(HANDLE Out, const char *FormatString, ...) {
    va_list args;
    va_start(args, FormatString);

    DWORD CharsWritten;
    DWORD Idx = 0;
    for (; FormatString[Idx]; ++Idx) {
        if (FormatString[Idx] == '%') {
            switch (FormatString[Idx + 1]) {
            case 's': {
                // Write the string up to the percent
                WriteConsoleA(Out, FormatString, Idx, &CharsWritten, 0);

                char *Str = va_arg(args, char*);
                DWORD Len = 0;
                for (; Str[Len]; ++Len)
                    ;
                WriteConsoleA(Out, Str, Len, &CharsWritten, 0);

            } goto next;
            case 'c': {
                WriteConsoleA(Out, FormatString, Idx, &CharsWritten, 0);

                char Char = va_arg(args, char);
                WriteConsoleA(Out, &Char, 1, &CharsWritten, 0);

            } goto next;
            case 'u': {
                WriteConsoleA(Out, FormatString, Idx, &CharsWritten, 0);

                uint32_t Int = va_arg(args, uint32_t);
                char Buffer[10];
                DWORD Len = (DWORD) WriteInt(Int, Buffer, 10);
                WriteConsoleA(Out, Buffer, Len, &CharsWritten, 0);

            } goto next;
            next:
                // Skip the %s
                FormatString += Idx + 2;
                Idx = 0;
            default: ;
            }
        }
    }

    if (Idx != 0) {
        WriteConsoleA(Out, FormatString, Idx, &CharsWritten, 0);
    }

    va_end(args);
}

#include "printers.cpp"

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
    for (; *Curr && *Curr == ' '; ++Curr)
        ;

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
            for (; *Curr && *Curr == ' '; ++Curr)
                ;
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
        Print(Out, "Usage: %s [regex] [search string (optional)]\n", ProgName);
        return 1;
    }

    Print(Out, "------------------- Regex --------------------\n\n");
    Print(Out, Regex);
    Print(Out, "\n");

    // Allocate storage for and then run each stage of the compiler in order
    // TODO: build an allocator for the stages to use with flexible storage

    // Allocate NFA stage storage
    size_t ArcListSize = sizeof(nfa_arc_list) + 32 * sizeof(nfa_transition);
    size_t NFASize = sizeof(nfa) + 32 * ArcListSize;
    nfa *NFA = (nfa *) VirtualAlloc(0, NFASize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    NFA->SizeOfArcList = ArcListSize;
    if (!NFA) {
        Print(Out, "Could not allocate space for NFA\n");
        return 1;
    }

    // Convert regex to NFA
    RegexToNFA(Regex, NFA);

    Print(Out, "\n-------------------- NFA ---------------------\n\n");
    PrintNFA(NFA);

    // Note: this is all x86-specific after this point
    // TODO: ARM support

    // Allocate storage for intermediate representation code gen
    size_t InstructionsSize = sizeof(instruction) * 1024;
    instruction *Instructions = (instruction*) VirtualAlloc(0, InstructionsSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (!Instructions) {
        Print(Out, "Could not allocate space for Instructions\n");
        return 1;
    }

    // Convert the NFA into an intermediate code representation
    size_t InstructionsGenerated = GenerateInstructions(NFA, Instructions);

    Print(Out, "\n---------------- Instructions ----------------\n\n");
    PrintInstructions(Instructions, InstructionsGenerated);

    // Allocate storage for the unpacked x86 opcodes
    size_t UnpackedOpcodesSize = sizeof(opcode_unpacked) * InstructionsGenerated;
    opcode_unpacked *UnpackedOpcodes = (opcode_unpacked*) VirtualAlloc(0, UnpackedOpcodesSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (!UnpackedOpcodes) {
        Print(Out, "Could not allocate space for Unpacked Opcodes");
        return 1;
    }

    // Turn the instructions into x86 op codes and resolve jump destinations
    AssembleInstructions(Instructions, InstructionsGenerated, UnpackedOpcodes);
    // Note: no more return count here, this keeps the same number of instructions

    // Allocate storage for the actual byte code
    size_t CodeSize = sizeof(opcode_unpacked) * InstructionsGenerated; // sizeof(opcode_unpacked) is an upper bound
    uint8_t *Code = (uint8_t*) VirtualAlloc(0, CodeSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

    size_t CodeWritten = PackCode(UnpackedOpcodes, InstructionsGenerated, Code);

    Print(Out, "\n-------------------- Code --------------------\n\n");
    PrintByteCode(Code, CodeWritten);

    if (Word) {
        bool IsMatch = RunCode(Code, CodeWritten, Word);

        Print(Out, "\n\n------------------- Result -------------------\n\n");
        Print(Out, "Search Word: %s\n", Word);
        if (IsMatch) {
            Print(Out, "MATCH\n");
        }else{
            Print(Out, "NO MATCH\n");
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
