// Copyright (c) 2016 Andrew Kallmeyer <fsmv@sapium.net>
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
 * Also in persuit of this goal, I am attempting to avoid abstractions other
 * people have created. Although I will obviously require established
 * abstractions for outputting information for users and for CPUs. Finally, this
 * program also must acknoledge the fact that it runs on a real machine with
 * limited memory and inside an operating system which manages access to that
 * memory.
 */

#include <stdint.h>

// TODO: Linux version
#include <windows.h>

#define Assert(cond) if (!(cond)) { *((int*)0) = 0; }
#define ArrayLength(arr) (sizeof(arr) / sizeof((arr)[0]))

#include "parser.cpp"
#include "x86_codegen.cpp"

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

#include <stdarg.h> // varargs defines
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

void PrintNFA(HANDLE Out, nfa *NFA) {
    Print(Out, "Number of states: %u\n", NFA->NumStates);

    for (size_t ArcListIdx = 0; ArcListIdx < NFA->ArcListCount; ++ArcListIdx) {
        nfa_arc_list *ArcList = NFAGetArcList(NFA, ArcListIdx);

        Print(Out, "Label: %u, %c, %c\n",
              ArcList->Label.Type, ArcList->Label.A, ArcList->Label.B);

        for (size_t TransitionIdx = 0;
             TransitionIdx < ArcList->TransitionCount;
             ++TransitionIdx)
        {
            nfa_transition *Transition = &ArcList->Transitions[TransitionIdx];

            Print(Out, "\t%u => %u\n", Transition->From, Transition->To);
        }
    }
}

int main() {
    // Get the file handle for the output stream
    HANDLE Out = GetStdHandle(STD_OUTPUT_HANDLE);
    if (!Out || Out == INVALID_HANDLE_VALUE) {
        // we can't print the message, so do a message box (plus boxes are fun)
        MessageBox(0, "Could not get print to standard output", 0, MB_OK | MB_ICONERROR);
        return 1;
    }

    // Parse the command line
    char *CommandLine = GetCommandLineA();
    char *ProgName = CommandLine;
    // Find the arguments
    char *Regex = CommandLine + 1;
    for (; *Regex; ++Regex) {
        if (*Regex == ' ' && *(Regex - 1) != '\\') {
            break;
        }
    }
    if (*Regex) { // found a space
        // fill the space between the args and progname for printing progname
        *Regex = '\0';
        Regex += 1;

        // Skip any extra spaces
        for (; *Regex && *Regex == ' '; ++Regex)
            ;

        if (!(*Regex)) { // There were spaces, but no argument
            Regex = 0;
        }

        // otherwise Regex now points to the argument
    } else { // no arguments
        Regex = 0;
    }

    if (!Regex) { // if we didn't find an argument
        Print(Out, "Usage: %s [regex]\n", ProgName);
        return 1;
    }

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
    ReParse(Regex, NFA);
    PrintNFA(Out, NFA);

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

#if 0
    // Print the final code (in text)
    WriteConsoleA(Out, Code, CodeWritten, &CharsWritten, 0);
#else
    // Print the final code (in hex)
    Print(Out, "\n");
    DWORD CharsWritten;
    for (int i = 0; i < CodeWritten; ++i) {
        char IntStr[5];
        size_t IntStrCount;
        if (Code[i] < 0x10) {
            IntStr[0] = '0';
            IntStrCount = WriteInt(Code[i], IntStr+1) + 1;
        } else {
            IntStrCount = WriteInt(Code[i], IntStr);
        }

        IntStr[IntStrCount] = ' ';

        WriteConsoleA(Out, IntStr, (DWORD) IntStrCount+1, &CharsWritten, 0);
    }
#endif

    return 0;
}

// Not using the CRT, for fun I guess. The binary is smaller!
void __stdcall mainCRTStartup() {
    int Result;
    Result = main();
    ExitProcess(Result);
}
