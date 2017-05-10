// Copyright (c) 2016-2017 Andrew Kallmeyer <fsmv@sapium.net>
// Provided under the MIT License: https://mit-license.org

#include "nfa.h"
#include "x86_opcode.h"
#include "mem_arena.h"
#include "print.h"

void PrintArena(const char *Name, mem_arena *Arena) {
    Print("%s\n  Used: %u\n  Committed: %u\n  Reserved: %u\n\n",
          Name, Arena->Used, Arena->Committed, Arena->Reserved);
}

void PrintRegex(char *Regex) {
    char *Ch;
    for (Ch = Regex; *Ch != '\0'; ++Ch) {}
    size_t Size = Ch - Regex;

    Print("Size: %u Bytes\n\n", Size);
    Print(Regex);
    Print("\n");
}

void PrintNFALabel(nfa_label Label) {
    switch (Label.Type) {
    case MATCH:
        Print("'%c'", Label.A);
        break;
    case RANGE:
        Print("'%c-%c'", Label.A, Label.B);
        break;
    case EPSILON:
        Print("'Epsilon'", Label.A);
        break;
    case DOT:
        Print("'.'", Label.A);
        break;
    }
}

void PrintNFA(nfa *NFA) {
    size_t NFASize = sizeof(nfa) +
                     (NFA->NumArcListsAllocated - 1) * sizeof(nfa_arc_list);

    Print("Size: %u Bytes\n", NFASize);
    Print("Number of states: %u\n\n", NFA->NumStates);

    nfa_arc_list *ArcList = NFAFirstArcList(NFA);
    for (size_t ArcListIdx = 0; ArcListIdx < NFA->NumArcLists; ++ArcListIdx) {
        Print("Arcs labeled ");
        PrintNFALabel(ArcList->Label);
        Print("; Num: %u\n", ArcList->NumTransitions);

        for (size_t TransitionIdx = 0;
             TransitionIdx < ArcList->NumTransitions;
             ++TransitionIdx)
        {
            nfa_transition *Transition = &ArcList->Transitions[TransitionIdx];

            Print("    %u => %u\n", Transition->From, Transition->To);
        }

        ArcList = NFANextArcList(ArcList);
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
    Print("Size: %u Bytes\n", InstructionsSize);
    Print("Num Instructions: %u\n", NumInstructions);
    Print("Num Jumps: %u\n\n", NumJumps);

    Print("   Op #   | Mode | Op  |     Arg1      | Arg2\n");
    Print("-----------------------------------------------\n");
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
        Print(" ");
        IntLen = WriteInt((uint32_t)Idx, IntBuf);
        IntBuf[IntLen] = '\0';
        Print(IntBuf);
        Print(IntPaddingStr + IntLen);

        Print(Separator);

        // Mode
        switch (Instruction->Mode) {
        case REG:
            Print("REG ");
            break;
        case MEM:
            Print("MEM ");
            break;
        case MODE_NONE:
            Print("J   ");
            break;
        }

        Print(Separator);

        // Op
        if (Instruction->Type == JUMP) {
            Print(jmp_strings[Instruction->Op]);
        } else {
            Print(op_strings[Instruction->Op]);
        }

        Print(Separator);

        // Args
        switch (Instruction->Type) {
        case JUMP:
            Print("Op # ");

            IntLen = WriteInt((uint32_t)Instruction->JumpDestIdx, IntBuf);
            IntBuf[IntLen] = '\0';
            Print(IntBuf);
            Print(IntPaddingStr + IntLen);

            Print(Separator);
            break;
        case ONE_REG:
            Print(reg_strings[Instruction->Dest]);
            Print("         "); // 9

            Print(Separator);
            break;
        case TWO_REG:
            Print(reg_strings[Instruction->Dest]);
            Print("         "); // 9

            Print(Separator);

            Print(reg_strings[Instruction->Src]);
            break;
        case REG_IMM:
            IntLen = WriteInt(Instruction->Imm, IntBuf);
            IntBuf[IntLen] = '\0';
            Print(IntBuf);
            Print(IntPaddingStr + IntLen);
            Print("     "); // 5

            Print(Separator);

            Print(reg_strings[Instruction->Dest]);
            break;
        }

        Print("\n");
    }
}

void PrintUnpackedOpcodes(opcode_unpacked *Opcodes, size_t NumOpcodes) {
    Print("Size %u Bytes\n\n", sizeof(opcode_unpacked) * NumOpcodes);
    Print("This is the instructions above in a struct that\n"
          "has real x86 codes instead of our enums.\n");
}

void PrintByteCode(uint8_t *Code, size_t Size) {
    Print("Size: %u Bytes\n\n", Size);
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

        Print(IntStr);
        if ((i + 1) % 16 == 0) {
            Print("\n");
        }
    }
}
