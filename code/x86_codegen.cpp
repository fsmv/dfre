// Copyright (c) 2016-2017 Andrew Kallmeyer <fsmv@sapium.net>
// Provided under the MIT License: https://mit-license.org

#include "nfa.h"
#include "x86_opcode.h"

void NFASortArcList(nfa_arc_list *ArcList) {
    // TODO(fsmv): Replace with radix sort or something

    for (size_t ATransitionIdx = 0;
         ATransitionIdx < ArcList->TransitionCount;
         ++ATransitionIdx)
    {
        for (size_t BTransitionIdx = ATransitionIdx + 1;
             BTransitionIdx < ArcList->TransitionCount;
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

instruction *
GenInstructionsTransitionSet(uint32_t DisableState, uint32_t ActivateMask,
                             instruction *Instructions)
{
    RI8(BT, REG, EBX, (uint8_t) (DisableState - 1));
    instruction *Jump = J(JNC);

    uint32_t DisableMask = (1 << (DisableState - 1)) ^ -1;
    RI32(AND, REG, EDX, DisableMask); // 2 + 4 bytes

    RI32(OR, REG, ECX, ActivateMask); // 2 + 4 bytes

    Jump->JumpDest = Instructions;

    return Instructions;
}

instruction *
GenInstructionsArcList(nfa_arc_list *ArcList, instruction *Instructions) {
    RR32(XOR, REG, ECX, ECX);
    RI32(MOV, REG, EDX, 0xFFFFFFFF);

    NFASortArcList(ArcList);

    uint32_t DisableState = (uint32_t) -1;
    // TODO(fsmv): big int here
    uint32_t ActivateMask = 0;
    for (size_t TransitionIdx = 0;
         TransitionIdx < ArcList->TransitionCount;
         ++TransitionIdx)
    {
        nfa_transition *Arc = &ArcList->Transitions[TransitionIdx];

        if (Arc->From != DisableState) {
            if (DisableState != -1) {
                Instructions = GenInstructionsTransitionSet(DisableState, ActivateMask, Instructions);
            }

            ActivateMask = (1 << Arc->To);
            DisableState = Arc->From;
        } else {
            ActivateMask |= (1 << Arc->To);
        }
    }

    if (DisableState != -1) {
        Instructions = GenInstructionsTransitionSet(DisableState, ActivateMask, Instructions);
    }

    RR32(AND, REG, EBX, EDX);
    RR32(OR, REG, EBX, ECX);

    return Instructions;
}

/**
 * NOTE(fsmv):
 *   eax = char *SearchString        [input]
 *   ebx = uint32_t ActiveStates     [output]
 *   ecx = uint32_t CurrentDisables  [clobbered]
 *   edx = uint32_t CurrentEnables   [clobbered]
 */
size_t GenerateInstructions(nfa *NFA, instruction *Instructions) {
    instruction *InstructionsStart = Instructions;

    RI32(MOV, REG, EBX, 1);

    instruction *CheckCharLoc = Instructions;

    // Epsilon arcs, garunteed to be the first arc list
    nfa_arc_list *EpsilonArcs = NFAGetArcList(NFA, 0);
    Assert(EpsilonArcs->Label.Type == EPSILON);
    Instructions = GenInstructionsArcList(EpsilonArcs, Instructions);

    // Dot arcs, garunteed to be the second arc list
    nfa_arc_list *DotArcs = NFAGetArcList(NFA, 1);
    Assert(DotArcs->Label.Type == DOT);
    Instructions = GenInstructionsArcList(DotArcs, Instructions);

    // Range arcs
    for (size_t ArcListIdx = 2; ArcListIdx < NFA->ArcListCount; ++ArcListIdx) {
        nfa_arc_list *ArcList = NFAGetArcList(NFA, ArcListIdx);

        if (ArcList->Label.Type == RANGE) {
            RI8(CMP, MEM, EAX, ArcList->Label.A);
            instruction *Jump1 = J(JL);

            RI8(CMP, MEM, EAX, ArcList->Label.B);
            instruction *Jump2 = J(JG);

            Instructions = GenInstructionsArcList(ArcList, Instructions);

            Jump1->JumpDest = Instructions;
            Jump2->JumpDest = Instructions;
        }
    }

    // Match Arcs

    // Used for building a linked list to store which instructions need JumpDest filled in
    instruction *LastMatchCharJmp = 0;
    instruction *LastMatchEndJmp = 0;

    // Write test and jump for each letter to match. The cases of a switch statement
    for (size_t ArcListIdx = 2; ArcListIdx < NFA->ArcListCount; ++ArcListIdx) {
        nfa_arc_list *ArcList = NFAGetArcList(NFA, ArcListIdx);

        if (ArcList->Label.Type == MATCH) {
            RI8(CMP, MEM, EAX, ArcList->Label.A);
            instruction *NextMatchCharJmp = J(JE);
            // do a linked list so we can find where to fill in the jump dests
            NextMatchCharJmp->JumpDest = LastMatchCharJmp;
            LastMatchCharJmp = NextMatchCharJmp;
        }
    }

    // jump to the end if it's not a character we want to match
    {
        // again, do a linked list so we can fill in the jump dests
        instruction *NextMatchEndJmp = J(JMP);
        NextMatchEndJmp->JumpDest = LastMatchEndJmp;
        LastMatchEndJmp = NextMatchEndJmp;
    }

    // Write the body of the switch statement for each char
    // loop backwards because the linked list for MatchChar jumps is backwards
    for (size_t ArcListIdx = NFA->ArcListCount - 1; ArcListIdx >= 2; --ArcListIdx) {
        nfa_arc_list *ArcList = NFAGetArcList(NFA, ArcListIdx);

        if (ArcList->Label.Type == MATCH) {
            // Fill the last jump dest in the linked list with this location,
            // then advance the linked list
            instruction *NextMatchCharJmp = LastMatchCharJmp->JumpDest;
            LastMatchCharJmp->JumpDest = Instructions;
            LastMatchCharJmp = NextMatchCharJmp;

            Instructions = GenInstructionsArcList(ArcList, Instructions);

            // again, do a linked list so we can fill in the jump dests
            instruction *NextMatchEndJmp = J(JMP);
            NextMatchEndJmp->JumpDest = LastMatchEndJmp;
        }
    }

    // Fill in the break jump locations
    instruction *MatchEnd = Instructions;
    while(LastMatchEndJmp) {
        instruction *NextMatchEndJmp = LastMatchEndJmp->JumpDest;
        LastMatchEndJmp->JumpDest = MatchEnd;
        LastMatchEndJmp = NextMatchEndJmp;
    }

    INC32(REG, EAX);
    RI8(CMP, MEM, EAX, 0);
    instruction *JmpToCheckChar = J(JNE);
    JmpToCheckChar->JumpDest = CheckCharLoc;

    RI32(AND, REG, EBX, (1 >> NFA->NumStates));
    RET;

    return Instructions - InstructionsStart;
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
        instruction *Instr = &Instructions[Idx];

        switch(Instr->Type) {
            case JUMP:
            {
                int32_t JumpDist = (int32_t) (Instr->JumpDest - Instr);
                // We don't know how big the ops between here and the dest are yet
                int32_t JumpOffsetUpperBound = JumpDist * MAX_OPCODE_LEN;

                if (JumpOffsetUpperBound < -128 || JumpOffsetUpperBound > 127) {
                    *(NextOpcode++) = OpJump32(Instr->Op, 0);
                } else {
                    *(NextOpcode++) = OpJump8(Instr->Op, 0);
                }
            }break;
            case ONE_REG:
                *(NextOpcode++) = OpReg(Instr->Op, Instr->Mode, Instr->Dest, Instr->Int32);
                break;
            case TWO_REG:
                *(NextOpcode++) = OpRegReg(Instr->Op, Instr->Mode, Instr->Dest, Instr->Src, Instr->Int32);
                break;
            case REG_IMM:
                if (Instr->Int32) {
                    *(NextOpcode++) = OpRegI32(Instr->Op, Instr->Mode, Instr->Dest, Instr->Imm);
                } else {
                    *(NextOpcode++) = OpRegI8(Instr->Op, Instr->Mode, Instr->Dest, (uint8_t) Instr->Imm);
                }
                break;
            case NOARG:
                *(NextOpcode++) = OpNoarg(Instr->Op);
                break;
        }
    }

    for (size_t Idx = 0; Idx < NumInstructions; ++Idx) {
        instruction *Instr = &Instructions[Idx];
        if (Instr->Type == JUMP) {
            size_t JumpDestIdx = Idx + (Instr->JumpDest - Instr);
            int32_t JumpOffset = ComputeJumpOffset(UnpackedOpcodes, Idx, JumpDestIdx);

            if (UnpackedOpcodes[Idx].ImmCount == 1) {
                Assert(-128 <= JumpOffset && JumpOffset <= 127);
                UnpackedOpcodes[Idx] = OpJump8(Instr->Op, (int8_t) JumpOffset);
            } else if (UnpackedOpcodes[Idx].ImmCount == 4) {
                UnpackedOpcodes[Idx] = OpJump32(Instr->Op, JumpOffset);
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