#include "../x86_opcode.h"
#include "../printers.cpp"

struct OpcodeWant {
    uint8_t Bytes[MAX_OPCODE_LEN];
    size_t Size;
};

void x86_opcode_RunTests() {
    instruction Cases[] = {
        RR8 (AND, REG,        EAX, ECX, 0),
        RR32(AND, REG,        EDX, EBX, 0),
        // TODO: Write more test data
        /*
        RR8 (AND, MEM,        ESP, EBP, 0),
        RR32(AND, MEM,        ESI, EDI, 0),
        RR8 (AND, MEM_DISP8,  ESP, EBP, 123),
        RR32(AND, MEM_DISP8,  ESI, EDI, 123),
        RR8 (AND, MEM_DISP32, ESP, EBP, 99999),
        RR32(AND, MEM_DISP32, ESI, EDI, 99999),*/
    };
    OpcodeWant Want[] = {
        {{0x20, 0xC8}, 2},
        {{0x21, 0xDA}, 2},
    };

    opcode_unpacked Opcodes[ArrayLength(Cases)];
    AssembleInstructions(Cases, ArrayLength(Cases), Opcodes);

    for (size_t i = 0; i < ArrayLength(Cases); ++i) {
        uint8_t Got[MAX_OPCODE_LEN];
        size_t GotLen = PackCode(&Opcodes[i], 1, Got);

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
