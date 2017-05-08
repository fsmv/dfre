// Copyright (c) 2016-2017 Andrew Kallmeyer <fsmv@sapium.net>
// Provided under the MIT License: https://mit-license.org

// TODO: Do we actually need a separate lexer?
#include "lexer.cpp"
#include "nfa.h"
#include "mem_arena.h"

nfa *NFAAllocArcList(mem_arena *Arena, nfa_label Label) {
    Alloc(Arena, sizeof(nfa_arc_list));
    nfa *NFA = (nfa *)Arena->Base;

    NFA->NumArcListsAllocated += 1;
    nfa_arc_list *ArcList = &NFA->ArcLists[NFA->NumArcLists++];
    ArcList->Label = Label;
    ArcList->NumTransitions = 0;
    return NFA;
}

nfa *NFAAddArc(mem_arena *Arena, nfa_label Label, nfa_transition Transition) {
    nfa *NFA = (nfa *)Arena->Base;
    nfa_arc_list *ArcListToUse = (nfa_arc_list *) 0;
    for (size_t ArcListIdx = 0; ArcListIdx < NFA->NumArcLists; ++ArcListIdx) {
        nfa_arc_list *TestArcList = &NFA->ArcLists[ArcListIdx];
        if (TestArcList->Label == Label &&
            TestArcList->NumTransitions < NFA_TRANSITIONS_PER_LIST)
        {
            ArcListToUse = TestArcList;
        }
    }

    if (!ArcListToUse) {
        NFA = NFAAllocArcList(Arena, Label);
        ArcListToUse = &NFA->ArcLists[NFA->NumArcLists - 1];
    }

    nfa_transition *TransitionLocation =
        &ArcListToUse->Transitions[ArcListToUse->NumTransitions++];
    *TransitionLocation = Transition;
    return NFA;
}

// Put arc lists with the same label next to eachother and combine the
// transition lists.
//
// Before: [ {EPSILON, 2, {trans_a, trans_b}}, {'A', 1, {trans_c}}, {EPSILON, 1, {trans_d}} ]
// After:  [ {EPSILON, 3, {trans_a, trans_b, trans_d}}, {'A', 1, {trans_c}} ]
//
// In NFA generation arcs lists are limited to NFA_TRANSITIONS_PER_LIST
// transitions each. To extend the list we simply add another arc list with the
// same label. To make looping over the transitions list in the code gen step
// easier, we want to pack those lists with the same label together with one,
// larger transition list.
//
// Combined ArcLists are not moved so that they're one after another. That is:
// if a list was made from 3 combined lists (has > 2 * NFA_TRANSITIONS_PER_LIST
// transitions), then that list will take up 3 * sizeof(nfa_arc_list) bytes.
void NFACombineArcLists(nfa *NFA) {
    size_t NumListsRemoved = 0;
    nfa_arc_list *CurrList = &NFA->ArcLists[0];
    // Skip the last one because it won't have any children
    for (size_t Idx = 0; Idx < NFA->NumArcLists - 1; ++Idx) {
        if (CurrList->NumTransitions < NFA_TRANSITIONS_PER_LIST) {
            CurrList += 1;
            continue;
        }

        size_t ChildrenFound = 0;
        nfa_arc_list *Child = CurrList + 1;
        nfa_arc_list *Swap = CurrList + 1;
        for (size_t CIdx = Idx; CIdx < NFA->NumArcLists; ++CIdx) {
            if (Child->Label != CurrList->Label) {
                Child += 1;
                continue;
            }

            // Move list after CurrList to Child pos and copy over transitions
            // from child
            nfa_arc_list ChildCopy = *Child;
            if (Child != Swap) {
                *Child = *Swap;
            }
            Swap += 1;
            nfa_transition *Dest = &CurrList->Transitions[CurrList->NumTransitions];
            for (size_t TIdx = 0; TIdx < ChildCopy.NumTransitions; ++TIdx) {
                *Dest++ = ChildCopy.Transitions[TIdx];
            }
            CurrList->NumTransitions += ChildCopy.NumTransitions;
            ChildrenFound += 1;

            if (ChildCopy.NumTransitions < NFA_TRANSITIONS_PER_LIST) {
                break;
            }
            Child += 1;
        }
        NumListsRemoved += ChildrenFound;
        CurrList += 1 + ChildrenFound;
    }

    NFA->NumArcLists -= NumListsRemoved;
}

struct chunk_bounds {
    uint32_t StartState;
    uint32_t EndState;
};

nfa *RegexToNFA(char *Regex, mem_arena *Arena) {
    Alloc(Arena, sizeof(nfa));
    nfa *NFA = (nfa *) Arena->Base;

    NFA->NumStates = 2; // Reserve 0 for accept, 1 for start
    NFA->NumArcLists = NFA->NumArcListsAllocated = 1; // Reserve 0 for epsilon
    nfa_label EpsilonLabel = {};
    EpsilonLabel.Type = EPSILON;
    NFA->ArcLists[0].Label = EpsilonLabel;
    NFA->ArcLists[0].NumTransitions = 0;

    // LastChunk.StartState == NFA_NULLSTATE means the last chunk cannot be looped
    chunk_bounds LastChunk{NFA_NULLSTATE, NFA_STARTSTATE};
    chunk_bounds ParenChunks[32];
    size_t NumOpenParens = 0;

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
                NFA = NFAAddArc(Arena, Label, Transition);

                LastChunk.StartState = LastChunk.EndState;
                LastChunk.EndState = MyState;
            } break;
            case '[': {
                uint32_t MyState = NFA->NumStates++;
                nfa_transition Transition = {};
                Transition.From = LastChunk.EndState;
                Transition.To = MyState;

                lexer_state Lexer{Token.Str};
                while (LexHasNextCharSetLabel(&Lexer)) {
                    nfa_label Label = LexNextCharSetLabel(&Lexer);
                    NFA = NFAAddArc(Arena, Label, Transition);
                }

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
                NFA = NFAAddArc(Arena, Label, Transition);

                Transition.From = LastChunk.EndState;
                Transition.To = MyState;
                NFA = NFAAddArc(Arena, Label, Transition);

                Transition.From = LastChunk.StartState;
                Transition.To = MyState;
                NFA = NFAAddArc(Arena, Label, Transition);

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
                NFA = NFAAddArc(Arena, Label, Transition);

                Transition.From = LastChunk.EndState;
                Transition.To = MyState;
                NFA = NFAAddArc(Arena, Label, Transition);

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
                NFA = NFAAddArc(Arena, Label, Transition);

                Transition.From = LastChunk.StartState;
                Transition.To = MyState;
                NFA = NFAAddArc(Arena, Label, Transition);

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
                NFA = NFAAddArc(Arena, Label, Transition);

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
                NFA = NFAAddArc(Arena, Label, Transition);

                Transition.From = OrChunk.StartState;
                Transition.To = NextOrSibling;
                NFA = NFAAddArc(Arena, Label, Transition);

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
                NFA = NFAAddArc(Arena, Label, Transition);

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
    NFA = NFAAddArc(Arena, Label, Transition);

    NFACombineArcLists(NFA);
    return NFA;
}
