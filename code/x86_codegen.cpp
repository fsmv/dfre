// Copyright (c) 2016-2019 Andrew Kallmeyer <fsmv@sapium.net>
// Provided under the MIT License: https://mit-license.org

#include "nfa.h"
#include "x86_opcode.h"

#define NextInstr() (*((instruction*)(Alloc(Arena, sizeof(instruction)))))
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
    NextInstr() = RI8(BT, REG, EAX, 0, (uint8_t) DisableState); // Check if DisableState is active
    size_t Jump = PeekIdx();
    NextInstr() = J(JNC); // skip the following if it's not active

    NextInstr() = RI32(OR, REG, ECX, 0, ActivateMask); // Remember to activate the activate states

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
            if (DisableState != (uint32_t)-1) {
                GenInstructionsTransitionSet(DisableState, ActivateMask, Arena);
            }

            ActivateMask = (1 << Arc->To);
            DisableState = Arc->From;
        } else {
            ActivateMask |= (1 << Arc->To);
        }
    }

    if (DisableState != (uint32_t)-1) {
        GenInstructionsTransitionSet(DisableState, ActivateMask, Arena);
    }
}

// TODO: Document the overall structure of the assembly code
size_t GenerateInstructions(nfa *NFA, mem_arena *Arena) {
    // ebx = char *CurrChar
    // eax = uint32_t ActiveStates
    // ecx = uint32_t CurrentEnables
    // edx = uint32_t CurrentDisables

    NextInstr() = R32(PUSH, REG, EBX, 0); // Callee save
    NextInstr() = RR32(MOVR, MEM_DISP8, ESP, EBX, 8); // Get pointer to the search string off the stack

    NextInstr() = RI32(MOV, REG, EAX, 0, 1 << NFA_STARTSTATE); // Set start state as active

    size_t Top = PeekIdx();

    // Loop following epsilon arcs until following doesn't activate any new states
    size_t EpsilonLoopStart = PeekIdx();
    // Epsilon arcs, garunteed to be the first arc list
    // TODO: Is this premature opmtimisation? We could just search for epsilon
    // like we do below for the dot arc list.
    nfa_arc_list *EpsilonArcs = NFAFirstArcList(NFA);
    Assert(EpsilonArcs->Label.Type == EPSILON);
    NextInstr() = RR32(XOR, REG, ECX, ECX, 0); // Clear states to enable
    NextInstr() = RR32(MOV, REG, EDX, EAX, 0); // Save current active states list
    GenInstructionsArcList(EpsilonArcs, Arena);
    NextInstr() = RR32(OR, REG, EAX, ECX, 0);  // Enable the states to enable
    NextInstr() = RR32(CMP, REG, EDX, EAX, 0); // Check if active states has changed
    NextInstr() = JD(JNE, EpsilonLoopStart);

    NextInstr() = RI8(CMP, MEM, EBX, 0, 0); // If we're at the end, stop
    size_t JmpToEnd = PeekIdx();
    NextInstr() = J(JE);

    NextInstr() = RR32(XOR, REG, ECX, ECX, 0); // Clear states to enable
    NextInstr() = RR32(MOV, REG, EDX, EAX, 0); // Save current active states list
    NextInstr() = R32(NOT, REG, EDX, 0); // Remember to disable any currently active state

    nfa_arc_list *StartList = NFANextArcList(EpsilonArcs);

    // Dot arcs (only one possible)
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
        if (ArcList->Label.Type == RANGE) {
            NextInstr() = RI8(CMP, MEM, EBX, 0, ArcList->Label.A);
            size_t Jump1 = PeekIdx();
            NextInstr() = J(JL);

            NextInstr() = RI8(CMP, MEM, EBX, 0, ArcList->Label.B);
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
            NextInstr() = RI8(CMP, MEM, EBX, 0, ArcList->Label.A);
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

    NextInstr() = RR32(AND, REG, EAX, EDX, 0); // Disabled the states to disable
    NextInstr() = RR32(OR, REG, EAX, ECX, 0);  // Enable the states to enable

    NextInstr() = R32(INC, REG, EBX, 0); // Next char in string
    NextInstr() = JD(JMP, Top);

    Instr(JmpToEnd)->JumpDestIdx = PeekIdx();
    // Return != 0 if accept state was active, 0 otherwise
    NextInstr() = RI32(AND, REG, EAX, 0, 1 << NFA_ACCEPTSTATE);
    NextInstr() = R32(POP, REG, EBX, 0); // Callee save
    NextInstr() = RET;

    return PeekIdx();
}
