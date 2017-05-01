// Copyright (c) 2016-2017 Andrew Kallmeyer <fsmv@sapium.net>
// Provided under the MIT License: https://mit-license.org

#include "nfa.h"
#include "x86_opcode.h"

void PrintRegex(char *Regex) {
    char *Ch;
    for (Ch = Regex; *Ch != '\0'; ++Ch) {}
    size_t Size = Ch - Regex;

    Print(Out, "Size: %u Bytes\n\n", Size);
    Print(Out, Regex);
    Print(Out, "\n");
}

void PrintNFALabel(nfa_label Label) {
    switch (Label.Type) {
    case MATCH:
        Print(Out, "'%c'", Label.A);
        break;
    case RANGE:
        Print(Out, "'%c-%c'", Label.A, Label.B);
        break;
    case EPSILON:
        Print(Out, "'Epsilon'", Label.A);
        break;
    case DOT:
        Print(Out, "'.'", Label.A);
        break;
    }
}

void PrintNFA(nfa *NFA) {
    size_t NFASize = sizeof(*NFA) - sizeof(NFA->ArcLists) +
                     (NFA->ArcListCount - 1) * NFA->SizeOfArcList;

    Print(Out, "Size: %u Bytes\n", NFASize);
    Print(Out, "Number of states: %u\n\n", NFA->NumStates);

    for (size_t ArcListIdx = 0; ArcListIdx < NFA->ArcListCount; ++ArcListIdx) {
        nfa_arc_list *ArcList = NFAGetArcList(NFA, ArcListIdx);

        Print(Out, "Arcs labeled ");
        PrintNFALabel(ArcList->Label);
        Print(Out, "; Num: %u\n", ArcList->TransitionCount);

        for (size_t TransitionIdx = 0;
             TransitionIdx < ArcList->TransitionCount;
             ++TransitionIdx)
        {
            nfa_transition *Transition = &ArcList->Transitions[TransitionIdx];

            Print(Out, "    %u => %u\n", Transition->From, Transition->To);
        }
    }
}

void PrintInstructions(instruction *Instructions, size_t NumInstructions) {
    size_t InstructionsSize = sizeof(instruction) * NumInstructions;
    size_t NumJumps = 0;
    for (size_t Idx = 0; Idx < NumInstructions; ++Idx) {
        if (Instructions[Idx].Type == JUMP) {
            NumJumps += 1;
        }
    }
    Print(Out, "Size: %u Bytes\n", InstructionsSize);
    Print(Out, "Num Instructions: %u\n", NumInstructions);
    Print(Out, "Num Jumps: %u\n\n", NumJumps);

    Print(Out, "   Op #   | Mode | Op  |     Arg1      | Arg2\n");
    Print(Out, "-----------------------------------------------\n");
    //           12345678 | REG  | INC | Op # 12345678 |
    //           12345678 | REG  | INC | 12345678      | EAX
    //           12345678 | REG  | INC | EAX           |
    //           12345678 | REG  | INC | EAX           | EAX

    const size_t MaxIntLen = 8;
    const char IntPaddingStr[MaxIntLen+1] = "        ";
    const char *Separator = " | ";

    for (size_t Idx = 0; Idx < NumInstructions; ++Idx) {
        instruction *Instruction = &Instructions[Idx];
        char IntBuf[MaxIntLen+1];
        size_t IntLen;

        // Op #
        Print(Out, " ");
        IntLen = WriteInt((uint32_t)Idx, IntBuf);
        IntBuf[IntLen] = '\0';
        Print(Out, IntBuf);
        Print(Out, IntPaddingStr + IntLen);

        Print(Out, Separator);

        // Mode
        switch (Instruction->Mode) {
        case REG:
            Print(Out, "REG ");
            break;
        case MEM:
            Print(Out, "MEM ");
            break;
        case MODE_NONE:
            Print(Out, "J   ");
            break;
        }

        Print(Out, Separator);

        // Op
        if (Instruction->Type == JUMP) {
            Print(Out, jmp_strings[Instruction->Op]);
        } else {
            Print(Out, op_strings[Instruction->Op]);
        }

        Print(Out, Separator);

        // Args
        switch (Instruction->Type) {
        case JUMP:
            Print(Out, "Op # ");

            IntLen = WriteInt((uint32_t)(Instruction->JumpDest - Instructions), IntBuf);
            IntBuf[IntLen] = '\0';
            Print(Out, IntBuf);
            Print(Out, IntPaddingStr + IntLen);

            Print(Out, Separator);
            break;
        case ONE_REG:
            Print(Out, reg_strings[Instruction->Dest]);
            Print(Out, "         "); // 9

            Print(Out, Separator);
            break;
        case TWO_REG:
            Print(Out, reg_strings[Instruction->Dest]);
            Print(Out, "         "); // 9

            Print(Out, Separator);

            Print(Out, reg_strings[Instruction->Src]);
            break;
        case REG_IMM:
            IntLen = WriteInt(Instruction->Imm, IntBuf);
            IntBuf[IntLen] = '\0';
            Print(Out, IntBuf);
            Print(Out, IntPaddingStr + IntLen);
            Print(Out, "     "); // 5

            Print(Out, Separator);

            Print(Out, reg_strings[Instruction->Dest]);
            break;
        }

        Print(Out, "\n");
    }
}

void PrintUnpackedOpcodes(opcode_unpacked *Opcodes, size_t NumOpcodes) {
    Print(Out, "Size %u Bytes\n\n", sizeof(opcode_unpacked) * NumOpcodes);
    Print(Out, "This is the instructions above in a struct that\n"
               "has real x86 codes instead of our enums.\n");
}

void PrintByteCode(uint8_t *Code, size_t Size) {
    Print(Out, "Size: %u Bytes\n\n", Size);
    for (size_t i = 0; i < Size; ++i) {
        char IntStr[4];
        if (Code[i] < 0x10) {
            IntStr[0] = '0';
            WriteInt(Code[i], IntStr+1);
        } else {
            WriteInt(Code[i], IntStr);
        }

        IntStr[2] = ' ';
        IntStr[3] = '\0';

        Print(Out, IntStr);
        if ((i + 1) % 16 == 0) {
            Print(Out, "\n");
        }
    }
}
