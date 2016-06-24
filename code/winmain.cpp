// Copyright (c) 2016 Andrew Kallmeyer <fsmv@sapium.net>
// Provided under the MIT License: https://mit-license.org

#include <stdint.h>
#define Assert(cond) if (!(cond)) { *((int*)0) = 0; }
#define ArrayLength(arr) (sizeof(arr) / sizeof((arr)[0]))

#include "parser.cpp"

#include <windows.h>

size_t WriteInt(uint32_t A, char *Str) {
    size_t CharCount = 0;

    // Write the string backwards
    uint32_t base = 10;
    do {
        *Str++ = '0' + (A % base);
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

int main(int argc, char *argv[]) {
    HANDLE Out = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD CharsWritten;
    if (!Out || Out == INVALID_HANDLE_VALUE) {
        MessageBox(0, "Could not get std Out", 0, MB_OK | MB_ICONERROR);
        return 1;
    }

    if (argc != 2) {
        char Usage1[] = "Usage: ";
        char Usage2[] = " [regex]\n";

        uint32_t Argv0Length = 0;
        for (char *Idx = argv[0]; *Idx != '\0'; ++Idx, ++Argv0Length) { }

        WriteConsoleA(Out, Usage1, ArrayLength(Usage1) - 1, &CharsWritten, 0);
        WriteConsoleA(Out, argv[0], Argv0Length, &CharsWritten, 0);
        WriteConsoleA(Out, Usage2, ArrayLength(Usage2) - 1, &CharsWritten, 0);
        return 1;
    }

    size_t ArcListSize = sizeof(nfa_arc_list) + 32 * sizeof(nfa_transition);
    size_t NFASize = sizeof(nfa) + 16 * ArcListSize;
    nfa *NFA = (nfa *) VirtualAlloc(0, NFASize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    NFA->SizeOfArcList = ArcListSize;

    if (!NFA) {
        char Msg[] = "Could not allocate space for NFA\n";
        WriteConsoleA(Out, Msg, ArrayLength(Msg) - 1, &CharsWritten, 0);
        return 1;
    }

    ReParse(argv[1], NFA);

    {
    char Msg[] = "Number of states: ";
    WriteConsoleA(Out, Msg, ArrayLength(Msg) - 1, &CharsWritten, 0);
    }

    char IntStr[10];
    size_t IntStrCount = WriteInt(NFA->NumStates, IntStr);
    WriteConsoleA(Out, IntStr, IntStrCount, &CharsWritten, 0);

    {
    char Msg[] = "\n";
    WriteConsoleA(Out, Msg, ArrayLength(Msg) - 1, &CharsWritten, 0);
    }

    for (size_t ArcListIdx = 0; ArcListIdx < NFA->ArcListCount; ++ArcListIdx) {
        nfa_arc_list *ArcList = NFAGetArcList(NFA, ArcListIdx);

        {
        char Msg[] = "Label: ";
        WriteConsoleA(Out, Msg, ArrayLength(Msg) - 1, &CharsWritten, 0);
        }

        IntStrCount = WriteInt(ArcList->Label.Type, IntStr);
        WriteConsoleA(Out, IntStr, IntStrCount, &CharsWritten, 0);

        {
        char Msg[] = ", ";
        WriteConsoleA(Out, Msg, ArrayLength(Msg) - 1, &CharsWritten, 0);
        }

        WriteConsoleA(Out, &ArcList->Label.A, 1, &CharsWritten, 0);

        {
        char Msg[] = ", ";
        WriteConsoleA(Out, Msg, ArrayLength(Msg) - 1, &CharsWritten, 0);
        }

        WriteConsoleA(Out, &ArcList->Label.B, 1, &CharsWritten, 0);

        {
        char Msg[] = "\n";
        WriteConsoleA(Out, Msg, ArrayLength(Msg) - 1, &CharsWritten, 0);
        }

        for (size_t TransitionIdx = 0;
             TransitionIdx < ArcList->TransitionCount;
             ++TransitionIdx)
        {
            nfa_transition *Transition = &ArcList->Transitions[TransitionIdx];

            {
            char Msg[] = "\t";
            WriteConsoleA(Out, Msg, ArrayLength(Msg) - 1, &CharsWritten, 0);
            }

            IntStrCount = WriteInt(Transition->From, IntStr);
            WriteConsoleA(Out, IntStr, IntStrCount, &CharsWritten, 0);

            {
            char Msg[] = " => ";
            WriteConsoleA(Out, Msg, ArrayLength(Msg) - 1, &CharsWritten, 0);
            }

            IntStrCount = WriteInt(Transition->To, IntStr);
            WriteConsoleA(Out, IntStr, IntStrCount, &CharsWritten, 0);

            {
            char Msg[] = "\n";
            WriteConsoleA(Out, Msg, ArrayLength(Msg) - 1, &CharsWritten, 0);
            }
        }
    }

    return 0;
}
