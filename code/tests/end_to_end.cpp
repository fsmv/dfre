#include "parser.cpp"
#include "x86_codegen.cpp"

#include "utils.h"
#include "print.h"
#include "mem_arena.h"

//TODO: put this stuff in a standalone file
extern "C" typedef uint32_t (*dfreMatch)(const char *Str); // Function pointer type for calling the compiled regex code

dfreMatch CompileRegex(const char *Regex, size_t *CodeSize) {
    static mem_arena ArenaA = ArenaInit();
    static mem_arena ArenaB = ArenaInit();

    nfa *NFA = RegexToNFA(Regex, &ArenaA);
    // Convert the NFA into an intermediate code representation
    size_t InstructionsGenerated = GenerateInstructions(NFA, &ArenaB);
    instruction *Instructions = (instruction *)ArenaB.Base;
    // Allocate storage for the unpacked x86 opcodes
    NFA = (nfa*)0;
    ArenaA.Used = 0;
    opcode_unpacked *UnpackedOpcodes = (opcode_unpacked*)Alloc(&ArenaA,
            sizeof(opcode_unpacked) * InstructionsGenerated);
    // Assemble the x86
    AssembleInstructions(Instructions, InstructionsGenerated, UnpackedOpcodes);
    // Note: no more return count here, this keeps the same number of instructions
    // Allocate storage for the actual byte code
    Instructions = (instruction*)0;
    ArenaB.Used = 0;
    size_t UpperBoundCodeSize = MAX_OPCODE_LEN * InstructionsGenerated;
    uint8_t *Code = (uint8_t*)Alloc(&ArenaB, UpperBoundCodeSize);
    // Write the code to the arena
    size_t CodeWritten = PackCode(UnpackedOpcodes, InstructionsGenerated, Code);
    *CodeSize = CodeWritten;
    // Load the code into an executable page
    void *CodeLoc = LoadCode(Code, CodeWritten);
    dfreMatch Match = (dfreMatch)CodeLoc;
    // Clear the arenas
    for (size_t i = 0; i < ArenaA.Committed; ++i) ArenaA.Base[i] = 0;
    ArenaA.Used = 0;
    for (size_t i = 0; i < ArenaB.Committed; ++i) ArenaB.Base[i] = 0;
    ArenaB.Used = 0;
    return Match;
}

// TODO: print case pass counts
// TODO: make %#s print \n instead of a real newline and print PASS strings
#define EXPECT_MATCH(str) do { \
   if (!Match(str)) { \
       T->Failed = true; \
       Print("FAIL \"%s\" did not match regex %s. %s:%u\n", (str), Regex, __FILE__, __LINE__); \
   } \
} while(false)

#define EXPECT_NO_MATCH(str) do { \
   if (Match(str)) { \
       T->Failed = true; \
       Print("FAIL \"%s\" matched regex %s. %s:%u\n", (str), Regex, __FILE__, __LINE__); \
   } \
} while(false)

// TODO: Maybe it could work to have a flat list of cases and deduplicate the
// compiled regex by using the same pointer for the regex field and checking for
// it changing. Is that better though?

void end_to_end_RunTests(tester_state *T) {
    size_t CodeSize;

    // TODO: split into categories and make pretty printing like the x86 tests
    // TODO: complete coverage

    {
        const char *Regex = "test";
        auto Match = CompileRegex(Regex, &CodeSize);

        EXPECT_MATCH("test");

        EXPECT_NO_MATCH("");
        EXPECT_NO_MATCH("ab");
        EXPECT_NO_MATCH("tttt");
        EXPECT_NO_MATCH("testa");
        EXPECT_NO_MATCH("atest");
        EXPECT_NO_MATCH("aa");
        EXPECT_NO_MATCH("aba");
        EXPECT_NO_MATCH("abbbba");
        EXPECT_NO_MATCH("42");
        EXPECT_NO_MATCH("bbbbb");
        EXPECT_NO_MATCH("b");

        Free((void*)Match, CodeSize);
    }
    {
        const char *Regex = "ab*";
        auto Match = CompileRegex(Regex, &CodeSize);

        EXPECT_MATCH("a");
        EXPECT_MATCH("ab");
        EXPECT_MATCH("abb");
        EXPECT_MATCH("abbbbbbbbbb");
        EXPECT_MATCH("abbbbb");

        EXPECT_NO_MATCH("");
        EXPECT_NO_MATCH("aa");
        EXPECT_NO_MATCH("aba");
        EXPECT_NO_MATCH("abbbba");
        EXPECT_NO_MATCH("42");
        EXPECT_NO_MATCH("bbbbb");
        EXPECT_NO_MATCH("b");

        Free((void*)Match, CodeSize);
    }
    {
        const char *Regex = "ab+";
        auto Match = CompileRegex(Regex, &CodeSize);

        EXPECT_MATCH("ab");
        EXPECT_MATCH("abb");
        EXPECT_MATCH("abbbbbbbbbb");
        EXPECT_MATCH("abbbbb");

        EXPECT_NO_MATCH("");
        EXPECT_NO_MATCH("a");
        EXPECT_NO_MATCH("aa");
        EXPECT_NO_MATCH("aba");
        EXPECT_NO_MATCH("abbbba");
        EXPECT_NO_MATCH("42");
        EXPECT_NO_MATCH("bbbbb");
        EXPECT_NO_MATCH("b");

        Free((void*)Match, CodeSize);
    }
    {
        const char *Regex = "ab?";
        auto Match = CompileRegex(Regex, &CodeSize);

        EXPECT_MATCH("a");
        EXPECT_MATCH("ab");

        EXPECT_NO_MATCH("");
        EXPECT_NO_MATCH("aa");
        EXPECT_NO_MATCH("aba");
        EXPECT_NO_MATCH("abbbba");
        EXPECT_NO_MATCH("42");
        EXPECT_NO_MATCH("bbbbb");
        EXPECT_NO_MATCH("b");
        EXPECT_NO_MATCH("abb");
        EXPECT_NO_MATCH("abbbbbbbbbb");
        EXPECT_NO_MATCH("abbbbb");

        Free((void*)Match, CodeSize);
    }
    {
        const char *Regex = "a|b";
        auto Match = CompileRegex(Regex, &CodeSize);

        EXPECT_MATCH("a");
        EXPECT_MATCH("b");

        EXPECT_NO_MATCH("");
        EXPECT_NO_MATCH("ab");
        EXPECT_NO_MATCH("aa");
        EXPECT_NO_MATCH("aba");
        EXPECT_NO_MATCH("abbbba");
        EXPECT_NO_MATCH("42");
        EXPECT_NO_MATCH("bbbbb");
        EXPECT_NO_MATCH("abb");
        EXPECT_NO_MATCH("abbbbbbbbbb");
        EXPECT_NO_MATCH("abbbbb");

        Free((void*)Match, CodeSize);
    }
    {
        const char *Regex = "..";
        auto Match = CompileRegex(Regex, &CodeSize);

        EXPECT_MATCH("ab");
        EXPECT_MATCH("ba");
        EXPECT_MATCH("zz");
        EXPECT_MATCH("+.");
        EXPECT_MATCH("42");
        EXPECT_MATCH(",r");
        EXPECT_MATCH("\n\a");

        EXPECT_NO_MATCH("");
        EXPECT_NO_MATCH("a");
        EXPECT_NO_MATCH("tttt");
        EXPECT_NO_MATCH("testa");
        EXPECT_NO_MATCH("atest");
        EXPECT_NO_MATCH("b");
        EXPECT_NO_MATCH("aba");
        EXPECT_NO_MATCH("abbbba");
        EXPECT_NO_MATCH("bbbbb");

        Free((void*)Match, CodeSize);
    }
    {
        const char *Regex = "[abc]";
        auto Match = CompileRegex(Regex, &CodeSize);

        EXPECT_MATCH("a");
        EXPECT_MATCH("b");
        EXPECT_MATCH("c");

        EXPECT_NO_MATCH("");
        EXPECT_NO_MATCH("tttt");
        EXPECT_NO_MATCH("\n");
        EXPECT_NO_MATCH("testa");
        EXPECT_NO_MATCH("atest");
        EXPECT_NO_MATCH("aba");
        EXPECT_NO_MATCH("abbbba");
        EXPECT_NO_MATCH("bbbbb");

        Free((void*)Match, CodeSize);
    }
    {
        const char *Regex = "[a-z]";
        auto Match = CompileRegex(Regex, &CodeSize);

        EXPECT_MATCH("a");
        EXPECT_MATCH("b");
        EXPECT_MATCH("c");
        EXPECT_MATCH("l");
        EXPECT_MATCH("o");
        EXPECT_MATCH("z");
        EXPECT_MATCH("q");
        EXPECT_MATCH("g");

        EXPECT_NO_MATCH("");
        EXPECT_NO_MATCH("0");
        EXPECT_NO_MATCH("A");
        EXPECT_NO_MATCH("Z");
        EXPECT_NO_MATCH("9");
        EXPECT_NO_MATCH("tttt");
        EXPECT_NO_MATCH("\n");
        EXPECT_NO_MATCH("testa");
        EXPECT_NO_MATCH("atest");
        EXPECT_NO_MATCH("aba");
        EXPECT_NO_MATCH("abbbba");
        EXPECT_NO_MATCH("bbbbb");

        Free((void*)Match, CodeSize);
    }
    {
        const char *Regex = "[a-g2-6]";
        auto Match = CompileRegex(Regex, &CodeSize);

        EXPECT_MATCH("a");
        EXPECT_MATCH("b");
        EXPECT_MATCH("c");
        EXPECT_MATCH("g");
        EXPECT_MATCH("5");
        EXPECT_MATCH("2");
        EXPECT_MATCH("6");

        EXPECT_NO_MATCH("");
        EXPECT_NO_MATCH("0");
        EXPECT_NO_MATCH("l");
        EXPECT_NO_MATCH("h");
        EXPECT_NO_MATCH("A");
        EXPECT_NO_MATCH("Z");
        EXPECT_NO_MATCH("9");
        EXPECT_NO_MATCH("tttt");
        EXPECT_NO_MATCH("\n");
        EXPECT_NO_MATCH("testa");
        EXPECT_NO_MATCH("atest");
        EXPECT_NO_MATCH("aba");

        Free((void*)Match, CodeSize);
    }
    {
        const char *Regex = "(ab)*";
        auto Match = CompileRegex(Regex, &CodeSize);

        EXPECT_MATCH("");
        EXPECT_MATCH("ab");
        EXPECT_MATCH("abab");
        EXPECT_MATCH("ababab");
        EXPECT_MATCH("ababababababab");

        EXPECT_NO_MATCH("aba");
        EXPECT_NO_MATCH("abb");
        EXPECT_NO_MATCH("abaa");
        EXPECT_NO_MATCH("ababbabab");
        EXPECT_NO_MATCH("aaaababab");
        EXPECT_NO_MATCH("0");
        EXPECT_NO_MATCH("l");
        EXPECT_NO_MATCH("9");
        EXPECT_NO_MATCH("tttt");
        EXPECT_NO_MATCH("\n");
        EXPECT_NO_MATCH("testa");
        EXPECT_NO_MATCH("atest");

        Free((void*)Match, CodeSize);
    }
}
