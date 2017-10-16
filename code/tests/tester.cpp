#if defined(DFRE_WIN32)
    #include "win32_platform.cpp"
#elif defined(DFRE_NIX32)
    #include "nix32_platform.cpp"
#else
    #error "DFRE_WIN32 or DFRE_NIX32 must be defined to set the platform"
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
        return 1;
    }
    Print("All tests passed!\n");
    return 0;
}
