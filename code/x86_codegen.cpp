// Copyright (c) 2016 Andrew Kallmeyer <fsmv@sapium.net>
// Provided under the MIT License: https://mit-license.org

#include "nfa.h"
#include "x86_opcode.h"

uint8_t *GenCodeLiteral(const char *String, uint8_t *Code, int32_t len = -1) {
    size_t i = 0;
    while(*String || (len != -1 && i < len)) {
        *Code++ = *String++;
        ++i;
    }

    return Code;
}

inline uint8_t *GenCodeInt32(uint32_t Int, uint8_t *Code) {
    *Code++ = (uint8_t) ((Int &       0xFF) >>  0);
    *Code++ = (uint8_t) ((Int &     0xFF00) >>  8);
    *Code++ = (uint8_t) ((Int &   0xFF0000) >> 16);
    *Code++ = (uint8_t) ((Int & 0xFF000000) >> 24);
    return Code;
}

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

#pragma warning(push)
#pragma warning(disable:4100) //TODO: LabelPrefix is unused
uint8_t *
GenCodeTransitionSet(uint32_t DisableState, uint32_t ActivateMask,
                     const char *LabelPrefix, uint8_t *Code)
{
    RI8(BT, MEM, EBX, (uint8_t) DisableState);

    // TODO: Do a save location then fill in jump later thing
    J8(JNC, 0x0C);
    //Code = GenCodeLiteral("jnc ", Code);
    //Code = GenCodeLiteral(LabelPrefix, Code);
    //Code += WriteInt(DisableState, (char*) Code);
    //*Code++ = '\n';

    uint32_t DisableMask = (1 << DisableState) ^ -1;
    RI32(AND, REG, EDX, DisableMask); // 2 + 4 bytes

    RI32(OR, REG, ECX, ActivateMask); // 2 + 4 bytes

    //Code = GenCodeLiteral(LabelPrefix, Code);
    //Code += WriteInt(DisableState, (char*) Code);
    //Code = GenCodeLiteral(":\n", Code);

    //Label(LabelPrefix, DisableState);

    return Code;
}
#pragma warning(pop)

uint8_t *
GenCodeArcList(nfa_arc_list *ArcList, const char *LabelPrefix, uint8_t *Code) {
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
                Code = GenCodeTransitionSet(DisableState, ActivateMask, LabelPrefix, Code);
            }

            ActivateMask = (1 << Arc->To);
            DisableState = Arc->From;
        } else {
            ActivateMask |= (1 << Arc->To);
        }
    }

    if (DisableState != -1) {
        Code = GenCodeTransitionSet(DisableState, ActivateMask, LabelPrefix, Code);
    }

    // TODO: these might need to be changed to REG. Text said MEM bits does REG
    RR32(AND, MEM, EBX, EDX);
    RR32(OR, MEM, EBX, ECX);

    return Code;
}

/**
 * NOTE(fsmv):
 *   eax = char *SearchString        [input]
 *   ebx = uint32_t ActiveStates     [output]
 *   ecx = uint32_t CurrentDisables  [clobbered]
 *   edx = uint32_t CurrentEnables   [clobbered]
 */
size_t GenerateCode(nfa *NFA, uint8_t *Code) {
    uint8_t *CodeStart = Code;

    //Code = GenCodeLiteral("CheckChar:\n", Code);

    RR32(XOR, REG, EBX, EBX);

    // Epsilon arcs, garunteed to be the first arc list
    //Code = GenCodeLiteral("; Epsilon Arcs\n", Code);
    nfa_arc_list *EpsilonArcs = NFAGetArcList(NFA, 0);
    Assert(EpsilonArcs->Label.Type == EPSILON);
    Code = GenCodeArcList(EpsilonArcs, "EpsilonArcs_", Code);

    // Dot arcs, garunteed to be the second arc list
    //Code = GenCodeLiteral("; Dot Arcs\n", Code);
    nfa_arc_list *DotArcs = NFAGetArcList(NFA, 1);
    Assert(DotArcs->Label.Type == DOT);
    Code = GenCodeArcList(DotArcs, "DotArcs_", Code);

    // Range arcs
    //Code = GenCodeLiteral("; Range Arcs\n", Code);
    for (size_t ArcListIdx = 2; ArcListIdx < NFA->ArcListCount; ++ArcListIdx) {
        nfa_arc_list *ArcList = NFAGetArcList(NFA, ArcListIdx);

        if (ArcList->Label.Type == RANGE) {
            char LabelIdent[3];
            LabelIdent[0] = ArcList->Label.A;
            LabelIdent[1] = ArcList->Label.B;
            LabelIdent[2] = '\0';

            char ArcListLabelPrefix[14];
            // TODO(fsmv): rename to stringcopy or something
            GenCodeLiteral("RangeArcs_", (uint8_t *)ArcListLabelPrefix);
            ArcListLabelPrefix[10] = ArcList->Label.A;
            ArcListLabelPrefix[11] = ArcList->Label.B;
            ArcListLabelPrefix[12] = '_';
            ArcListLabelPrefix[13] = '\0';

            RI8(CMP, MEM, EAX, ArcList->Label.A);

            //Code = GenCodeLiteral("jl  NotInRange_", Code);
            //Code = GenCodeLiteral(LabelIdent, Code);
            //Code = GenCodeLiteral("\n", Code);
            Code = GenCodeLiteral("\x7C\x00", Code);
            uint8_t *JumpSpot1 = Code;

            RI8(CMP, MEM, EAX, ArcList->Label.B);

            //Code = GenCodeLiteral("jg  NotInRange_", Code);
            //Code = GenCodeLiteral(LabelIdent, Code);
            //Code = GenCodeLiteral("\n", Code);
            Code = GenCodeLiteral("\x7F\x00", Code);
            uint8_t *JumpSpot2 = Code;

            Code = GenCodeArcList(ArcList, ArcListLabelPrefix, Code);

            *(JumpSpot1 - 1) = (uint8_t) (JumpSpot1 - Code);
            *(JumpSpot2 - 1) = (uint8_t) (JumpSpot2 - Code);

            //Code = GenCodeLiteral("NotInRange_", Code);
            //Code = GenCodeLiteral(LabelIdent, Code);
            //Code = GenCodeLiteral(":\n", Code);
        }
    }

    // Match arcs
    uint8_t *JumpLocations[(1 << 8) - 1];
    uint8_t *MatchEndJumpLocations[(1 << 8) - 1];
    size_t MatchEndJumpLocationsSize = 0;

    //Code = GenCodeLiteral("; Match Arcs\n", Code);
    for (size_t ArcListIdx = 2; ArcListIdx < NFA->ArcListCount; ++ArcListIdx) {
        nfa_arc_list *ArcList = NFAGetArcList(NFA, ArcListIdx);

        if (ArcList->Label.Type == MATCH) {
            char LabelIdent[2];
            LabelIdent[0] = ArcList->Label.A;
            LabelIdent[1] = '\0';

            RI8(CMP, MEM, EAX, ArcList->Label.A);

            //Code = GenCodeLiteral("je  Match_", Code);
            //Code = GenCodeLiteral(LabelIdent, Code);
            Code = GenCodeLiteral("\x0F\x84\xFF\xFF\xFF\xFF", Code);
            JumpLocations[ArcList->Label.A] = Code;
        }
    }

    //Code = GenCodeLiteral("jmp Match_End\n", Code);
    Code = GenCodeLiteral("\xE9\xFF\xFF\xFF\xFF", Code);
    MatchEndJumpLocations[MatchEndJumpLocationsSize++] = Code;

    for (size_t ArcListIdx = 1; ArcListIdx < NFA->ArcListCount; ++ArcListIdx) {
        nfa_arc_list *ArcList = NFAGetArcList(NFA, ArcListIdx);

        if (ArcList->Label.Type == MATCH) {
            char LabelIdent[2];
            LabelIdent[0] = ArcList->Label.A;
            LabelIdent[1] = '\0';

            char ArcListLabelPrefix[13];
            // TODO(fsmv): rename to stringcopy or something
            GenCodeLiteral("MatchArcs_", (uint8_t *)ArcListLabelPrefix);
            ArcListLabelPrefix[10] = ArcList->Label.A;
            ArcListLabelPrefix[11] = '_';
            ArcListLabelPrefix[12] = '\0';

            //Code = GenCodeLiteral("Match_", Code);
            //Code = GenCodeLiteral(LabelIdent, Code);
            //Code = GenCodeLiteral(":\n", Code);
            uint32_t JumpLen = (uint32_t) (Code - JumpLocations[ArcList->Label.A]);
            GenCodeInt32(JumpLen, JumpLocations[ArcList->Label.A] - 4);

            Code = GenCodeArcList(ArcList, ArcListLabelPrefix, Code);

            //Code = GenCodeLiteral("jmp Match_End\n", Code);
            Code = GenCodeLiteral("\xE9\xFF\xFF\xFF\xFF", Code);
            MatchEndJumpLocations[MatchEndJumpLocationsSize++] = Code;
        }
    }

    //Code = GenCodeLiteral("Match_End:\n", Code);
    for (int i = 0; i < MatchEndJumpLocationsSize; ++i) {
        uint32_t JumpLen = (uint32_t) (Code - MatchEndJumpLocations[i]);
        GenCodeInt32(JumpLen, MatchEndJumpLocations[i] - 4);
    }

    INC32(REG, EAX);
    RI8(CMP, MEM, EAX, 0);
    //Code = GenCodeLiteral("jne CheckChar\n", Code);
    Code = GenCodeLiteral("\x0F\x85\xFF\xFF\xFF\xFF", Code);
    GenCodeInt32((uint32_t)(CodeStart - Code), Code - 4);

    return Code - CodeStart;
}
