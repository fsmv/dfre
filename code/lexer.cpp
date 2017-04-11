// Copyright (c) 2016-2017 Andrew Kallmeyer <fsmv@sapium.net>
// Provided under the MIT License: https://mit-license.org

struct lexer_state {
    char *Pos;
};

struct token {
    char *Str;
    int32_t Length;
};

lexer_state ReLexer(char *Regex) {
    lexer_state Result;
    Result.Pos = Regex;

    return Result;
}

bool ReLexerHasNext(lexer_state *State) {
    bool Result = (*State->Pos != '\0');
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
