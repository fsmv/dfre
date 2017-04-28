// Copyright (c) 2016-2017 Andrew Kallmeyer <fsmv@sapium.net>
// Provided under the MIT License: https://mit-license.org

// TODO: Do we actually need a separate lexer?
#include "lexer.cpp"
#include "nfa.h"

inline nfa_arc_list *NFACreateArcList(nfa *NFA, nfa_label Label) {
    nfa_arc_list *ArcList = NFAGetArcList(NFA, NFA->ArcListCount++);
    ArcList->Label = Label;
    ArcList->TransitionCount = 0;
    return ArcList;
}

void NFAAddArc(nfa *NFA, nfa_label Label, nfa_transition Transition) {
    nfa_arc_list *ArcListToUse = (nfa_arc_list *) 0;
    for (size_t ArcListIdx = 0; ArcListIdx < NFA->ArcListCount; ++ArcListIdx) {
        nfa_arc_list *TestArcList = NFAGetArcList(NFA, ArcListIdx);
        if (TestArcList->Label == Label) {
            ArcListToUse = TestArcList;
        }
    }

    if (!ArcListToUse) {
        ArcListToUse = NFACreateArcList(NFA, Label);
    }

    nfa_transition *TransitionLocation =
        &ArcListToUse->Transitions[ArcListToUse->TransitionCount++];
    *TransitionLocation = Transition;
}


struct ParenInfo {
    uint32_t LoopBackState;
    uint32_t AcceptState;
};

#define NULLSTATE ((uint32_t) -1)

void ReParse(char *regex, nfa *NFA) {
    NFA->NumStates = 2; // Reserve 0 for accept, 1 for start

    uint32_t LastState = NFA_STARTSTATE;
    uint32_t LoopBackState = NULLSTATE;
    ParenInfo ParenLoopBackStates[16]; // Paren accept state is this - 1
    size_t NumOpenParens = 0;

    // NOTE(fsmv): Epsilon is garunteed to be the first arc list for the code-gen step
    nfa_label EpsilonLabel = {};
    EpsilonLabel.Type = EPSILON;
    NFACreateArcList(NFA, EpsilonLabel);

    // NOTE(fsmv): Dot is garunteed to be the second arc list for the code-gen step
    nfa_label DotLabel = {};
    DotLabel.Type = DOT;
    NFACreateArcList(NFA, DotLabel);

    lexer_state Lexer = ReLexer(regex);
    while(ReLexerHasNext(&Lexer)) {
        token Token = ReLexerNext(&Lexer);

        switch(*Token.Str) {
            case '.': {
                LoopBackState = LastState;
                uint32_t MyState = NFA->NumStates++;

                nfa_label Label = {};
                Label.Type = DOT;

                nfa_transition Transition = {};
                Transition.From = LoopBackState;
                Transition.To = MyState;

                NFAAddArc(NFA, Label, Transition);
                LastState = MyState;
            } break;
            case '*': {
                // TODO: Report error, or ignore if nofail
                Assert(LoopBackState != NULLSTATE);

                nfa_label Label = {};
                Label.Type = EPSILON;

                nfa_transition Transition = {};
                Transition.From = LastState;
                Transition.To = LoopBackState;

                NFAAddArc(NFA, Label, Transition);

                uint32_t MyState = NFA->NumStates++;
                Transition.To = MyState; // last accept => new accept

                NFAAddArc(NFA, Label, Transition);

                Transition.From = LoopBackState; // loop back => new accept

                NFAAddArc(NFA, Label, Transition);

                LoopBackState = NULLSTATE;
                LastState = MyState;
            } break;
            case '+': {
                // TODO: Report error, or ignore if nofail
                Assert(LoopBackState != NULLSTATE);

                uint32_t MyState = NFA->NumStates++;
                nfa_label Label = {};
                Label.Type = EPSILON;
                nfa_transition Transition = {};

                Transition.From = LastState;
                Transition.To = LoopBackState;
                NFAAddArc(NFA, Label, Transition);
                // Must not end on a state with out transitions
                Transition.To = MyState;
                NFAAddArc(NFA, Label, Transition);

                LoopBackState = NULLSTATE;
                LastState = MyState;
            } break;
            case '?': {
                // TODO: Report error, or ignore if nofail
                Assert(LoopBackState != NULLSTATE);

                uint32_t MyState = NFA->NumStates++;

                nfa_label Label = {};
                Label.Type = EPSILON;
                nfa_transition Transition = {};

                Transition.From = LastState;
                Transition.To = MyState;
                NFAAddArc(NFA, Label, Transition);
                Transition.From = LoopBackState;
                NFAAddArc(NFA, Label, Transition);

                LoopBackState = NULLSTATE;
                LastState = MyState;
            } break;
            case '(': {
                Assert(NumOpenParens < ArrayLength(ParenLoopBackStates));

                ParenInfo *PInfo = &ParenLoopBackStates[NumOpenParens++];
                PInfo->LoopBackState = LastState;
                PInfo->AcceptState = NFA->NumStates++;
            } break;
            case ')': {
                // TODO: Report error, or ignore/count as a literal if nofail
                Assert(NumOpenParens > 0);

                ParenInfo *PInfo = &ParenLoopBackStates[--NumOpenParens];

                nfa_label Label = {};
                Label.Type = EPSILON;
                nfa_transition Transition = {};

                Transition.From = LastState;
                Transition.To = PInfo->AcceptState;
                NFAAddArc(NFA, Label, Transition);

                LoopBackState = PInfo->LoopBackState;
                LastState = PInfo->AcceptState;
            } break;
            case '|': {
                size_t NextOrSibling = NFA->NumStates++;

                uint32_t OrGroupStartState = NFA_STARTSTATE;
                uint32_t OrGroupAcceptState = NFA_ACCEPTSTATE;
                if (NumOpenParens > 0) {
                    ParenInfo *PInfo = &ParenLoopBackStates[NumOpenParens - 1];
                    OrGroupStartState = PInfo->LoopBackState;
                    OrGroupAcceptState = PInfo->AcceptState;
                }

                nfa_label Label = {};
                Label.Type = EPSILON;
                nfa_transition Transition = {};

                Transition.From = LastState;
                Transition.To = OrGroupAcceptState;
                NFAAddArc(NFA, Label, Transition);
                Transition.From = OrGroupStartState;
                Transition.To = NextOrSibling;
                NFAAddArc(NFA, Label, Transition);

                LoopBackState = NULLSTATE;
                LastState = NextOrSibling;
            } break;
            case '\\':
            default: {
                nfa_label Label = {};
                Label.Type = MATCH;

                nfa_transition Transition = {};
                for (char *c = Token.Str; (c - Token.Str) < Token.Length; ++c) {
                    // Move the the escaped char
                    if (*c == '\\' && (c - Token.Str) + 1 < Token.Length) {
                        c += 1;
                    }
                    Label.A = *c;

                    uint32_t MyState = NFA->NumStates++;
                    Transition.From = LastState;
                    Transition.To = MyState;
                    NFAAddArc(NFA, Label, Transition);

                    LoopBackState = Transition.From;
                    LastState = MyState;
                }
            } break;
        }
    }
    nfa_label Label = {};
    Label.Type = EPSILON;
    nfa_transition Transition = {};
    Transition.From = LastState;
    Transition.To = NFA_ACCEPTSTATE;
    NFAAddArc(NFA, Label, Transition);
}
