// Copyright (c) 2016-2019 Andrew Kallmeyer <fsmv@sapium.net>
// Provided under the MIT License: https://mit-license.org

// TODO: put all the NFA handling functions together
// They're not currently in this file because some are only needed in one place
// Also some work on an Arena pointer instead of an nfa pointer, maybe make nfa_arena.h
#ifndef NFA_H_

// +---------------------------------------------------+
// | Read struct documentation bottom to top because C |
// +---------------------------------------------------+

// One arc (arrow), without the label
struct nfa_transition {
    uint32_t From;
    uint32_t To;
};

/**
 * Arc labels can have multiple types
 *
 *  - There are mulitple types of labels that can be on an arc.
 *  - For all arc types but the epsilon arc, the current character in the input
 *    string is consumed, i.e. the current character is advanced to the next
 *    character in the string.
 *
 * MATCH   := match and consume a specific character
 *   A := The specific character to match
 *   B := Ignored
 * DOT     := match and consume any character
 *   A := Ignored
 *   B := Ignored
 * RANGE   := match and consume characters that fall within a specified range
 *   A := The start of the range to match
 *   B := The end of the range to match
 * EPSILON := match any character, does not consume a character.
 *   A := Ignored
 *   B := Ignored
 */
enum nfa_label_type {
    MATCH = 0, DOT, EPSILON, RANGE
};

// One label the goes with a group of transitions (the label on an arc)
struct nfa_label {
    // TODO: Should we find a way to enforce that different types don't use A and B?
    //       Maybe a union of structs for each type?
    nfa_label_type Type;

    // Data used differently by the different label types
    // See nfa_label_type documentation
    char A;
    char B;
};

// The number of transitions an arc_list holds when stack allocated.
//
// When generating the NFA, we add arc_list chunks to the end of the arena
// allowing multiple arc_lists with the same label. Later the chunks are
// combined in NFACombineArcLists(nfa)
#define NFA_TRANSITIONS_PER_LIST_CHUNK 8
/**
 * A list of arcs in the NFA which have the same label.
 *
 * Here, arcs are split into separate label and transition structures
 */
struct nfa_arc_list {
    nfa_label Label;
    size_t NumTransitions;
    nfa_transition Transitions[NFA_TRANSITIONS_PER_LIST_CHUNK];
};

/**
 * A representation of an NFA (non-deterministic finite automata) state machine.
 * If unfamiliar, please search online for an image of the usual circles and
 * arrows picture of an NFA.
 *
 * What is the NFA Model?
 * ----------------------
 *
 *  - NFAs have a list of states (circles), and arcs (arrows) between the states.
 *
 *  - The idea is, we start at one state and follow arcs to other states
 *    conditionally based on the label on the arc.
 *
 *  - Normally, in following an arc, a character of the input is consumed.
 *
 *  - If, when all characters of the input string have been consumed we have
 *    ended in a state marked accept, then the string matches
 *    (or in other words: the string is in the language described by the NFA).
 *
 *  - Arcs can only be traveled over if the current character in the input
 *    string meets the requirements of the label on the arc.
 *
 *  - The NFA can be in multiple states at once (if it was deterministic
 *    instead, it would only be able to be in one state at a time). As such
 *    sometimes we refer to states the NFA is in as active states.
 *
 * This structure
 * --------------
 *
 *  - In this representation, we store multiple arcs with the same label
 *    together. We store one label with a list of arcs.
 *    - This is done because we have many arcs with the same labels and this
 *      structure mirrors the structure of thefinal assembly code generated.
 *
 *  - For representing states, we simply use an integer index number.
 *    - Since states need no extra information, we don't need to store an array
 *      of them. We only need to remember how many there are.
 */
struct nfa {
    uint32_t NumStates;

    size_t NumArcListsAllocated;
    size_t NumArcLists;
    // We allocate extra space at the end of the struct for this array
    nfa_arc_list _ArcLists[1];
};

// Check equality of nfa_label structs
inline bool operator==(nfa_label A, nfa_label B) {
    bool Result = (A.Type == B.Type);
    Result     &= (A.A == B.A);
    Result     &= (A.B == B.B);

    return Result;
}

// Check equality of nfa_label structs
inline bool operator!=(nfa_label A, nfa_label B) {
    return !(A == B);
}

nfa_arc_list *NFAFirstArcList(nfa *NFA) {
    return &NFA->_ArcLists[0];
}

// Handles both combined and uncombined NFAs
// See also NFACombineArcLists in parser.cpp
nfa_arc_list *NFANextArcList(nfa_arc_list *ArcList) {
    // TODO: Maybe just store the number of arc list slots this list takes up.
    //       If we did that, we wouldn't have these branches here.
    size_t ExtraTransitions = 0;
    if (ArcList->NumTransitions > NFA_TRANSITIONS_PER_LIST_CHUNK) {
        ExtraTransitions = ArcList->NumTransitions - NFA_TRANSITIONS_PER_LIST_CHUNK;
    }
    size_t ExtraArcLists = ExtraTransitions / NFA_TRANSITIONS_PER_LIST_CHUNK;
    if (ExtraTransitions % NFA_TRANSITIONS_PER_LIST_CHUNK != 0) {
        ExtraArcLists += 1;
    }
    return ArcList + 1 + ExtraArcLists;
}

#define NFA_NULLSTATE ((uint32_t) -1)
#define NFA_STARTSTATE  ((uint32_t) 1)
#define NFA_ACCEPTSTATE ((uint32_t) 0)

#define NFA_H_
#endif
