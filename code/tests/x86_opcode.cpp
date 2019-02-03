#include "../x86_opcode.h"
#include "../printers.cpp"

struct opcode_want {
    uint8_t Bytes[MAX_OPCODE_LEN];
    size_t Size;
};

#define WantOp(...) opcode_want{{__VA_ARGS__}, sizeof((uint8_t[]){__VA_ARGS__})}

struct opcode_case {
    instruction Input;
    opcode_want Want;
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
    opcode_case Cases[] = {
        // All No argument ops
        // Encoded by OpNoarg(..)
        {RET, WantOp(0xC3)},

        // All single reg instructions
        // Declared with R8 and R32, encoded by OpReg(..)
        //  All ShortReg registers and addressing modes
        {R32(INC, REG, EAX, 0), WantOp(0x40)},
        {R32(INC, REG, EBX, 0), WantOp(0x43)},
        {R32(INC, REG, ECX, 0), WantOp(0x41)},
        {R32(INC, REG, EDX, 0), WantOp(0x42)},
        {R32(INC, REG, ESP, 0), WantOp(0x44)},
        {R32(INC, REG, EBP, 0), WantOp(0x45)},
        {R32(INC, REG, ESI, 0), WantOp(0x46)},
        {R32(INC, REG, EDI, 0), WantOp(0x47)},
        //  Remaining ShortReg opcodes (register doesn't matter)
        {R32(PUSH, REG, ESP, 0), WantOp(0x54)},
        {R32(POP , REG, EBP, 0), WantOp(0x5D)},
        //  All 32bit non-ShortReg addressing modes and registers
        //   Register dereference modes, every register
        {R32(POP, MEM, EAX, 0), WantOp(0x8F, 0x00)},
        {R32(POP, MEM, EBX, 0), WantOp(0x8F, 0x03)},
        {R32(POP, MEM, ECX, 0), WantOp(0x8F, 0x01)},
        {R32(POP, MEM, EDX, 0), WantOp(0x8F, 0x02)},
        {R32(POP, MEM, ESP, 0), WantOp(0x8F, 0x04, 0x24)}, // (MEM || MEM_DISP*) && ESP is special branch
        {R32(POP, MEM, EBP, 0), WantOp(0x8F, 0x45, 0x00)}, // MEM && EBP is a special branch
        {R32(POP, MEM, ESI, 0), WantOp(0x8F, 0x06)},
        {R32(POP, MEM, EDI, 0), WantOp(0x8F, 0x07)},
        //   Cover each register class with displacement modes
        {R32(PUSH, MEM_DISP8 , EAX, 0xAC), WantOp(0xFF, 0x70, 0xAC)},
        {R32(PUSH, MEM_DISP8 , ESP, 0xAC), WantOp(0xFF, 0x74, 0x24, 0xAC)},
        {R32(PUSH, MEM_DISP8 , EBP, 0xAC), WantOp(0xFF, 0x75, 0xAC)},
        {R32(PUSH, MEM_DISP8 , EDI, 0xAC), WantOp(0xFF, 0x77, 0xAC)},
        {R32(PUSH, MEM_DISP32, ECX, 0xBADF00D), WantOp(0xFF, 0xB1, 0x0D, 0xF0, 0xAD, 0x0B)},
        {R32(PUSH, MEM_DISP32, ESP, 0xBADF00D), WantOp(0xFF, 0xB4, 0x24, 0x0D, 0xF0, 0xAD, 0x0B)},
        {R32(PUSH, MEM_DISP32, EBP, 0xBADF00D), WantOp(0xFF, 0xB5, 0x0D, 0xF0, 0xAD, 0x0B)},
        {R32(PUSH, MEM_DISP32, ESI, 0xBADF00D), WantOp(0xFF, 0xB6, 0x0D, 0xF0, 0xAD, 0x0B)},
        //  Remaining 32 bit non-ShortReg opcodes
        {R32(INC, MEM, ESP, 0), WantOp(0xFF, 0x04, 0x24)},
        {R32(NOT, MEM, EDI, 0), WantOp(0xF7, 0x17)},
        //  8bit register direct non-ShortReg, all registers
        //  ESP, EBP, ESI, EDI are not encodable.
        {R8(NOT, REG, EAX, 0), WantOp(0xF6, 0xD0)},
        {R8(NOT, REG, EBX, 0), WantOp(0xF6, 0xD3)},
        {R8(NOT, REG, ECX, 0), WantOp(0xF6, 0xD1)},
        {R8(NOT, REG, EDX, 0), WantOp(0xF6, 0xD2)},
        //  Remaining 8bit register direct non-ShortReg opcodes
        //  PUSH and POP are not allowed (PUSH is possible but not implemented)
        {R8(INC, REG, EAX, 0), WantOp(0xFE, 0xC0)},

        // All two register instructions
        // Declared with RR8 and RR32, and encoded by OpRegReg(..)
        //  All addressing modes and registers
        //   8bit reg. ESP, EBP, ESI, EDI not encodable for either arg
        {RR8(AND, REG, EAX, EDX, 0), WantOp(0x20, 0xD0)},
        {RR8(AND, REG, EBX, ECX, 0), WantOp(0x20, 0xCB)},
        {RR8(AND, REG, ECX, EBX, 0), WantOp(0x20, 0xD9)},
        {RR8(AND, REG, EDX, EAX, 0), WantOp(0x20, 0xC2)},
        //   8bit mem. ESP, EBP, ESI, EDI not encodable for Src
        {RR8(AND, MEM, EAX, EDX, 0), WantOp(0x20, 0x10)},
        {RR8(AND, MEM, EBX, ECX, 0), WantOp(0x20, 0x0B)},
        {RR8(AND, MEM, ECX, EBX, 0), WantOp(0x20, 0x19)},
        {RR8(AND, MEM, EDX, EAX, 0), WantOp(0x20, 0x02)},
        {RR8(AND, MEM, ESP, EBX, 0), WantOp(0x20, 0x1C, 0x24)}, // Same special case as in single reg
        {RR8(AND, MEM, EBP, EAX, 0), WantOp(0x20, 0x45, 0x00)}, // Same sepcial case as in single reg
        {RR8(AND, MEM, ESI, EDX, 0), WantOp(0x20, 0x16)},
        {RR8(AND, MEM, EDI, ECX, 0), WantOp(0x20, 0x0F)},
        //   8bit mem_disp. ESP, EBP, ESI, EDI not encodable for src
        //   Same register classes issue as in the single register tests
        {RR8(AND, MEM_DISP8 , EAX, EAX, 0x42), WantOp(0x20, 0x40, 0x42)},
        {RR8(AND, MEM_DISP8 , ESP, EBX, 0x42), WantOp(0x20, 0x5C, 0x24, 0x42)},
        {RR8(AND, MEM_DISP8 , EBP, ECX, 0x42), WantOp(0x20, 0x4D, 0x42)},
        {RR8(AND, MEM_DISP8 , ESI, EDX, 0x42), WantOp(0x20, 0x56, 0x42)},
        {RR8(AND, MEM_DISP32, ECX, EAX, 0x43424242), WantOp(0x20, 0x81, 0x42, 0x42, 0x42, 0x43)},
        {RR8(AND, MEM_DISP32, ESP, EBX, 0x43424242), WantOp(0x20, 0x9C, 0x24, 0x42, 0x42, 0x42, 0x43)},
        {RR8(AND, MEM_DISP32, EBP, ECX, 0x43424242), WantOp(0x20, 0x8D, 0x42, 0x42, 0x42, 0x43)},
        {RR8(AND, MEM_DISP32, EDI, EDX, 0x43424242), WantOp(0x20, 0x97, 0x42, 0x42, 0x42, 0x43)},
        //   Remaining 8bit opcodes
        {RR8(OR  , REG, EAX, EAX, 0), WantOp(0x08, 0xC0)},
        {RR8(XOR , REG, EAX, EAX, 0), WantOp(0x30, 0xC0)},
        {RR8(CMP , REG, EAX, EAX, 0), WantOp(0x38, 0xC0)},
        {RR8(MOV , REG, EAX, EAX, 0), WantOp(0x88, 0xC0)},
        {RR8(MOVR, MEM, EAX, EAX, 0), WantOp(0x8A, 0x00)}, // MEM To see if its actually reversed
        //   32bit reg
        {RR32(XOR, REG, EAX, EDI, 0), WantOp(0x31, 0xF8)},
        {RR32(XOR, REG, EBX, ESI, 0), WantOp(0x31, 0xF3)},
        {RR32(XOR, REG, ECX, EBP, 0), WantOp(0x31, 0xE9)},
        {RR32(XOR, REG, EDX, ESP, 0), WantOp(0x31, 0xE2)},
        {RR32(XOR, REG, ESP, EDX, 0), WantOp(0x31, 0xD4)},
        {RR32(XOR, REG, EBP, ECX, 0), WantOp(0x31, 0xCD)},
        {RR32(XOR, REG, ESI, EBX, 0), WantOp(0x31, 0xDE)},
        {RR32(XOR, REG, EDI, EAX, 0), WantOp(0x31, 0xC7)},
        //   32bit mem
        {RR32(XOR, MEM, EAX, EDI, 0), WantOp(0x31, 0x38)},
        {RR32(XOR, MEM, EBX, ESI, 0), WantOp(0x31, 0x33)},
        {RR32(XOR, MEM, ECX, EBP, 0), WantOp(0x31, 0x29)},
        {RR32(XOR, MEM, EDX, ESP, 0), WantOp(0x31, 0x22)},
        {RR32(XOR, MEM, ESP, EDX, 0), WantOp(0x31, 0x14, 0x24)},
        {RR32(XOR, MEM, EBP, ECX, 0), WantOp(0x31, 0x4D, 0x00)},
        {RR32(XOR, MEM, ESI, EBX, 0), WantOp(0x31, 0x1E)},
        {RR32(XOR, MEM, EDI, EAX, 0), WantOp(0x31, 0x07)},
        //   32bit mem_disp
        //   Same register classes issue as in the single register tests
        {RR32(XOR, MEM_DISP8 , EAX, EAX, 0x42), WantOp(0x31, 0x40, 0x42)},
        {RR32(XOR, MEM_DISP8 , ESP, EBX, 0x42), WantOp(0x31, 0x5C, 0x24, 0x42)},
        {RR32(XOR, MEM_DISP8 , EBP, ESP, 0x42), WantOp(0x31, 0x65, 0x42)},
        {RR32(XOR, MEM_DISP8 , ESI, EDX, 0x42), WantOp(0x31, 0x56, 0x42)},
        {RR32(XOR, MEM_DISP32, ECX, EAX, 0x42424342), WantOp(0x31, 0x81, 0x42, 0x43, 0x42, 0x42)},
        {RR32(XOR, MEM_DISP32, ESP, EDI, 0x42424342), WantOp(0x31, 0xBC, 0x24, 0x42, 0x43, 0x42, 0x42)},
        {RR32(XOR, MEM_DISP32, EBP, ECX, 0x42424342), WantOp(0x31, 0x8D, 0x42, 0x43, 0x42, 0x42)},
        {RR32(XOR, MEM_DISP32, EDI, EBP, 0x42424342), WantOp(0x31, 0xAF, 0x42, 0x43, 0x42, 0x42)},
        //   Remaining 32bit opcodes
        {RR32(AND , REG, EAX, EAX, 0), WantOp(0x21, 0xC0)},
        {RR32(OR  , REG, EAX, EAX, 0), WantOp(0x09, 0xC0)},
        {RR32(CMP , REG, EAX, EAX, 0), WantOp(0x39, 0xC0)},
        {RR32(MOV , REG, EAX, EAX, 0), WantOp(0x89, 0xC0)},
        {RR32(MOVR, MEM, EAX, EAX, 0), WantOp(0x8B, 0x00)}, // MEM To see if its actually reversed

        // TODO RI8 and RI32 cases
    };

    // AssembleInstructions needs a contiguous buffer because it fills in jump offsets.
    // Too bad I can't make a struct of arrays and keep the want next to the input.
    instruction Instructions[ArrayLength(Cases)];
    for (size_t i = 0; i < ArrayLength(Cases); ++i) {
        Instructions[i] = Cases[i].Input;
    }
    opcode_unpacked Opcodes[ArrayLength(Cases)];
    AssembleInstructions(Instructions, ArrayLength(Instructions), Opcodes);

    size_t PassCount = 0;
    for (size_t i = 0; i < ArrayLength(Cases); ++i) {
        uint8_t Got[MAX_OPCODE_LEN];
        size_t GotLen = PackCode(&Opcodes[i], 1, Got);

        // Easy way to see the output for new cases so I can double check with a disassembler
        if (Cases[i].Want.Size == 0) {
            PrintByteCode(Got, GotLen, false);
            Print("\n");
            continue;
        }

        bool Passed = true;
        if (GotLen != Cases[i].Want.Size) {
            Passed = false;
        }
        for (size_t j = 0; Passed && j < GotLen; ++j) {
            if (Got[j] != Cases[i].Want.Bytes[j]) {
                Passed = false;
                break;
            }
        }
        if (Passed) {
            PassCount += 1;
            Print("PASS ");
            PrintInstruction(&Cases[i].Input);
            Print(" => ");
            PrintByteCode(Got, GotLen, false);
            Print("\n");
        } else {
            TestFailed();
            Print("FAIL ");
            PrintInstruction(&Cases[i].Input);
            Print(" => ");
            PrintByteCode(Got, GotLen, false);
            Print("; Wanted ");
            PrintByteCode(Cases[i].Want.Bytes, Cases[i].Want.Size, false);
            Print("\n");
        }
    }
    Print("%u / %u cases passed\n", PassCount, ArrayLength(Cases));
}
