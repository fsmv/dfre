#include "../x86_opcode.h"
#include "../printers.cpp"

struct OpcodeWant {
    uint8_t Bytes[MAX_OPCODE_LEN];
    size_t Size;
};

void x86_opcode_RunTests() {
    // TODO: Jumps, need to be a special case since they use Instruction indexes
    /*
        // All Jumps
        // Declared with J (used to set the dest manually later) or JD
        // Uses instruction index for offsets.
        //  8bit jump opcodes
        JD(JMP, 0),
        JD(JNC, 0),
        JD(JE , 0),
        JD(JNE, 0),
        JD(JL , 0),
        JD(JG , 0),
        //  32 bit jump opcodes
        //  13 or more instructions distance is our heuristic for using 32 bit
        JD(JMP, 20),
        JD(JNC, 21),
        JD(JE , 22),
        JD(JNE, 23),
        JD(JL , 24),
        JD(JG , 25),
    */
    instruction Cases[] = {
        // All No argument ops
        // Encoded by OpNoarg(..)
        RET,

        // All single reg instructions
        // Declared with R8 and R32, encoded by OpReg(..)
        //  All ShortReg registers and addressing modes
        R32(INC, REG, EAX, 0),
        R32(INC, REG, EBX, 0),
        R32(INC, REG, ECX, 0),
        R32(INC, REG, EDX, 0),
        R32(INC, REG, ESP, 0),
        R32(INC, REG, EBP, 0),
        R32(INC, REG, ESI, 0),
        R32(INC, REG, EDI, 0),
        //  Remaining ShortReg opcodes (register doesn't matter)
        R32(PUSH, REG, ESP, 0),
        R32(POP , REG, EBP, 0),
        //  All 32bit non-ShortReg addressing modes and registers
        //   Register dereference modes, every register
        R32(POP, MEM, EAX, 0),
        R32(POP, MEM, EBX, 0),
        R32(POP, MEM, ECX, 0),
        R32(POP, MEM, EDX, 0),
        R32(POP, MEM, ESP, 0), // (MEM || MEM_DISP*) && ESP is special branch
        R32(POP, MEM, EBP, 0), // MEM && EBP is a special branch
        R32(POP, MEM, ESI, 0),
        R32(POP, MEM, EDI, 0),
        //   Cover each register class with displacement modes
        R32(PUSH, MEM_DISP8 , EAX, 0xAC),
        R32(PUSH, MEM_DISP8 , ESP, 0xAC),
        R32(PUSH, MEM_DISP8 , EBP, 0xAC),
        R32(PUSH, MEM_DISP8 , EDI, 0xAC),
        R32(PUSH, MEM_DISP32, ECX, 0xBADF00D),
        R32(PUSH, MEM_DISP32, ESP, 0xBADF00D),
        R32(PUSH, MEM_DISP32, EBP, 0xBADF00D),
        R32(PUSH, MEM_DISP32, ESI, 0xBADF00D),
        //  Remaining 32 bit non-ShortReg opcodes
        R32(INC, MEM, ESP, 0),
        R32(NOT, MEM, EDI, 0),
        //  8bit register direct non-ShortReg, all registers
        //  ESP, EBP, ESI, EDI are not encodable.
        R8(NOT, REG, EAX, 0),
        R8(NOT, REG, EBX, 0),
        R8(NOT, REG, ECX, 0),
        R8(NOT, REG, EDX, 0),
        //  Remaining 8bit register direct non-ShortReg opcodes
        //  PUSH and POP are not allowed (PUSH is possible but not implemented)
        R8(INC, REG, EAX, 0),

        // All two register instructions
        // Declared with RR8 and RR32, and encoded by OpRegReg(..)
        //  All addressing modes and registers
        //   8bit reg. ESP, EBP, ESI, EDI not encodable for either arg
        RR8(AND, REG, EAX, EDX, 0),
        RR8(AND, REG, EBX, ECX, 0),
        RR8(AND, REG, ECX, EBX, 0),
        RR8(AND, REG, EDX, EAX, 0),
        //   8bit mem. ESP, EBP, ESI, EDI not encodable for Src
        RR8(AND, MEM, EAX, EDX, 0),
        RR8(AND, MEM, EBX, ECX, 0),
        RR8(AND, MEM, ECX, EBX, 0),
        RR8(AND, MEM, EDX, EAX, 0),
        RR8(AND, MEM, ESP, EBX, 0), // Same special case as in single reg
        RR8(AND, MEM, EBP, EAX, 0), // Same sepcial case as in single reg
        RR8(AND, MEM, ESI, EDX, 0),
        RR8(AND, MEM, EDI, ECX, 0),
        //   8bit mem_disp. ESP, EBP, ESI, EDI not encodable for src
        //   Same register classes issue as in the single register tests
        RR8(AND, MEM_DISP8 , EAX, EAX, 0x42),
        RR8(AND, MEM_DISP8 , ESP, EBX, 0x42),
        RR8(AND, MEM_DISP8 , EBP, ECX, 0x42),
        RR8(AND, MEM_DISP8 , ESI, EDX, 0x42),
        RR8(AND, MEM_DISP32, ECX, EAX, 0x43424242),
        RR8(AND, MEM_DISP32, ESP, EBX, 0x43424242),
        RR8(AND, MEM_DISP32, EBP, ECX, 0x43424242),
        RR8(AND, MEM_DISP32, EDI, EDX, 0x43424242),
        //   Remaining 8bit opcodes
        RR8(OR  , REG, EAX, EAX, 0),
        RR8(XOR , REG, EAX, EAX, 0),
        RR8(CMP , REG, EAX, EAX, 0),
        RR8(MOV , REG, EAX, EAX, 0),
        RR8(MOVR, MEM, EAX, EAX, 0), // MEM To see if its actually reversed
        //   32bit reg
        RR32(XOR, REG, EAX, EDI, 0),
        RR32(XOR, REG, EBX, ESI, 0),
        RR32(XOR, REG, ECX, EBP, 0),
        RR32(XOR, REG, EDX, ESP, 0),
        RR32(XOR, REG, ESP, EDX, 0),
        RR32(XOR, REG, EBP, ECX, 0),
        RR32(XOR, REG, ESI, EBX, 0),
        RR32(XOR, REG, EDI, EAX, 0),
        //   32bit mem
        RR32(XOR, MEM, EAX, EDI, 0),
        RR32(XOR, MEM, EBX, ESI, 0),
        RR32(XOR, MEM, ECX, EBP, 0),
        RR32(XOR, MEM, EDX, ESP, 0),
        RR32(XOR, MEM, ESP, EDX, 0),
        RR32(XOR, MEM, EBP, ECX, 0),
        RR32(XOR, MEM, ESI, EBX, 0),
        RR32(XOR, MEM, EDI, EAX, 0),
        //   32bit mem_disp
        //   Same register classes issue as in the single register tests
        RR32(XOR, MEM_DISP8 , EAX, EAX, 0x42),
        RR32(XOR, MEM_DISP8 , ESP, EBX, 0x42),
        RR32(XOR, MEM_DISP8 , EBP, ESP, 0x42),
        RR32(XOR, MEM_DISP8 , ESI, EDX, 0x42),
        RR32(XOR, MEM_DISP32, ECX, EAX, 0x42424342),
        RR32(XOR, MEM_DISP32, ESP, EDI, 0x42424342),
        RR32(XOR, MEM_DISP32, EBP, ECX, 0x42424342),
        RR32(XOR, MEM_DISP32, EDI, EBP, 0x42424342),
        //   Remaining 32bit opcodes
        RR32(AND , REG, EAX, EAX, 0),
        RR32(OR  , REG, EAX, EAX, 0),
        RR32(CMP , REG, EAX, EAX, 0),
        RR32(MOV , REG, EAX, EAX, 0),
        RR32(MOVR, MEM, EAX, EAX, 0), // MEM To see if its actually reversed

        // TODO RI8 and RI32 cases
    };
    OpcodeWant Want[] = {
        // All No argument ops
        {{0xC3}, 1}, // RET

        // All single reg instructions
        //  All ShortReg registers and addressing modes
        {{0x40}, 1},
        {{0x43}, 1},
        {{0x41}, 1},
        {{0x42}, 1},
        {{0x44}, 1},
        {{0x45}, 1},
        {{0x46}, 1},
        {{0x47}, 1},
        //  Remaining ShortReg opcodes (register doesn't matter)
        {{0x54}, 1},
        {{0x5D}, 1},
        //  All 32bit non-ShortReg addressing modes and registers
        //   Register dereference modes, every register
        {{0x8F, 0x00}, 2},
        {{0x8F, 0x03}, 2},
        {{0x8F, 0x01}, 2},
        {{0x8F, 0x02}, 2},
        {{0x8F, 0x04, 0x24}, 3},
        {{0x8F, 0x45, 0x00}, 3},
        {{0x8F, 0x06}, 2},
        {{0x8F, 0x07}, 2},
        //   Cover each register class with displacement modes
        {{0xFF, 0x70, 0xAC}, 3},
        {{0xFF, 0x74, 0x24, 0xAC}, 4},
        {{0xFF, 0x75, 0xAC}, 3},
        {{0xFF, 0x77, 0xAC}, 3},
        {{0xFF, 0xB1, 0x0D, 0xF0, 0xAD, 0x0B}, 6},
        {{0xFF, 0xB4, 0x24, 0x0D, 0xF0, 0xAD, 0x0B}, 7},
        {{0xFF, 0xB5, 0x0D, 0xF0, 0xAD, 0x0B}, 6},
        {{0xFF, 0xB6, 0x0D, 0xF0, 0xAD, 0x0B}, 6},
        //  Remaining 32 bit non-ShortReg opcodes
        {{0xFF, 0x04, 0x24}, 3},
        {{0xF7, 0x17}, 2},
        //  8bit register direct non-ShortReg, all registers
        {{0xF6, 0xD0}, 2},
        {{0xF6, 0xD3}, 2},
        {{0xF6, 0xD1}, 2},
        {{0xF6, 0xD2}, 2},
        //  Remaining 8bit register direct non-ShortReg opcodes
        {{0xFE, 0xC0}, 2},

        // All two register instructions
        //  All addressing modes and registers
        //   8bit reg.
        {{0x20, 0xD0}, 2},
        {{0x20, 0xCB}, 2},
        {{0x20, 0xD9}, 2},
        {{0x20, 0xC2}, 2},
        //   8bit mem. ESP, EBP, ESI, EDI not encodable for Src
        {{0x20, 0x10}, 2},
        {{0x20, 0x0B}, 2},
        {{0x20, 0x19}, 2},
        {{0x20, 0x02}, 2},
        {{0x20, 0x1C, 0x24}, 3},
        {{0x20, 0x45, 0x00}, 3},
        {{0x20, 0x16}, 2},
        {{0x20, 0x0F}, 2},
        //   8bit mem_disp. ESP, EBP, ESI, EDI not encodable for src
        {{0x20, 0x40, 0x42}, 3},
        {{0x20, 0x5C, 0x24, 0x42}, 4},
        {{0x20, 0x4D, 0x42}, 3},
        {{0x20, 0x56, 0x42}, 3},
        {{0x20, 0x81, 0x42, 0x42, 0x42, 0x43}, 6},
        {{0x20, 0x9C, 0x24, 0x42, 0x42, 0x42, 0x43}, 7},
        {{0x20, 0x8D, 0x42, 0x42, 0x42, 0x43}, 6},
        {{0x20, 0x97, 0x42, 0x42, 0x42, 0x43}, 6},
        //   Remaining 8bit opcodes
        {{0x08, 0xC0}, 2},
        {{0x30, 0xC0}, 2},
        {{0x38, 0xC0}, 2},
        {{0x88, 0xC0}, 2},
        {{0x8A, 0x00}, 2},
        //   32bit reg
        {{0x31, 0xF8}, 2},
        {{0x31, 0xF3}, 2},
        {{0x31, 0xE9}, 2},
        {{0x31, 0xE2}, 2},
        {{0x31, 0xD4}, 2},
        {{0x31, 0xCD}, 2},
        {{0x31, 0xDE}, 2},
        {{0x31, 0xC7}, 2},
        //   32bit mem
        {{0x31, 0x38}, 2},
        {{0x31, 0x33}, 2},
        {{0x31, 0x29}, 2},
        {{0x31, 0x22}, 2},
        {{0x31, 0x14, 0x24}, 3},
        {{0x31, 0x4D, 0x00}, 3},
        {{0x31, 0x1E}, 2},
        {{0x31, 0x07}, 2},
        //   32bit mem_disp
        {{0x31, 0x40, 0x42}, 3},
        {{0x31, 0x5C, 0x24, 0x42}, 4},
        {{0x31, 0x65, 0x42}, 3},
        {{0x31, 0x56, 0x42}, 3},
        {{0x31, 0x81, 0x42, 0x43, 0x42, 0x42}, 6},
        {{0x31, 0xBC, 0x24, 0x42, 0x43, 0x42, 0x42}, 7},
        {{0x31, 0x8D, 0x42, 0x43, 0x42, 0x42}, 6},
        {{0x31, 0xAF, 0x42, 0x43, 0x42, 0x42}, 6},
        //   Remaining 32bit opcodes
        {{0x21, 0xC0}, 2},
        {{0x09, 0xC0}, 2},
        {{0x39, 0xC0}, 2},
        {{0x89, 0xC0}, 2},
        {{0x8B, 0x00}, 2},
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
