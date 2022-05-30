// Copyright (c) 2016-2019 Andrew Kallmeyer <fsmv@sapium.net>
// Provided under the MIT License: https://mit-license.org

#include "nfa.h"
#include "x86_opcode.h"

#define DWORD_TO_BYTES 4

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

struct GeneratedInstructions {
  instruction *Instructions;
  size_t Count;

  //private:
  mem_arena *Arena;
  uint32_t NumStateDwords;
  uint32_t *ActivateMask;
};

instruction *NextInstr(GeneratedInstructions *ret) {
  instruction *Result = (instruction *)Alloc(ret->Arena, sizeof(instruction));
  ret->Count += 1;
  return Result;
}

void GenInstructionsTransitionSet(uint32_t FromState, GeneratedInstructions *ret) {
    // Note: bittest is weird. It takes a r/m32, imm8 argument. So we have to
    // use RI8 in this assembler API when it actually acts on a 32 bit dword.
    const int32_t FromDword = -1 * (FromState / 32);
    const uint8_t FromBit = FromState - (32*FromDword);
    *NextInstr(ret) = RI8(BT, MEM_DISP32, EBP, FromDword, FromBit); // Check if DisableState is active
    size_t Jump = ret->Count;
    *NextInstr(ret) = J(JNC); // skip the following if it's not active

    // Remember to activate the activate states
    for (size_t i = 0; i < ret->NumStateDwords; ++i) {
        if (ret->ActivateMask[i] == 0) {
          continue; // We can skip or-ing with 0, the mask is always the same
        }
        // Set EBP[CurrentEnables[i]] = ActivateMask[i] TODO: pass in the constant??
        *NextInstr(ret) = RI32(OR, MEM_DISP32, EBP, -1 * (ret->NumStateDwords + i) * DWORD_TO_BYTES, ret->ActivateMask[i]);
    }

    ret->Instructions[Jump].JumpDestIdx = ret->Count;
}

void GenInstructionsArcList(nfa_arc_list *ArcList, GeneratedInstructions *ret) {
    NFASortArcList(ArcList);

    uint32_t DisableState = (uint32_t) -1;
    for (size_t TransitionIdx = 0;
         TransitionIdx < ArcList->NumTransitions;
         ++TransitionIdx)
    {
        nfa_transition *Arc = &ArcList->Transitions[TransitionIdx];

        // Calculate where to set the bit in the ActivateMask
        const uint32_t ActivateDword = Arc->To / 32;
        const uint32_t ActivateBit = 1 << (Arc->To - (ActivateDword*32));

        if (Arc->From != DisableState) { // New from state
            // Write the transition set code at the end of each group of Arcs
            // that have the same Arc->From state
            if (DisableState != (uint32_t)-1) {
                GenInstructionsTransitionSet(DisableState, ret);
            }

            // Clear the activate mask, starting a new group of Arcs
            for (size_t i = 0; i < ret->NumStateDwords; ++i) {
                ret->ActivateMask[i] = 0;
            }
            DisableState = Arc->From; // Remember which group we're working on
        }
        ret->ActivateMask[ActivateDword] |= ActivateBit;
    }

    // Write the code for the last group of Arcs
    if (DisableState != (uint32_t)-1) {
        GenInstructionsTransitionSet(DisableState, ret);
    }
}


// TODO: Document the overall structure of the assembly code
GeneratedInstructions GenerateInstructions(nfa *NFA, mem_arena *Arena) {
    // ebx = char *CurrChar
    // ebp[0:NumStateBytes] = ActiveStates
    // ebp[NumStateBytes:2*NumStateBytes] = CurrentEnables
    // ebp[2*NumStateBytes:3*NumStateBytes] = CurrentDisables

    const uint32_t NumStateDwords = DivCeil(NFA->NumStates, 32);
    const uint32_t NumStateBytes = NumStateDwords * DWORD_TO_BYTES;
    // EBP byte offsets for these stack arrays arrays
    const int32_t ActiveStates = 0;
    const int32_t CurrentEnables = -1 * NumStateBytes;
    const int32_t CurrentDisables = -2 * NumStateBytes;

    GeneratedInstructions Result = {};
    Result.Instructions = (instruction *)(Arena->Base + NumStateBytes);
    Result.Arena = Arena;
    Result.NumStateDwords = NumStateDwords;
    Result.ActivateMask = (uint32_t *)Alloc(Arena, NumStateBytes);
    GeneratedInstructions *ret = &Result; // Just an alias for consistency

    *NextInstr(ret) = R32(PUSH, REG, EBP, 0); // Callee save
    *NextInstr(ret) = R32(PUSH, REG, EBX, 0); // Callee save
    *NextInstr(ret) = R32(PUSH, REG, ESI, 0); // Callee save
    *NextInstr(ret) = RR32(MOV, REG, EBP, ESP, 0); // save the start of the stack

    *NextInstr(ret) = RR32(MOVR, MEM_DISP32, ESP, EBX, 4*DWORD_TO_BYTES); // Get pointer to the search string off the stack

    *NextInstr(ret) = RI32(SUB, REG, ESP, 0, 3*NumStateBytes); // Make room for ActiveStates, CurrentEnables, CurrentDisables on the stack
    // Loop to clear the stack memory we just allocated
    *NextInstr(ret) = RR32(MOV, REG, ESI, EBP, 0); // We will increment ESI as we loop
    *NextInstr(ret) = RI32(MOV, REG, ECX, 0, 3*NumStateDwords); // Set the counter for the loop
    size_t ClearLoop = ret->Count;
    *NextInstr(ret) = RI32(MOV, MEM, ESI, 0, 0);
    *NextInstr(ret) = RI32(SUB, REG, ESI, 0, 4/*bytes_per_dword*/);
    *NextInstr(ret) = R32(DEC, REG, ECX, 0);
    *NextInstr(ret) = JD(JNE, ClearLoop);

    // Set the start state as active
    const int32_t StartStateDword = ActiveStates - (NFA->StartState / 32);
    const uint8_t StartStateBit = NFA->StartState - (32*StartStateDword);
    *NextInstr(ret) = RI32(MOV, MEM_DISP32, EBP, StartStateDword, 1 << StartStateBit);

    size_t Top = ret->Count;

    // Loop following epsilon arcs until following doesn't activate any new states
    size_t EpsilonLoopStart = ret->Count;
    // Epsilon arcs, garunteed to be the first arc list
    // TODO: Is this premature opmtimisation? We could just search for epsilon
    // like we do below for the dot arc list.
    nfa_arc_list *EpsilonArcs = NFAFirstArcList(NFA);
    Assert(EpsilonArcs->Label.Type == EPSILON);

    // (Ab)Use the CurrentDisables storage to save the state of the active states (unrolled loop)
    for (size_t i = 0; i < NumStateDwords; ++i) {
      *NextInstr(ret) = RR32(MOVR, MEM_DISP32, EBP, EAX, ActiveStates - i*DWORD_TO_BYTES);
      *NextInstr(ret) = RR32(MOV, MEM_DISP32, EBP, EAX, CurrentDisables - i*DWORD_TO_BYTES);
    }
    GenInstructionsArcList(EpsilonArcs, ret);

    // Unrolled loop to enable the states from the epsilon arcs
    for (size_t i = 0; i < NumStateDwords; ++i) {
      *NextInstr(ret) = RR32(MOVR, MEM_DISP32, EBP, EAX, CurrentEnables - i*DWORD_TO_BYTES);
      *NextInstr(ret) = RR32(OR, MEM_DISP32, EBP, EAX, ActiveStates - i*DWORD_TO_BYTES);  // Enable the states to enable
    }

    // Unrolled loop to check if we're done iterating on the epsilon arcs.
    //
    // Stops jumping back to the top when the epsilon arcs did not activate any
    // new states.
    for (size_t i = 0; i < NumStateDwords; ++i) {
      *NextInstr(ret) = RR32(MOVR, MEM_DISP32, EBP, EAX, CurrentDisables - i*DWORD_TO_BYTES); // Copy the Disable byte to eax
      *NextInstr(ret) = RR32(CMP, MEM_DISP32, EBP, EAX, ActiveStates - i*DWORD_TO_BYTES); // Check if active states has changed
      *NextInstr(ret) = JD(JNE, EpsilonLoopStart); // jump to the top if changed
    }

    // If we found the end of the string, stop processing now
    *NextInstr(ret) = RI8(CMP, MEM, EBX, 0, 0);
    size_t JmpToEnd = ret->Count;
    *NextInstr(ret) = J(JE);

    // Clear states to enable
    for (size_t i = 0; i < NumStateDwords; ++i) {
      *NextInstr(ret) = RI32(MOV, MEM_DISP32, EBP, CurrentEnables - i*DWORD_TO_BYTES, 0);
    }
    // Set CurrentDisables to disable the active states, before we consume input
    for (size_t i = 0; i < NumStateDwords; ++i) {
      *NextInstr(ret) = RR32(MOVR, MEM_DISP32, EBP, EAX, ActiveStates - i*DWORD_TO_BYTES);
      *NextInstr(ret) = RR32(MOV, MEM_DISP32, EBP, EAX, CurrentDisables - i*DWORD_TO_BYTES);
      *NextInstr(ret) = R32(NOT, MEM_DISP32, EBP, CurrentDisables - i*DWORD_TO_BYTES);
    }

    nfa_arc_list *StartList = NFANextArcList(EpsilonArcs);

    // Dot arcs (only one possible)
    nfa_arc_list *DotArcs = StartList;
    for (size_t Idx = 0; Idx < NFA->NumArcLists; ++Idx) {
        if (DotArcs->Label.Type == DOT) {
            break;
        }
        DotArcs = NFANextArcList(DotArcs);
    }
    GenInstructionsArcList(DotArcs, ret);

    // Range arcs
    nfa_arc_list *ArcList = StartList;
    for (size_t ArcListIdx = 1; ArcListIdx < NFA->NumArcLists; ++ArcListIdx) {
        if (ArcList->Label.Type == RANGE) {
            *NextInstr(ret) = RI8(CMP, MEM, EBX, 0, ArcList->Label.A);
            size_t Jump1 = ret->Count;
            *NextInstr(ret) = J(JL);

            *NextInstr(ret) = RI8(CMP, MEM, EBX, 0, ArcList->Label.B);
            size_t Jump2 = ret->Count;
            *NextInstr(ret) = J(JG);

            GenInstructionsArcList(ArcList, ret);

            ret->Instructions[Jump1].JumpDestIdx = ret->Count;
            ret->Instructions[Jump2].JumpDestIdx = ret->Count;
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
            *NextInstr(ret) = RI8(CMP, MEM, EBX, 0, ArcList->Label.A);
            size_t NextMatchCharJmp = ret->Count;
            *NextInstr(ret) = J(JE);
            // do a linked list so we can find where to fill in the jump dests
            if (LastMatchCharJmp != (size_t)-1) {
                ret->Instructions[LastMatchCharJmp].JumpDestIdx = NextMatchCharJmp;
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
        size_t NextMatchEndJmp = ret->Count;
        *NextInstr(ret) = J(JMP);
        ret->Instructions[NextMatchEndJmp].JumpDestIdx = LastMatchEndJmp;
        LastMatchEndJmp = NextMatchEndJmp;
    }

    // Write the body of the switch statement for each char
    ArcList = StartList;
    for (size_t ArcListIdx = 1; ArcListIdx < NFA->NumArcLists; ++ArcListIdx) {
        if (ArcList->Label.Type == MATCH) {
            // Fill the last jump dest in the linked list with this location,
            // then advance the linked list
            size_t NextMatchCharJmp = ret->Instructions[StartMatchCharJmp].JumpDestIdx;
            ret->Instructions[StartMatchCharJmp].JumpDestIdx = ret->Count;
            StartMatchCharJmp = NextMatchCharJmp;

            GenInstructionsArcList(ArcList, ret);

            // again, do a linked list so we can fill in the jump dests
            size_t NextMatchEndJmp = ret->Count;
            *NextInstr(ret) = J(JMP);
            ret->Instructions[NextMatchEndJmp].JumpDestIdx = LastMatchEndJmp;
            LastMatchEndJmp = NextMatchEndJmp;
        }
        ArcList = NFANextArcList(ArcList);
    }

    // Fill in the break jump locations
    size_t MatchEnd = ret->Count;
    while(LastMatchEndJmp != (size_t)-1) {
        size_t NextMatchEndJmp = ret->Instructions[LastMatchEndJmp].JumpDestIdx;
        ret->Instructions[LastMatchEndJmp].JumpDestIdx = MatchEnd;
        LastMatchEndJmp = NextMatchEndJmp;
    }

    // Disable the states to disable, and enable the states to enable (unrolled)
    for (size_t i = 0; i < NumStateDwords; ++i) {
      *NextInstr(ret) = RR32(MOVR, MEM_DISP32, EBP, EAX, CurrentDisables - i*DWORD_TO_BYTES);
      *NextInstr(ret) = RR32(AND, MEM_DISP32, EBP, EAX, ActiveStates - i*DWORD_TO_BYTES); // Disable the states to disable
      *NextInstr(ret) = RR32(MOVR, MEM_DISP32, EBP, EAX, CurrentEnables - i*DWORD_TO_BYTES);
      *NextInstr(ret) = RR32(OR, MEM_DISP32, EBP, EAX, ActiveStates - i*DWORD_TO_BYTES); // Enable the states to enable
    }

    *NextInstr(ret) = R32(INC, REG, EBX, 0); // Next char in string
    *NextInstr(ret) = JD(JMP, Top);

    ret->Instructions[JmpToEnd].JumpDestIdx = ret->Count;
    // Return != 0 in eax if accept state was active, 0 otherwise
    *NextInstr(ret) = RR32(MOVR, MEM_DISP32, EBP, EAX, ActiveStates);
    *NextInstr(ret) = RI32(AND, REG, EAX, 0, 1 << NFA_ACCEPTSTATE);

    *NextInstr(ret) = RR32(MOV, REG, ESP, EBP, 0); // restore the stack
    *NextInstr(ret) = R32(POP, REG, ESI, 0); // Callee save
    *NextInstr(ret) = R32(POP, REG, EBX, 0); // Callee save
    *NextInstr(ret) = R32(POP, REG, EBP, 0); // Callee save
    *NextInstr(ret) = RET;

    return Result;
}
