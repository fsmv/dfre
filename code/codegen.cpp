// Copyright (c) 2016 Andrew Kallmeyer <fsmv@sapium.net>
// Provided under the MIT License: https://mit-license.org

#include "nfa.h"

uint8_t *GenCodeLiteral(const char *String, uint8_t *Code) {
    while(*String) {
        *Code++ = *String++;
    }

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

uint8_t *
GenCodeTransitionSet(uint32_t DisableState, uint32_t ActivateMask,
                     const char *LabelPrefix, uint8_t *Code)
{
    Code = GenCodeLiteral("bt  [ebx], ", Code);
    Code += WriteInt(DisableState, (char*) Code);
    *Code++ = '\n';

    Code = GenCodeLiteral("jnc ", Code);
    Code = GenCodeLiteral(LabelPrefix, Code);
    Code += WriteInt(DisableState, (char*) Code);
    *Code++ = '\n';

    Code = GenCodeLiteral("and edx, ", Code);
    uint32_t DisableMask = (1 << DisableState) ^ -1;
    Code += WriteInt(DisableMask, (char*) Code);
    *Code++ = '\n';

    Code = GenCodeLiteral("or  ecx, ", Code);
    Code += WriteInt(ActivateMask, (char*) Code);
    *Code++ = '\n';

    Code = GenCodeLiteral(LabelPrefix, Code);
    Code += WriteInt(DisableState, (char*) Code);
    Code = GenCodeLiteral(":\n", Code);

    return Code;
}

uint8_t *
GenCodeArcList(nfa_arc_list *ArcList, const char *LabelPrefix, uint8_t *Code) {
    Code = GenCodeLiteral("xor ecx, ecx\n", Code);
    Code = GenCodeLiteral("mov edx, -1\n", Code);

    NFASortArcList(ArcList);

    uint32_t DisableState = -1;
    // TODO(fsmv): big int here
    uint32_t ActivateMask = 0;
    for (size_t TransitionIdx = 0;
         TransitionIdx < ArcList->TransitionCount;
         ++TransitionIdx)
    {
        nfa_transition *Arc = &ArcList->Transitions[TransitionIdx];

        if (Arc->From != DisableState) {
            // TODO(fsmv): DisableState isn't correct in the code output
            Code = GenCodeTransitionSet(DisableState, ActivateMask, LabelPrefix, Code);

            ActivateMask = (1 << Arc->To);
            DisableState = Arc->From;
        } else {
            ActivateMask |= (1 << Arc->To);
        }
    }

    if (DisableState != -1) {
        Code = GenCodeTransitionSet(DisableState, ActivateMask, LabelPrefix, Code);
    }

    Code = GenCodeLiteral("and [ebx], edx\n", Code);
    Code = GenCodeLiteral("or  [ebx], ecx\n", Code);

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

    Code = GenCodeLiteral("CheckChar:\n", Code);

    // Epsilon arcs, garunteed to be the first arc list
    Code = GenCodeLiteral("; Epsilon Arcs\n", Code);
    nfa_arc_list *EpsilonArcs = NFAGetArcList(NFA, 0);
    Assert(EpsilonArcs->Label.Type == EPSILON);
    Code = GenCodeArcList(EpsilonArcs, "EpsilonArcs_", Code);

    // Dot arcs, garunteed to be the second arc list
    Code = GenCodeLiteral("; Dot Arcs\n", Code);
    nfa_arc_list *DotArcs = NFAGetArcList(NFA, 1);
    Assert(DotArcs->Label.Type == DOT);
    Code = GenCodeArcList(DotArcs, "DotArcs_", Code);

    // Range arcs
    Code = GenCodeLiteral("; Range Arcs\n", Code);
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

            Code = GenCodeLiteral("cmp [eax], '", Code);
            *Code++ = ArcList->Label.A;
            Code = GenCodeLiteral("'\n", Code);

            Code = GenCodeLiteral("jl  NotInRange_", Code);
            Code = GenCodeLiteral(LabelIdent, Code);
            Code = GenCodeLiteral("\n", Code);

            Code = GenCodeLiteral("cmp [eax], '", Code);
            *Code++ = ArcList->Label.B;
            Code = GenCodeLiteral("'\n", Code);

            Code = GenCodeLiteral("jg  NotInRange_", Code);
            Code = GenCodeLiteral(LabelIdent, Code);
            Code = GenCodeLiteral("\n", Code);

            Code = GenCodeArcList(ArcList, ArcListLabelPrefix, Code);

            Code = GenCodeLiteral("NotInRange_", Code);
            Code = GenCodeLiteral(LabelIdent, Code);
            Code = GenCodeLiteral(":\n", Code);
        }
    }

    // Match arcs
    Code = GenCodeLiteral("; Match Arcs\n", Code);
    for (size_t ArcListIdx = 2; ArcListIdx < NFA->ArcListCount; ++ArcListIdx) {
        nfa_arc_list *ArcList = NFAGetArcList(NFA, ArcListIdx);

        if (ArcList->Label.Type == MATCH) {
            char LabelIdent[2];
            LabelIdent[0] = ArcList->Label.A;
            LabelIdent[1] = '\0';

            Code = GenCodeLiteral("cmp [eax], '", Code);
            *Code++ = ArcList->Label.A;
            Code = GenCodeLiteral("'\n", Code);

            Code = GenCodeLiteral("je  Match_", Code);
            Code = GenCodeLiteral(LabelIdent, Code);
            Code = GenCodeLiteral("\n", Code);
        }
    }

    Code = GenCodeLiteral("jmp Match_End\n", Code);

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

            Code = GenCodeLiteral("Match_", Code);
            Code = GenCodeLiteral(LabelIdent, Code);
            Code = GenCodeLiteral("\n", Code);

            Code = GenCodeArcList(ArcList, ArcListLabelPrefix, Code);

            Code = GenCodeLiteral("jmp Match_End\n", Code);
        }
    }

    Code = GenCodeLiteral("Match_End:\n", Code);

    // Loop control
    Code = GenCodeLiteral("inc eax\n", Code);
    Code = GenCodeLiteral("cmp [eax], 0\n", Code);
    Code = GenCodeLiteral("jne CheckChar\n", Code);

    return Code - CodeStart;
}
