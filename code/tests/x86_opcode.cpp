#include "../x86_opcode.h"
#include "../printers.cpp"

struct OpcodeWant {
    uint8_t Bytes[MAX_OPCODE_LEN];
    size_t Size;
};

void x86_opcode_RunTests() {
    instruction Cases[] = {
        RR8 (AND, MEM,        EAX, ECX, 0),
        RR32(AND, REG,        EDI, ESI, 0),
        RR8 (AND, MEM_DISP8,  EAX, ECX, 0x22),
        RR32(AND, MEM_DISP8,  ECX, EDX, 0x33),
        RR8 (AND, MEM_DISP32, EBX, EAX, 0xFFFFFFFF),
        RR32(AND, MEM_DISP32, EDX, EBX, 0xC0DED00D),
    };
    OpcodeWant Want[] = {
        {{0x20, 0x08}, 2},
        {{0x21, 0xF7}, 2},
        {{0x20, 0x48, 0x22}, 3},
        {{0x21, 0x51, 0x33}, 3},
        {{0x20, 0x83, 0xFF, 0xFF, 0xFF, 0xFF}, 6},
        {{0x21, 0x9A, 0x0D, 0xD0, 0xDE, 0xC0}, 6},
    };

    opcode_unpacked Opcodes[ArrayLength(Cases)];
    AssembleInstructions(Cases, ArrayLength(Cases), Opcodes);

    for (size_t i = 0; i < ArrayLength(Cases); ++i) {
        uint8_t Got[MAX_OPCODE_LEN];
        size_t GotLen = PackCode(&Opcodes[i], 1, Got);

        // TODO: Put in want data and remove me
        // Easy way to get opcodes separated that I can check with a disassembler
        if (i >= ArrayLength(Want)) {
            PrintByteCode(Got, GotLen, false);
            Print("\n");
            continue;
        }

        bool Passed = true;
        if (GotLen != Want[i].Size) {
            Passed = false;
        }
        for (size_t j = 0; Passed && j < GotLen; ++j) {
            if (Got[j] != Want[i].Bytes[j]) {
                Passed = false;
            }
        }
        if (Passed) {
            Print("PASS ");
            PrintInstruction(&Cases[i]);
            Print(" => ");
            PrintByteCode(Got, GotLen, false);
            Print("\n");
        } else {
            TestFailed();
            Print("FAIL ");
            PrintInstruction(&Cases[i]);
            Print(" => ");
            PrintByteCode(Got, GotLen, false);
            Print("; Wanted ");
            PrintByteCode(Want[i].Bytes, Want[i].Size, false);
            Print("\n");
        }
    }
}
