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

#define NULLSTATE ((uint32_t) -1)

void ReParse(char *regex, nfa *NFA) {
    NFA->NumStates = 1;

    uint32_t LoopBackState = NULLSTATE;

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
                LoopBackState = NFA->NumStates - 1;
                int MyState = NFA->NumStates++;

                nfa_label Label = {};
                Label.Type = DOT;

                nfa_transition Transition = {};
                Transition.From = LoopBackState;
                Transition.To = MyState;

                NFAAddArc(NFA, Label, Transition);
            } break;
            case '*': {
                Assert(LoopBackState != NULLSTATE);

                nfa_label Label = {};
                Label.Type = EPSILON;

                nfa_transition Transition = {};
                Transition.From = NFA->NumStates - 1;
                Transition.To = LoopBackState;

                NFAAddArc(NFA, Label, Transition);

                int MyState = NFA->NumStates++;
                Transition.To = MyState; // last accept => new accept

                NFAAddArc(NFA, Label, Transition);

                Transition.From = LoopBackState; // loop back => new accept

                NFAAddArc(NFA, Label, Transition);

                LoopBackState = NULLSTATE;
            } break;
            case '+': {
                Assert(LoopBackState != NULLSTATE);

                nfa_label Label = {};
                Label.Type = EPSILON;

                nfa_transition Transition = {};
                Transition.From = NFA->NumStates - 1;
                Transition.To = LoopBackState;

                NFAAddArc(NFA, Label, Transition);

                LoopBackState = NULLSTATE;
            } break;
            case '?': {
                Assert(LoopBackState != NULLSTATE);

                int LastAccept = NFA->NumStates - 1;
                int NewAccept = NFA->NumStates++;

                nfa_label Label = {};
                Label.Type = EPSILON;

                nfa_transition Transition = {};
                Transition.From = LastAccept;
                Transition.To = NewAccept;

                NFAAddArc(NFA, Label, Transition);

                Transition.From = LoopBackState; // loop back => new accept

                NFAAddArc(NFA, Label, Transition);

                LoopBackState = NULLSTATE;
            } break;

            //TODO: Impliment these:
            case '(':
            case ')':
            case '|': {
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

                    Transition.From = NFA->NumStates - 1;
                    Transition.To = NFA->NumStates++;

                    NFAAddArc(NFA, Label, Transition);

                    LoopBackState = Transition.From;
                }
            } break;
        }
    }
}
