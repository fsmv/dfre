#include "../x86_opcode.h"
#include "../printers.cpp"

struct OpcodeWant {
    uint8_t Bytes[MAX_OPCODE_LEN];
    size_t Size;
};

void x86_opcode_RunTests() {
    instruction Cases[] = {
        // Test all different registers and addressing options for RR8 and RR32
        // The bytes of the RR* instruction arguments independent of the opcode
        RR8 (AND, MEM,        EAX, ECX, 0),
        RR32(AND, REG,        EDI, ESI, 0),
        RR8 (AND, MEM_DISP8,  EAX, ECX, 0x22),
        RR32(AND, MEM_DISP8,  ECX, EDX, 0x33),
        RR8 (AND, MEM_DISP32, EBX, EAX, 0xFFFFFFFF),
        RR32(AND, MEM_DISP32, EDX, EBX, 0xC0DED00D),
        // Test the opcode table (except AND which was tested above)
        // Opcode bytes are independent from the args
        RR8 (OR,   REG, EAX, EAX, 0),
        RR32(OR,   REG, EAX, EAX, 0),
        RR8 (XOR,  REG, EAX, EAX, 0),
        RR32(XOR,  REG, EAX, EAX, 0),
        RR8 (CMP,  REG, EAX, EAX, 0),
        RR32(CMP,  REG, EAX, EAX, 0),
        RR8 (MOV,  REG, EAX, EAX, 0),
        RR32(MOV,  REG, EAX, EAX, 0),
        RR8 (MOVR, REG, EAX, EAX, 0),
        RR32(MOVR, REG, EAX, EAX, 0),

        // TODO fit in with the other test cases with revised categories
        RI8 (OR,  REG, EAX, EAX, 0xDC),
        RI32(OR,  REG, EAX, EAX, 0x01020304),
        RI8 (XOR, REG, EAX, EAX, 0xDC),
        RI32(XOR, REG, EAX, EAX, 0x01020304),
        RI8 (CMP, REG, EAX, EAX, 0xDC),
        RI32(CMP, REG, EAX, EAX, 0x01020304),

        RI8 (BT,  REG, EAX, EAX, 0x04),
    };
    OpcodeWant Want[] = {
        // RR8 and RR32 Args cases
        {{0x20, 0x08}, 2},
        {{0x21, 0xF7}, 2},
        {{0x20, 0x48, 0x22}, 3},
        {{0x21, 0x51, 0x33}, 3},
        {{0x20, 0x83, 0xFF, 0xFF, 0xFF, 0xFF}, 6},
        {{0x21, 0x9A, 0x0D, 0xD0, 0xDE, 0xC0}, 6},
        // RR8 and RR32 opcode cases
        {{0x08, 0xC0}, 2},
        {{0x09, 0xC0}, 2},
        {{0x30, 0xC0}, 2},
        {{0x31, 0xC0}, 2},
        {{0x38, 0xC0}, 2},
        {{0x39, 0xC0}, 2},
        {{0x88, 0xC0}, 2},
        {{0x89, 0xC0}, 2},
        {{0x8A, 0xC0}, 2},
        {{0x8B, 0xC0}, 2},
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
