// Copyright (c) 2016-2017 Andrew Kallmeyer <fsmv@sapium.net>
// Provided under the MIT License: https://mit-license.org

struct lexer_state {
    char *Pos;
};

struct token {
    char *Str;
    int32_t Length;
};

bool LexHasNext(lexer_state *State) {
    bool Result = (*State->Pos != '\0');
    return Result;
}

#define ESCAPE_CHAR '\\'

token LexNext(lexer_state *State) {
    token Result = {};
    Result.Str = State->Pos;

    // TODO: add {m}, {m, n}, character classes

    switch (*State->Pos) {
        default:
        case '.':
        case '*':
        case '+':
        case '|':
        case '?':
        case '(':
        case ')':
        {
            Result.Length = 1;
            State->Pos += 1;
        } break;
        case '[': {
            // Skip to the ']' or end of string also handle escaping ']'
            char *Str;
            for (Str = State->Pos;
                 (*Str != ']' || *(Str-1) == ESCAPE_CHAR) && *(Str+1) != '\0';
                 ++Str) {}
            Result.Length = 1 + Str - State->Pos;
            State->Pos = Str + 1;
        } break;
        case ESCAPE_CHAR: {
            Result.Length = 1;
            if (*(State->Pos + 1) == '\0') {
                State->Pos += 1;
            } else {
                Result.Str += 1;
                State->Pos += 2;
            }
        } break;
        case '\0': {
            Result.Str = 0;
        } break;
    }

    return Result;
}
