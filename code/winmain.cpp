// Copyright (c) 2016 Andrew Kallmeyer <fsmv@sapium.net>
// Provided under the MIT License: https://mit-license.org

#include <stdint.h>
#define Assert(cond) if (!(cond)) { *((int*)0) = 0; }
#define ArrayLength(arr) (sizeof(arr) / sizeof((arr)[0]))

#include "lexer.cpp"

#include <windows.h>

int main(int argc, char *argv[]) {
    HANDLE Out = GetStdHandle(STD_OUTPUT_HANDLE);
    LPDWORD CharsWritten;
    if (!Out || Out == INVALID_HANDLE_VALUE) {
        MessageBox(0, "Could not get std Out", 0, MB_OK | MB_ICONERROR);
        return 1;
    }

    if (argc != 2) {
        char Usage1[] = "Usage: ";
        char Usage2[] = " [regex]\n";

        uint32_t Argv0Length = 0;
        for (char *Idx = argv[0]; *Idx != '\0'; ++Idx, ++Argv0Length) { }

        WriteConsoleA(Out, Usage1, ArrayLength(Usage1), CharsWritten, 0);
        WriteConsoleA(Out, argv[0], Argv0Length, CharsWritten, 0);
        WriteConsoleA(Out, Usage2, ArrayLength(Usage2), CharsWritten, 0);
        return 1;
    }

    lexer_state Lexer = ReLexer(argv[1]);
    while(ReLexerHasNext(&Lexer)) {
        token Token = ReLexerNext(&Lexer);

        WriteConsoleA(Out, Token.Str, Token.Length, CharsWritten, 0);
        WriteConsoleA(Out, "\n", 1, CharsWritten, 0);
    }

    return 0;
}
