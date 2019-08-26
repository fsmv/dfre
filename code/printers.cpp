// Copyright (c) 2016-2019 Andrew Kallmeyer <fsmv@sapium.net>
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


inline void printFirstArg(instruction *Instruction) {
    const char *RegName = reg_strings[Instruction->Dest];
    if (Instruction->Mode == MEM_DISP8 || Instruction->Mode == MEM_DISP32) {
        Print("%s + %x", RegName, (uint32_t)Instruction->Disp);
    } else {
        Print(RegName);
    }
}

// Prints one instruction with no new line at the end. Examples:
//
// [REG] AND  Op # 12345678
// [REG] OR   12345678      , EAX
// [MEM] INC  EAX
// [REG] ADD  EAX + 12345678, EAX
void PrintInstruction(instruction *Instruction) {
    // Mode
    switch (Instruction->Mode) {
    case REG:
        Print("[REG");
        break;
    case MEM:
    case MEM_DISP8:
    case MEM_DISP32:
        Print("[MEM");
        break;
    case MODE_NONE:
        Print("[JMP");
        break;
    }
    // Bit-width
    if (Instruction->Is16) {
        Print(",32] ");
    } else {
        Print(", 8] ");
    }
    // Op
    if (Instruction->Type == JUMP) {
        Print(jmp_strings[Instruction->Op]);
    } else {
        Print(op_strings[Instruction->Op]);
    }
    Print(" ");
    // Args
    switch (Instruction->Type) {
    case JUMP:
        Print("Op # %x", (uint32_t)Instruction->JumpDestIdx);
        break;
    case NOARG:
        break;
    case ONE_REG:
        printFirstArg(Instruction);
        break;
    case TWO_REG:
        printFirstArg(Instruction);
        Print(", %s", reg_strings[Instruction->Src]);
        break;
    case REG_IMM:
        printFirstArg(Instruction);
        Print(", %x", (uint32_t)Instruction->Imm);
        break;
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

    // TODO: Add padding and hex ints to Print
    const char IntPaddingStr[BASE16_MAX_INT_STR+1] = "        ";

    for (size_t Idx = 0; Idx < NumInstructions; ++Idx) {
        instruction *Instruction = &Instructions[Idx];
        char IntBuf[BASE16_MAX_INT_STR];
        size_t IntLen;

        // Op #
        IntLen = WriteInt((uint32_t)Idx, IntBuf);
        IntBuf[IntLen] = '\0';
        Print(IntBuf);
        Print(IntPaddingStr + IntLen);

        PrintInstruction(Instruction);
        Print("\n");
    }
}

void PrintUnpackedOpcodes(opcode_unpacked *Opcodes, size_t NumOpcodes) {
    Print("Size %u Bytes\n\n", sizeof(opcode_unpacked) * NumOpcodes);
    Print("This is the instructions above in a struct that\n"
          "has real x86 codes instead of our enums.\n");
}

void PrintByteCode(uint8_t *Code, size_t Size, bool PrintSize = true, bool PrintNewlines = true) {
    if (PrintSize) {
        Print("Size: %u Bytes\n\n", Size);
    }
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
        if (PrintNewlines && ((i + 1) % 16 == 0 || i + 1 == Size)) {
            Print("\n");
        }
    }
}
