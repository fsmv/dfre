// Copyright (c) 2016-2019 Andrew Kallmeyer <fsmv@sapium.net>
// Provided under the MIT License: https://mit-license.org

#if defined(DFRE_WIN32)
    #include "win32_platform.cpp"
#elif defined(DFRE_NIX32)
    #include "linux32_syscalls.cpp"
    #include "posix_platform.cpp"
#elif defined(DFRE_OSX32)
    #include "osx32_syscalls.cpp"
    #include "posix_platform.cpp"
#else
    #error "DFRE_WIN32, DFRE_NIX32, or DFRE_OSX32 must be defined to set the platform"
#endif

#include "print.h"
#include "utils.h"

static bool failed = false;

inline void TestFailed() {
    failed = true;
}

#include "tests/x86_opcode.cpp"

int main(int argc, char *argv[]) {
    x86_opcode_RunTests();

    if (failed) {
        Print("\nAt least one test failed.\n");
        return 1;
    }
    Print("\nAll tests passed!\n");
    return 0;
}
