// Copyright (c) 2016-2017 Andrew Kallmeyer <fsmv@sapium.net>
// Provided under the MIT License: https://mit-license.org

#include "nfa.h"
#include "x86_opcode.h"

#define NextInstr() (*((instruction*)(Arena->Base + Alloc(Arena, sizeof(instruction)))))
#define PeekIdx() (Arena->Used / sizeof(instruction))
#define Instr(idx) ((instruction*)(Arena->Base) + idx)

void NFASortArcList(nfa_arc_list *ArcList) {
    // TODO(fsmv): Replace with radix sort or something

    for (size_t ATransitionIdx = 0;
         ATransitionIdx < ArcList->NumTransitions;
         ++ATransitionIdx)
    {
        for (size_t BTransitionIdx = ATransitionIdx + 1;
             BTransitionIdx < ArcList->NumTransitions;
             ++BTransitionIdx)
        {
            if (ArcList->Transitions[ATransitionIdx].From >
                ArcList->Transitions[BTransitionIdx].From)
            {
                nfa_transition Temp = ArcList->Transitions[ATransitionIdx];
                ArcList->Transitions[ATransitionIdx] = 
                    ArcList->Transitions[BTransitionIdx];
                ArcList->Transitions[BTransitionIdx] = Temp;
            }
        }
    }
}

void GenInstructionsTransitionSet(uint32_t DisableState, uint32_t ActivateMask,
                                  mem_arena *Arena)
{
    NextInstr() = RI8(BT, REG, EBX, (uint8_t) DisableState); // Check if DisableState is active
    size_t Jump = PeekIdx();
    NextInstr() = J(JNC); // skip the following if it's not active

    NextInstr() = RI32(OR, REG, ECX, ActivateMask); // Remember to activate the activate states

    Instr(Jump)->JumpDestIdx = PeekIdx();
}

void GenInstructionsArcList(nfa_arc_list *ArcList, mem_arena *Arena) {
    NFASortArcList(ArcList);

    uint32_t DisableState = (uint32_t) -1;
    // TODO(fsmv): big int here
    uint32_t ActivateMask = 0;
    for (size_t TransitionIdx = 0;
         TransitionIdx < ArcList->NumTransitions;
         ++TransitionIdx)
    {
        nfa_transition *Arc = &ArcList->Transitions[TransitionIdx];

        if (Arc->From != DisableState) {
            if (DisableState != -1) {
                GenInstructionsTransitionSet(DisableState, ActivateMask, Arena);
            }

            ActivateMask = (1 << Arc->To);
            DisableState = Arc->From;
        } else {
            ActivateMask |= (1 << Arc->To);
        }
    }

    if (DisableState != -1) {
        GenInstructionsTransitionSet(DisableState, ActivateMask, Arena);
    }
}

/**
 * NOTE(fsmv):
 *   eax = char *SearchString        [input]
 *   ebx = uint32_t ActiveStates     [output]
 *   ecx = uint32_t CurrentEnables   [clobbered]
 *   edx = uint32_t CurrentDisables  [clobbered]
 */
size_t GenerateInstructions(nfa *NFA, mem_arena *Arena) {
    NextInstr() = RI32(MOV, REG, EBX, 1 << NFA_STARTSTATE); // Set state 0 as active

    size_t Top = PeekIdx();

    // Loop following epsilon arcs until following doesn't activate any new states
    size_t EpsilonLoopStart = PeekIdx();
    // Epsilon arcs, garunteed to be the first arc list
    nfa_arc_list *EpsilonArcs = NFAFirstArcList(NFA);
    Assert(EpsilonArcs->Label.Type == EPSILON);
    NextInstr() = RR32(XOR, REG, ECX, ECX); // Clear states to enable
    NextInstr() = RR32(MOV, REG, EDX, EBX); // Save current active states list
    GenInstructionsArcList(EpsilonArcs, Arena);
    NextInstr() = RR32(OR, REG, EBX, ECX);  // Enable the states to enable
    NextInstr() = RR32(CMP, REG, EDX, EBX); // Check if active states has changed
    NextInstr() = JD(JNE, EpsilonLoopStart);

    NextInstr() = RI8(CMP, MEM, EAX, 0); // If we're at the end, stop
    size_t JmpToEnd = PeekIdx();
    NextInstr() = J(JE);

    NextInstr() = RR32(XOR, REG, ECX, ECX); // Clear states to enable
    NextInstr() = RR32(MOV, REG, EDX, EBX); // Save current active states list
    NextInstr() = R32(NOT, REG, EDX); // Remember to disable any currently active state

    nfa_arc_list *StartList = NFANextArcList(EpsilonArcs);

    // Dot arcs
    nfa_arc_list *DotArcs = StartList;
    for (size_t Idx = 0; Idx < NFA->NumArcLists; ++Idx) {
        if (DotArcs->Label.Type == DOT) {
            break;
        }
        DotArcs = NFANextArcList(DotArcs);
    }
    GenInstructionsArcList(DotArcs, Arena);

    // Range arcs
    nfa_arc_list *ArcList = StartList;
    for (size_t ArcListIdx = 1; ArcListIdx < NFA->NumArcLists; ++ArcListIdx) {
        if (ArcList->Label.Type != RANGE) {
            NextInstr() = RI8(CMP, MEM, EAX, ArcList->Label.A);
            size_t Jump1 = PeekIdx();
            NextInstr() = J(JL);

            NextInstr() = RI8(CMP, MEM, EAX, ArcList->Label.B);
            size_t Jump2 = PeekIdx();
            NextInstr() = J(JG);

            GenInstructionsArcList(ArcList, Arena);

            Instr(Jump1)->JumpDestIdx = PeekIdx();
            Instr(Jump2)->JumpDestIdx = PeekIdx();
        }
        ArcList = NFANextArcList(ArcList);
    }

    // Match Arcs

    // Used for building a linked list to store which instructions need JumpDest filled in
    size_t StartMatchCharJmp = (size_t)-1;
    size_t LastMatchCharJmp = (size_t)-1;
    size_t LastMatchEndJmp = (size_t)-1;

    // Write test and jump for each letter to match. The cases of a switch statement
    ArcList = StartList;
    for (size_t ArcListIdx = 1; ArcListIdx < NFA->NumArcLists; ++ArcListIdx) {
        if (ArcList->Label.Type == MATCH) {
            NextInstr() = RI8(CMP, MEM, EAX, ArcList->Label.A);
            size_t NextMatchCharJmp = PeekIdx();
            NextInstr() = J(JE);
            // do a linked list so we can find where to fill in the jump dests
            if (LastMatchCharJmp != (size_t)-1) {
                Instr(LastMatchCharJmp)->JumpDestIdx = NextMatchCharJmp;
            } else {
                StartMatchCharJmp = NextMatchCharJmp;
            }
            LastMatchCharJmp = NextMatchCharJmp;
        }
        ArcList = NFANextArcList(ArcList);
    }

    // jump to the end if it's not a character we want to match
    {
        // again, do a linked list so we can fill in the jump dests
        size_t NextMatchEndJmp = PeekIdx();
        NextInstr() = J(JMP);
        Instr(NextMatchEndJmp)->JumpDestIdx = LastMatchEndJmp;
        LastMatchEndJmp = NextMatchEndJmp;
    }

    // Write the body of the switch statement for each char
    ArcList = StartList;
    for (size_t ArcListIdx = 1; ArcListIdx < NFA->NumArcLists; ++ArcListIdx) {
        if (ArcList->Label.Type == MATCH) {
            // Fill the last jump dest in the linked list with this location,
            // then advance the linked list
            size_t NextMatchCharJmp = Instr(StartMatchCharJmp)->JumpDestIdx;
            Instr(StartMatchCharJmp)->JumpDestIdx = PeekIdx();
            StartMatchCharJmp = NextMatchCharJmp;

            GenInstructionsArcList(ArcList, Arena);

            // again, do a linked list so we can fill in the jump dests
            size_t NextMatchEndJmp = PeekIdx();
            NextInstr() = J(JMP);
            Instr(NextMatchEndJmp)->JumpDestIdx = LastMatchEndJmp;
            LastMatchEndJmp = NextMatchEndJmp;
        }
        ArcList = NFANextArcList(ArcList);
    }

    // Fill in the break jump locations
    size_t MatchEnd = PeekIdx();
    while(LastMatchEndJmp != (size_t)-1) {
        size_t NextMatchEndJmp = Instr(LastMatchEndJmp)->JumpDestIdx;
        Instr(LastMatchEndJmp)->JumpDestIdx = MatchEnd;
        LastMatchEndJmp = NextMatchEndJmp;
    }

    NextInstr() = RR32(AND, REG, EBX, EDX); // Disabled the states to disable
    NextInstr() = RR32(OR, REG, EBX, ECX);  // Enable the states to enable

    NextInstr() = R32(INC, REG, EAX); // Next char in string
    NextInstr() = JD(JMP, Top);

    Instr(JmpToEnd)->JumpDestIdx = PeekIdx();
    // Return != 0 if accept state was active, 0 otherwise
    NextInstr() = RI32(AND, REG, EBX, 1 << NFA_ACCEPTSTATE);
    NextInstr() = RET;

    return PeekIdx();
}

int32_t ComputeJumpOffset(opcode_unpacked *UnpackedOpcodes, size_t JumpIdx, size_t JumpDestIdx) {
    size_t Start = 0, End = 0;
    if (JumpDestIdx < JumpIdx) { // backwards
        Start = JumpDestIdx;
        End = JumpIdx + 1;
    } else { // forwards (or equal)
        Start = JumpIdx + 1;
        End = JumpDestIdx;
    }

    size_t JumpOffset = 0;
    for (size_t Idx = Start; Idx < End; ++Idx) {
        JumpOffset += SizeOpcode(UnpackedOpcodes[Idx]);
    }

    int32_t Result = (int32_t) JumpOffset;
    if (JumpDestIdx < JumpIdx) { // backwards
        Result *= -1;
    }
    return Result;
}

void AssembleInstructions(instruction *Instructions, size_t NumInstructions, opcode_unpacked *UnpackedOpcodes) {
    opcode_unpacked *NextOpcode = UnpackedOpcodes;
    for (size_t Idx = 0; Idx < NumInstructions; ++Idx) {
        instruction *Inst = &Instructions[Idx];

        switch(Inst->Type) {
            case JUMP:
            {
                int32_t JumpDist = (int32_t) (Inst->JumpDestIdx - Idx);
                // We don't know how big the ops between here and the dest are yet
                int32_t JumpOffsetUpperBound = JumpDist * MAX_OPCODE_LEN;

                if (JumpOffsetUpperBound < -128 || JumpOffsetUpperBound > 127) {
                    *(NextOpcode++) = OpJump32(Inst->Op, 0);
                } else {
                    *(NextOpcode++) = OpJump8(Inst->Op, 0);
                }
            }break;
            case ONE_REG:
                *(NextOpcode++) = OpReg(Inst->Op, Inst->Mode, Inst->Dest, Inst->Int32);
                break;
            case TWO_REG:
                *(NextOpcode++) = OpRegReg(Inst->Op, Inst->Mode, Inst->Dest, Inst->Src, Inst->Int32);
                break;
            case REG_IMM:
                if (Inst->Int32) {
                    *(NextOpcode++) = OpRegI32(Inst->Op, Inst->Mode, Inst->Dest, Inst->Imm);
                } else {
                    *(NextOpcode++) = OpRegI8(Inst->Op, Inst->Mode, Inst->Dest, (uint8_t) Inst->Imm);
                }
                break;
            case NOARG:
                *(NextOpcode++) = OpNoarg(Inst->Op);
                break;
        }
    }

    for (size_t Idx = 0; Idx < NumInstructions; ++Idx) {
        instruction *Inst = &Instructions[Idx];
        if (Inst->Type == JUMP) {
            int32_t JumpOffset = ComputeJumpOffset(UnpackedOpcodes, Idx, Inst->JumpDestIdx);

            if (UnpackedOpcodes[Idx].ImmCount == 1) {
                Assert(-128 <= JumpOffset && JumpOffset <= 127);
                UnpackedOpcodes[Idx] = OpJump8(Inst->Op, (int8_t) JumpOffset);
            } else if (UnpackedOpcodes[Idx].ImmCount == 4) {
                UnpackedOpcodes[Idx] = OpJump32(Inst->Op, JumpOffset);
            }
        }
    }
}

size_t PackCode(opcode_unpacked *UnpackedOpcodes, size_t NumOpcodes, uint8_t *Code) {
    uint8_t *CodeStart = Code;

    for (size_t Idx = 0; Idx < NumOpcodes; ++Idx) {
        Code = WriteOpcode(UnpackedOpcodes[Idx], Code);
    }

    return (size_t) (Code - CodeStart);
}
