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

struct chunk_bounds {
    uint32_t StartState;
    uint32_t EndState;
};

void RegexToNFA(char *Regex, nfa *NFA) {
    NFA->NumStates = 2; // Reserve 0 for accept, 1 for start

    // LastChunk.StartState == NFA_NULLSTATE means the last chunk cannot be looped
    chunk_bounds LastChunk{NFA_NULLSTATE, NFA_STARTSTATE};
    chunk_bounds ParenChunks[32];
    size_t NumOpenParens = 0;

    // Epsilon is garunteed to be the first arc list for the code-gen step
    nfa_label EpsilonLabel = {};
    EpsilonLabel.Type = EPSILON;
    NFACreateArcList(NFA, EpsilonLabel);

    // Dot is garunteed to be the second arc list for the code-gen step
    nfa_label DotLabel = {};
    DotLabel.Type = DOT;
    NFACreateArcList(NFA, DotLabel);

    lexer_state Lexer{Regex};
    while(LexHasNext(&Lexer)) {
        token Token = LexNext(&Lexer);

        switch(*Token.Str) {
            case '.': {
                uint32_t MyState = NFA->NumStates++;

                nfa_label Label = {};
                Label.Type = DOT;
                nfa_transition Transition = {};

                Transition.From = LastChunk.EndState;
                Transition.To = MyState;
                NFAAddArc(NFA, Label, Transition);

                LastChunk.StartState = LastChunk.EndState;
                LastChunk.EndState = MyState;
            } break;
            case '*': {
                if (LastChunk.StartState == NFA_NULLSTATE) {
                    // TODO: report warning / error
                    break;
                }
                uint32_t MyState = NFA->NumStates++;

                nfa_transition Transition = {};
                nfa_label Label = {};
                Label.Type = EPSILON;

                Transition.From = LastChunk.EndState;
                Transition.To = LastChunk.StartState;
                NFAAddArc(NFA, Label, Transition);

                Transition.From = LastChunk.EndState;
                Transition.To = MyState;
                NFAAddArc(NFA, Label, Transition);

                Transition.From = LastChunk.StartState;
                Transition.To = MyState;
                NFAAddArc(NFA, Label, Transition);

                LastChunk.StartState = NFA_NULLSTATE;
                LastChunk.EndState = MyState;
            } break;
            case '+': {
                if (LastChunk.StartState == NFA_NULLSTATE) {
                    // TODO: report warning / error
                    break;
                }
                uint32_t MyState = NFA->NumStates++;

                nfa_label Label = {};
                Label.Type = EPSILON;
                nfa_transition Transition = {};

                Transition.From = LastChunk.EndState;
                Transition.To = LastChunk.StartState;
                NFAAddArc(NFA, Label, Transition);

                Transition.From = LastChunk.EndState;
                Transition.To = MyState;
                NFAAddArc(NFA, Label, Transition);

                LastChunk.StartState = NFA_NULLSTATE;
                LastChunk.EndState = MyState;
            } break;
            case '?': {
                if (LastChunk.StartState == NFA_NULLSTATE) {
                    // TODO: report warning / error
                    break;
                }
                uint32_t MyState = NFA->NumStates++;

                nfa_label Label = {};
                Label.Type = EPSILON;
                nfa_transition Transition = {};

                Transition.From = LastChunk.EndState;
                Transition.To = MyState;
                NFAAddArc(NFA, Label, Transition);

                Transition.From = LastChunk.StartState;
                Transition.To = MyState;
                NFAAddArc(NFA, Label, Transition);

                LastChunk.StartState = NFA_NULLSTATE;
                LastChunk.EndState = MyState;
            } break;
            case '(': {
                // TODO: Allocator
                Assert(NumOpenParens < ArrayLength(ParenChunks));

                uint32_t MyState = NFA->NumStates++;
                chunk_bounds *ParenChunk = &ParenChunks[NumOpenParens++];
                ParenChunk->StartState = LastChunk.EndState;
                ParenChunk->EndState = MyState;
            } break;
            case ')': {
                if (NumOpenParens == 0) {
                    // TODO: report warning / error
                    break;
                }
                chunk_bounds *MyChunk = &ParenChunks[--NumOpenParens];

                nfa_label Label = {};
                Label.Type = EPSILON;
                nfa_transition Transition = {};

                Transition.From = LastChunk.EndState;
                Transition.To = MyChunk->EndState;
                NFAAddArc(NFA, Label, Transition);

                LastChunk = *MyChunk;
            } break;
            case '|': {
                // If we see '|', we're at the end of the first or chunk sibling
                size_t NextOrSibling = NFA->NumStates++;

                chunk_bounds OrChunk{NFA_STARTSTATE, NFA_ACCEPTSTATE};
                if (NumOpenParens > 0) {
                    OrChunk = ParenChunks[NumOpenParens - 1];
                }

                nfa_label Label = {};
                Label.Type = EPSILON;
                nfa_transition Transition = {};

                Transition.From = LastChunk.EndState;
                Transition.To = OrChunk.EndState;
                NFAAddArc(NFA, Label, Transition);

                Transition.From = OrChunk.StartState;
                Transition.To = NextOrSibling;
                NFAAddArc(NFA, Label, Transition);

                LastChunk.StartState = NFA_NULLSTATE;
                LastChunk.EndState = NextOrSibling;
            } break;
            default: {
                uint32_t MyState = NFA->NumStates++;

                nfa_label Label = {};
                Label.Type = MATCH;
                Label.A = *Token.Str;

                nfa_transition Transition = {};
                Transition.From = LastChunk.EndState;
                Transition.To = MyState;
                NFAAddArc(NFA, Label, Transition);

                LastChunk.StartState = LastChunk.EndState;
                LastChunk.EndState = MyState;
            } break;
        }
    }
    nfa_label Label = {};
    Label.Type = EPSILON;
    nfa_transition Transition = {};

    Transition.From = LastChunk.EndState;
    Transition.To = NFA_ACCEPTSTATE;
    NFAAddArc(NFA, Label, Transition);
}
