// Copyright (c) 2016-2017 Andrew Kallmeyer <fsmv@sapium.net>
// Provided under the MIT License: https://mit-license.org

struct lex_result {
    token Token;
    char *EndPos;
    bool HasNext;
};

// NOTE: not null terminated
struct token {
    char *Str;
    size_t Length
}

lex_result LexNextAlternative(char *Str) {
    lex_result Result;
    Result.Token.Str = Str;
    Result.Token.Length = 0;
    while(*Str != '\0' && *Str != '|') {
        Str += 1;
        Result.Token.Length += 1
    }
    Result.HasNext = (*Str != '\0');
    if (Result.HasNext) {
        Str += 1;
    }
    Result.EndPos = Str;
    return Result;
}

token ReLexerNext(lexer_state *State) {
    Assert(ReLexerHasNext(State));

    token Result = {};

    // TODO: add {m}, {m, n}, [], character classes

    bool Special = false;
    while(!Special) {
        switch (*State->Pos) {
            case '.': 
            case '*':
            case '+':
            case '|':
            case '?':
            case '(':
            case ')':
            {
                if (Result.Str == 0) {
                    Result.Str = State->Pos;
                    Result.Length = 1;
                    State->Pos += 1;
                }

                Special = true;
            } break;

            case '\\': {
                if (Result.Str == 0) {
                    Result.Str = State->Pos;
                }

                if (*(State->Pos + 1) == '\0') {
                    Result.Length += 1;
                    State->Pos += 1;
                } else {
                    Result.Length += 2;
                    State->Pos += 2;
                }

                Special = false;
            } break;

            default: {
                if (Result.Str == 0) {
                    Result.Str = State->Pos;
                }

                Result.Length += 1;
                State->Pos += 1;
                Special = false;
            } break;

            case '\0': {
                Special = true;
            } break;
        };
    };

    return Result;
}
