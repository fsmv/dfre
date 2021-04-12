// Copyright (c) 2016-2019 Andrew Kallmeyer <fsmv@sapium.net>
// Provided under the MIT License: https://mit-license.org

#if defined(DFRE_WIN32)
    #include "win32_platform.cpp"
#elif defined(DFRE_NIX32)
    #include "posix_platform.cpp"
#elif defined(DFRE_OSX32)
    #include "posix_platform.cpp"
#else
    #error "DFRE_WIN32, DFRE_NIX32, or DFRE_OSX32 must be defined to set the platform"
#endif

#include "printers.cpp"

#include "print.h"
#include "utils.h"
#include "mem_arena.h"

struct tester_state {
    bool Failed;
    mem_arena Arena;
};

#include "tests/x86_opcode.cpp"
#include "tests/end_to_end.cpp"

int main(int argc, char *argv[]) {
    tester_state T = {};
    T.Arena = ArenaInit();

    x86_opcode_RunTests(&T);
    end_to_end_RunTests(&T);

    if (T.Failed) {
        Print("At least one test failed.\n");
        return 1;
    }
    Print("All tests passed!\n");
    return 0;
}
