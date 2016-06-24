// Copyright (c) 2016 Andrew Kallmeyer <fsmv@sapium.net>
// Provided under the MIT License: https://mit-license.org

#include "lexer.cpp"

struct nfa_transition {
    uint32_t From;
    uint32_t To;
};

enum nfa_arc_type {
    MATCH = 0, DOT, EPSILON, RANGE
};

struct nfa_label {
    nfa_arc_type Type;
    char A;
    char B;
};

struct nfa_arc_list {
    nfa_label Label;
    size_t TransitionCount;
    nfa_transition Transitions[1];
};

struct nfa {
    uint32_t NumStates;
    size_t ArcListCount;
    size_t SizeOfArcList;
    nfa_arc_list ArcLists[1];
};

bool operator==(nfa_label A, nfa_label B) {
    bool Result = (A.Type == B.Type);
    Result     &= (A.A == B.A);
    Result     &= (A.B == B.B);

    return Result;
}

inline nfa_arc_list *NFAGetArcList(nfa *NFA, size_t Idx) {
    uint8_t *Result = (uint8_t *) NFA->ArcLists
                    + Idx * NFA->SizeOfArcList;
    return (nfa_arc_list *) Result;
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
        ArcListToUse = NFAGetArcList(NFA, NFA->ArcListCount++);
        ArcListToUse->Label = Label;
        ArcListToUse->TransitionCount = 0;
    }

    nfa_transition *TransitionLocation =
        &ArcListToUse->Transitions[ArcListToUse->TransitionCount++];
    *TransitionLocation = Transition;
}

void ReParse(char *regex, nfa *NFA) {
    NFA->NumStates = 1;

    uint32_t LoopBackState = -1;

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
                Assert(LoopBackState != -1);

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

                LoopBackState = -1;
            } break;
            case '+': {
                Assert(LoopBackState != -1);

                nfa_label Label = {};
                Label.Type = EPSILON;

                nfa_transition Transition = {};
                Transition.From = NFA->NumStates - 1;
                Transition.To = LoopBackState;

                NFAAddArc(NFA, Label, Transition);

                LoopBackState = -1;
            } break;
            case '?': {
                Assert(LoopBackState != -1);

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

                LoopBackState = -1;
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
