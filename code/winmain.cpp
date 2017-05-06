// Copyright (c) 2016 Andrew Kallmeyer <fsmv@sapium.net>
// Provided under the MIT License: https://mit-license.org

#include <stdint.h>
#define Assert(cond) if (!(cond)) { *((int*)0) = 0; }
#define ArrayLength(arr) (sizeof(arr) / sizeof((arr)[0]))

size_t WriteInt(uint32_t A, char *Str, uint32_t base = 16) {
    size_t CharCount = 0;

    // Write the string backwards
    do {
        uint32_t digit = A % base;
        if (digit < 10) {
            *Str++ = '0' + digit;
        } else {
            *Str++ = 'A' + (digit - 10);
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

#include "parser.cpp"
#include "bits_codegen.cpp"

#include <windows.h>

#include <stdarg.h>
void Print(HANDLE Out, const char *FormatString, ...) {
    va_list args;
    va_start(args, FormatString);

    DWORD CharsWritten;
    size_t Idx = 0;
    for (; FormatString[Idx]; ++Idx) {
        if (FormatString[Idx] == '%') {
            switch (FormatString[Idx + 1]) {
            case 's': {
                // Write the string up to the percent
                WriteConsoleA(Out, FormatString, Idx, &CharsWritten, 0);

                char *Str = va_arg(args, char*);
                size_t Len = 0;
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
                size_t Len = WriteInt(Int, Buffer, 10);
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
    DWORD CharsWritten;
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
    HANDLE Out = GetStdHandle(STD_OUTPUT_HANDLE);

    if (!Out || Out == INVALID_HANDLE_VALUE) {
        MessageBox(0, "Could not get std Out", 0, MB_OK | MB_ICONERROR);
        return 1;
    }

    // Parse the command line
    char *CommandLine = GetCommandLineA();
    char *ProgName = CommandLine;
    char *Regex = CommandLine + 1;
    // Find the end of ProgName
    for (; *Regex; ++Regex) {
        if (*Regex == ' ' && *(Regex - 1) != '\\') {
            break;
        }
    }
    if (*Regex) {
        *Regex = '\0';
        Regex += 1;

        // Find the start of Regex
        for (; *Regex && *Regex == ' '; ++Regex)
            ;

        if (!(*Regex)) { // No non space char after ProgName
            Regex = 0;
        }
    } else { // no space after ProgName
        Regex = 0;
    }

    if (!Regex) {
        Print(Out, "Usage: %s [regex]\n", ProgName);
        return 1;
    }

    size_t ArcListSize = sizeof(nfa_arc_list) + 32 * sizeof(nfa_transition);
    size_t NFASize = sizeof(nfa) + 16 * ArcListSize;
    nfa *NFA = (nfa *) VirtualAlloc(0, NFASize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    NFA->SizeOfArcList = ArcListSize;

    if (!NFA) {
        Print(Out, "Could not allocate space for NFA\n");
        return 1;
    }

    ReParse(Regex, NFA);
    PrintNFA(Out, NFA);

#define CodeLen 1024

    uint8_t *Code = (uint8_t*) VirtualAlloc(0, CodeLen, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    size_t CodeWritten = GenerateCode(NFA, Code);

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

        WriteConsoleA(Out, IntStr, IntStrCount+1, &CharsWritten, 0);
    }

    //WriteConsoleA(Out, Code, CodeWritten, &CharsWritten, 0);

    return 0;
}

void __stdcall mainCRTStartup() {
    int Result;
    Result = main();
    ExitProcess(Result);
}
