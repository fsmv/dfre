// Copyright (c) 2016 Andrew Kallmeyer <fsmv@sapium.net>
// Provided under the MIT License: https://mit-license.org

#ifndef NFA_H
#define NFA_H

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

inline bool operator==(nfa_label A, nfa_label B) {
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

#endif
