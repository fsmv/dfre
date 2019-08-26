// Copyright (c) 2016-2019 Andrew Kallmeyer <fsmv@sapium.net>
// Provided under the MIT License: https://mit-license.org

#include "x86_opcode.h"
#include "mem_arena.h"

#define WantOp(...) opcode_want{{__VA_ARGS__}, sizeof((uint8_t[]){__VA_ARGS__})}
struct opcode_want {
    uint8_t Bytes[MAX_OPCODE_LEN];
    size_t Size;
};

struct opcode_case {
    instruction Input;
    opcode_want Want;
};

static const char IndentBuffer[] = "        ";
static const int IndentAmount = 2;
inline const char *GetIndent(int IndentLevel) {
    const char *Indent = IndentBuffer + ArrayLength(IndentBuffer) - 1 -
                         IndentAmount * IndentLevel;
    Assert(Indent >= IndentBuffer && Indent <= IndentBuffer + ArrayLength(IndentBuffer) - 1);
    return Indent;
}

void TestOpcodes(tester_state *T, const char *Name, int IndentLevel, opcode_case *Cases, size_t NumCases) {
    const char *Indent = GetIndent(IndentLevel);
    Print("%s%s\n", Indent, Name);
    Indent = GetIndent(IndentLevel+1);

    // AssembleInstructions needs a contiguous buffer because it fills in jump offsets.
    instruction *Instructions = (instruction*)Alloc(&T->Arena, NumCases * sizeof(instruction));
    for (size_t i = 0; i < NumCases; ++i) {
        Instructions[i] = Cases[i].Input;
    }
    opcode_unpacked *Opcodes = (opcode_unpacked*)Alloc(&T->Arena, NumCases * sizeof(opcode_unpacked));
    AssembleInstructions(Instructions, NumCases, Opcodes);

    size_t PassCount = 0;
    for (size_t i = 0; i < NumCases; ++i) {
        uint8_t Got[MAX_OPCODE_LEN];
        size_t GotLen = PackCode(&Opcodes[i], 1, Got);

        // Make sure we don't overflow
        if (GotLen > MAX_OPCODE_LEN) {
            T->Failed = true;
            Print("%sFAIL PackCode(NextOp) = %u bytes > MAX_OPCODE_LEN (%u)\n",
                  Indent, GotLen, MAX_OPCODE_LEN);
        }
        // Sneak in a test of SizeOpcode to re-use the test data
        size_t PredictedSize = SizeOpcode(Opcodes[i]);
        if (PredictedSize != GotLen) {
            T->Failed = true;
            Print("%sFAIL SizeOpcode(NextOp) = %u bytes != PackCode(Op) = %u bytes\n",
                  Indent, PredictedSize, GotLen);
        }

        // Easy way to see the output for new cases so I can double check with a disassembler
        if (Cases[i].Want.Size == 0) {
            Print(Indent);
            PrintByteCode(Got, GotLen, /*PrintSize*/false);
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
            Print("%sPASS ", Indent);
            PrintInstruction(&Cases[i].Input);
            Print(" => ");
            PrintByteCode(Got, GotLen, /*PrintSize*/false);
        } else {
            T->Failed = true;
            Print("%sFAIL ", Indent);
            PrintInstruction(&Cases[i].Input);
            Print(" => ");
            PrintByteCode(Got, GotLen, /*PrintSize*/false, /*PrintNewlines*/false);
            Print("; Wanted ");
            PrintByteCode(Cases[i].Want.Bytes, Cases[i].Want.Size, /*PrintSize*/false);
        }
    }
    Print("%s%u / %u cases passed\n\n", Indent, PassCount, NumCases);
}

void TestOpNoarg(tester_state *T) {
    opcode_case Cases[] = {
        {RET, WantOp(0xC3)},
    };
    TestOpcodes(T, "OpNoarg(..) - All no argument opcodes", 0, Cases, ArrayLength(Cases));
}

void TestOpReg(tester_state *T) {
    Print("OpReg(..) - All single register instructions\n");
    const char *Indent1 = GetIndent(1);

    Print("%sAll normal 32 bit options\n", Indent1);
    {
        opcode_case Cases[] = {
            {R32(INC , MEM, EAX, 0), WantOp(0xFF, 0x00)},
            {R32(NOT , MEM, EAX, 0), WantOp(0xF7, 0x10)},
            {R32(PUSH, MEM, EAX, 0), WantOp(0xFF, 0x30)},
            {R32(POP , MEM, EAX, 0), WantOp(0x8F, 0x00)},
        };
        TestOpcodes(T, "All 32 bit ops", 2, Cases, ArrayLength(Cases));
    }
    {
        opcode_case Cases[] = {
            {R32(NOT, REG, EAX, 0), WantOp(0xF7, 0xD0)},
            {R32(NOT, REG, EBX, 0), WantOp(0xF7, 0xD3)},
            {R32(NOT, REG, ECX, 0), WantOp(0xF7, 0xD1)},
            {R32(NOT, REG, EDX, 0), WantOp(0xF7, 0xD2)},
            {R32(NOT, REG, ESP, 0), WantOp(0xF7, 0xD4)},
            {R32(NOT, REG, EBP, 0), WantOp(0xF7, 0xD5)},
            {R32(NOT, REG, ESI, 0), WantOp(0xF7, 0xD6)},
            {R32(NOT, REG, EDI, 0), WantOp(0xF7, 0xD7)},
        };
        TestOpcodes(T, "Register direct mode, all registers", 2, Cases, ArrayLength(Cases));
    }
    {
        opcode_case Cases[] = {
            {R32(POP, MEM, EAX, 0), WantOp(0x8F, 0x00)},
            {R32(POP, MEM, EBX, 0), WantOp(0x8F, 0x03)},
            {R32(POP, MEM, ECX, 0), WantOp(0x8F, 0x01)},
            {R32(POP, MEM, EDX, 0), WantOp(0x8F, 0x02)},
            {R32(POP, MEM, ESP, 0), WantOp(0x8F, 0x04, 0x24)}, // (MEM || MEM_DISP*) && ESP is special branch
            {R32(POP, MEM, EBP, 0), WantOp(0x8F, 0x45, 0x00)}, // MEM && EBP is a special branch
            {R32(POP, MEM, ESI, 0), WantOp(0x8F, 0x06)},
            {R32(POP, MEM, EDI, 0), WantOp(0x8F, 0x07)},
        };
        TestOpcodes(T, "Dereference register mode, all registers", 2, Cases, ArrayLength(Cases));
    }
    {
        opcode_case Cases[] = {
            {R32(PUSH, MEM_DISP8, EAX, 0xAC), WantOp(0xFF, 0x70, 0xAC)},
            {R32(POP , MEM_DISP8, EBX, 0),    WantOp(0x8F, 0x43, 0x00)},
            {R32(POP , MEM_DISP8, ECX, 0),    WantOp(0x8F, 0x41, 0x00)},
            {R32(POP , MEM_DISP8, EDX, 0),    WantOp(0x8F, 0x42, 0x00)},
            {R32(PUSH, MEM_DISP8, ESP, 0xAC), WantOp(0xFF, 0x74, 0x24, 0xAC)},
            {R32(PUSH, MEM_DISP8, EBP, 0xAC), WantOp(0xFF, 0x75, 0xAC)},
            {R32(POP , MEM_DISP8, ESI, 0),    WantOp(0x8F, 0x46, 0x00)},
            {R32(PUSH, MEM_DISP8, EDI, 0xAC), WantOp(0xFF, 0x77, 0xAC)},
        };
        TestOpcodes(T, "Dereference register with 8 bit displacement mode, all registers", 2, Cases, ArrayLength(Cases));
    }
    {
        opcode_case Cases[] = {
            {R32(PUSH, MEM_DISP32, EAX, 0x1337ABCD), WantOp(0xFF, 0xB0, 0xCD, 0xAB, 0x37, 0x13)},
            {R32(POP , MEM_DISP32, EBX, 0x88888888), WantOp(0x8F, 0x83, 0x88, 0x88, 0x88, 0x88)},
            {R32(PUSH, MEM_DISP32, ECX, 0xBADF00D),  WantOp(0xFF, 0xB1, 0x0D, 0xF0, 0xAD, 0x0B)},
            {R32(POP , MEM_DISP32, EDX, 0xCA7F00D),  WantOp(0x8F, 0x82, 0x0D, 0xF0, 0xA7, 0x0C)},
            {R32(PUSH, MEM_DISP32, ESP, 0xBADF00D),  WantOp(0xFF, 0xB4, 0x24, 0x0D, 0xF0, 0xAD, 0x0B)},
            {R32(PUSH, MEM_DISP32, EBP, 0xBADF00D),  WantOp(0xFF, 0xB5, 0x0D, 0xF0, 0xAD, 0x0B)},
            {R32(PUSH, MEM_DISP32, ESI, 0xBADF00D),  WantOp(0xFF, 0xB6, 0x0D, 0xF0, 0xAD, 0x0B)},
            {R32(PUSH, MEM_DISP32, EDI, 0xD06F00D),  WantOp(0xFF, 0xB7, 0x0D, 0xF0, 0x06, 0x0D)},
        };
        TestOpcodes(T, "Dereference register with 32 bit displacement mode, all registers", 2, Cases, ArrayLength(Cases));
    }

    Print("%sAll ShortReg (single byte, 32 bit only) options\n", Indent1);
    {
        opcode_case Cases[] = {
            {R32(INC , REG, EDX, 0), WantOp(0x42)},
            {R32(PUSH, REG, ESP, 0), WantOp(0x54)},
            {R32(POP , REG, EBP, 0), WantOp(0x5D)},
        };
        TestOpcodes(T, "All ShortReg ops", 2, Cases, ArrayLength(Cases));
    }
    {
        opcode_case Cases[] = {
            {R32(INC, REG, EAX, 0), WantOp(0x40)},
            {R32(INC, REG, EBX, 0), WantOp(0x43)},
            {R32(INC, REG, ECX, 0), WantOp(0x41)},
            {R32(INC, REG, EDX, 0), WantOp(0x42)},
            {R32(INC, REG, ESP, 0), WantOp(0x44)},
            {R32(INC, REG, EBP, 0), WantOp(0x45)},
            {R32(INC, REG, ESI, 0), WantOp(0x46)},
            {R32(INC, REG, EDI, 0), WantOp(0x47)},
        };
        TestOpcodes(T, "All modes (register direct only), all registers", 2, Cases, ArrayLength(Cases));
    }

    Print("%sAll 8 bit options\n", Indent1);
    {
        opcode_case Cases[] = {
            {R8(INC, REG, EAX, 0), WantOp(0xFE, 0xC0)},
            {R8(NOT, REG, EAX, 0), WantOp(0xF6, 0xD0)},
        };
        TestOpcodes(T, "All 8 bit ops", 2, Cases, ArrayLength(Cases));
    }
    {
        opcode_case Cases[] = {
            {R8(NOT, REG, EAX, 0), WantOp(0xF6, 0xD0)},
            {R8(NOT, REG, EBX, 0), WantOp(0xF6, 0xD3)},
            {R8(NOT, REG, ECX, 0), WantOp(0xF6, 0xD1)},
            {R8(NOT, REG, EDX, 0), WantOp(0xF6, 0xD2)},
        };
        TestOpcodes(T, "Register direct mode, all registers (restricted set)", 2, Cases, ArrayLength(Cases));
    }
    {
        opcode_case Cases[] = {
            {R8(INC, MEM, EAX, 0), WantOp(0xFE, 0x00)},
            {R8(INC, MEM, EBX, 0), WantOp(0xFE, 0x03)},
            {R8(NOT, MEM, ECX, 0), WantOp(0xF6, 0x11)},
            {R8(NOT, MEM, EDX, 0), WantOp(0xF6, 0x12)},
            {R8(NOT, MEM, ESP, 0), WantOp(0xF6, 0x14, 0x24)},
            {R8(NOT, MEM, EBP, 0), WantOp(0xF6, 0x55, 0x00)},
            {R8(INC, MEM, ESI, 0), WantOp(0xFE, 0x06)},
            {R8(INC, MEM, EDI, 0), WantOp(0xFE, 0x07)},
        };
        TestOpcodes(T, "Dereference register mode, all registers", 2, Cases, ArrayLength(Cases));
    }
    {
        opcode_case Cases[] = {
            {R8(NOT, MEM_DISP8, EAX, 0xAB), WantOp(0xF6, 0x50, 0xAB)},
            {R8(NOT, MEM_DISP8, EBX, 0x00), WantOp(0xF6, 0x53, 0x00)},
            {R8(NOT, MEM_DISP8, ECX, 0xCD), WantOp(0xF6, 0x51, 0xCD)},
            {R8(INC, MEM_DISP8, EDX, 0xEF), WantOp(0xFE, 0x42, 0xEF)},
            {R8(INC, MEM_DISP8, ESP, 0x99), WantOp(0xFE, 0x44, 0x24, 0x99)},
            {R8(INC, MEM_DISP8, EBP, 0x33), WantOp(0xFE, 0x45, 0x33)},
            {R8(INC, MEM_DISP8, ESI, 0x55), WantOp(0xFE, 0x46, 0x55)},
            {R8(NOT, MEM_DISP8, EDI, 0x13), WantOp(0xF6, 0x57, 0x13)},
        };
        TestOpcodes(T, "Dereference register with 8 bit displacement mode, all registers", 2, Cases, ArrayLength(Cases));
    }
    {
        opcode_case Cases[] = {
            {R8(NOT, MEM_DISP32, EAX, 0x1337ABCD), WantOp(0xF6, 0x90, 0xCD, 0xAB, 0x37, 0x13)},
            {R8(NOT, MEM_DISP32, EBX, 0x88888888), WantOp(0xF6, 0x93, 0x88, 0x88, 0x88, 0x88)},
            {R8(NOT, MEM_DISP32, ECX, 0xBADF00D) , WantOp(0xF6, 0x91, 0x0D, 0xF0, 0xAD, 0x0B)},
            {R8(NOT, MEM_DISP32, EDX, 0xCA7F00D) , WantOp(0xF6, 0x92, 0x0D, 0xF0, 0xA7, 0x0C)},
            {R8(INC, MEM_DISP32, ESP, 0xBADF00D) , WantOp(0xFE, 0x84, 0x24, 0x0D, 0xF0, 0xAD, 0x0B)},
            {R8(INC, MEM_DISP32, EBP, 0xBADF00D) , WantOp(0xFE, 0x85, 0x0D, 0xF0, 0xAD, 0x0B)},
            {R8(INC, MEM_DISP32, ESI, 0xBADF00D) , WantOp(0xFE, 0x86, 0x0D, 0xF0, 0xAD, 0x0B)},
            {R8(INC, MEM_DISP32, EDI, 0xD06F00D) , WantOp(0xFE, 0x87, 0x0D, 0xF0, 0x06, 0x0D)},
        };
        TestOpcodes(T, "Dereference register with 32 bit displacement mode, all registers", 2, Cases, ArrayLength(Cases));
    }
}

void TestOpRegReg(tester_state *T) {
    Print("OpRegReg(..) - All two register instructions\n");
    const char *Indent1 = GetIndent(1);

    Print("%sAll 32 bit options\n", Indent1);
    {
        opcode_case Cases[] = {
            {RR32(AND , REG, EAX, EAX, 0), WantOp(0x21, 0xC0)},
            {RR32(OR  , REG, EAX, EAX, 0), WantOp(0x09, 0xC0)},
            {RR32(XOR , REG, EAX, EAX, 0), WantOp(0x31, 0xC0)},
            {RR32(CMP , REG, EAX, EAX, 0), WantOp(0x39, 0xC0)},
            {RR32(MOV , REG, EAX, EAX, 0), WantOp(0x89, 0xC0)},
            {RR32(MOVR, MEM, EAX, EAX, 0), WantOp(0x8B, 0x00)}, // MEM To see if its actually reversed
        };
        TestOpcodes(T, "All 32 bit ops", 2, Cases, ArrayLength(Cases));
    }
    {
        opcode_case Cases[] = {
            {RR32(XOR, REG, EAX, EDI, 0), WantOp(0x31, 0xF8)},
            {RR32(XOR, REG, EBX, ESI, 0), WantOp(0x31, 0xF3)},
            {RR32(XOR, REG, ECX, EBP, 0), WantOp(0x31, 0xE9)},
            {RR32(XOR, REG, EDX, ESP, 0), WantOp(0x31, 0xE2)},
            {RR32(XOR, REG, ESP, EDX, 0), WantOp(0x31, 0xD4)},
            {RR32(XOR, REG, EBP, ECX, 0), WantOp(0x31, 0xCD)},
            {RR32(XOR, REG, ESI, EBX, 0), WantOp(0x31, 0xDE)},
            {RR32(XOR, REG, EDI, EAX, 0), WantOp(0x31, 0xC7)},
        };
        TestOpcodes(T, "Register direct mode, all registers", 2, Cases, ArrayLength(Cases));
    }
    {
        opcode_case Cases[] = {
            {RR32(XOR, MEM, EAX, EDI, 0), WantOp(0x31, 0x38)},
            {RR32(XOR, MEM, EBX, ESI, 0), WantOp(0x31, 0x33)},
            {RR32(XOR, MEM, ECX, EBP, 0), WantOp(0x31, 0x29)},
            {RR32(XOR, MEM, EDX, ESP, 0), WantOp(0x31, 0x22)},
            {RR32(XOR, MEM, ESP, EDX, 0), WantOp(0x31, 0x14, 0x24)},
            {RR32(XOR, MEM, EBP, ECX, 0), WantOp(0x31, 0x4D, 0x00)},
            {RR32(XOR, MEM, ESI, EBX, 0), WantOp(0x31, 0x1E)},
            {RR32(XOR, MEM, EDI, EAX, 0), WantOp(0x31, 0x07)},
        };
        TestOpcodes(T, "Dereference register mode, all registers", 2, Cases, ArrayLength(Cases));
    }
    {
        opcode_case Cases[] = {
            {RR32(XOR, MEM_DISP8 , EAX, EAX, 0x42), WantOp(0x31, 0x40, 0x42)},
            {RR32(XOR, MEM_DISP8 , EBX, ESI, 0x42), WantOp(0x31, 0x73, 0x42)},
            {RR32(XOR, MEM_DISP8 , ECX, ESP, 0x42), WantOp(0x31, 0x61, 0x42)},
            {RR32(XOR, MEM_DISP8 , EDX, EAX, 0x42), WantOp(0x31, 0x42, 0x42)},
            {RR32(XOR, MEM_DISP8 , ESP, EBX, 0x42), WantOp(0x31, 0x5C, 0x24, 0x42)},
            {RR32(XOR, MEM_DISP8 , EBP, ESP, 0x42), WantOp(0x31, 0x65, 0x42)},
            {RR32(XOR, MEM_DISP8 , ESI, EDX, 0x42), WantOp(0x31, 0x56, 0x42)},
            {RR32(XOR, MEM_DISP8 , EDI, EDX, 0x42), WantOp(0x31, 0x57, 0x42)},
        };
        TestOpcodes(T, "Dereference register with 8 bit displacement mode, all registers", 2, Cases, ArrayLength(Cases));
    }
    {
        opcode_case Cases[] = {
            {RR32(XOR, MEM_DISP32, EAX, ECX, 0x42424342), WantOp(0x31, 0x88, 0x42, 0x43, 0x42, 0x42)},
            {RR32(XOR, MEM_DISP32, EBX, EAX, 0x42424342), WantOp(0x31, 0x83, 0x42, 0x43, 0x42, 0x42)},
            {RR32(XOR, MEM_DISP32, ECX, ESP, 0x42424342), WantOp(0x31, 0xA1, 0x42, 0x43, 0x42, 0x42)},
            {RR32(XOR, MEM_DISP32, EDX, EDI, 0x42424342), WantOp(0x31, 0xBA, 0x42, 0x43, 0x42, 0x42)},
            {RR32(XOR, MEM_DISP32, ESP, EDI, 0x42424342), WantOp(0x31, 0xBC, 0x24, 0x42, 0x43, 0x42, 0x42)},
            {RR32(XOR, MEM_DISP32, EBP, ECX, 0x42424342), WantOp(0x31, 0x8D, 0x42, 0x43, 0x42, 0x42)},
            {RR32(XOR, MEM_DISP32, ESI, EBP, 0x42424342), WantOp(0x31, 0xAE, 0x42, 0x43, 0x42, 0x42)},
            {RR32(XOR, MEM_DISP32, EDI, EBP, 0x42424342), WantOp(0x31, 0xAF, 0x42, 0x43, 0x42, 0x42)},
        };
        TestOpcodes(T, "Dereference register with 32 bit displacement mode, all registers", 2, Cases, ArrayLength(Cases));
    }

    Print("%sAll 8 bit options\n", Indent1);
    {
        opcode_case Cases[] = {
            {RR8(AND , REG, EAX, EAX, 0), WantOp(0x20, 0xC0)},
            {RR8(OR  , REG, EAX, EAX, 0), WantOp(0x08, 0xC0)},
            {RR8(XOR , REG, EAX, EAX, 0), WantOp(0x30, 0xC0)},
            {RR8(CMP , REG, EAX, EAX, 0), WantOp(0x38, 0xC0)},
            {RR8(MOV , REG, EAX, EAX, 0), WantOp(0x88, 0xC0)},
            {RR8(MOVR, MEM, EAX, EAX, 0), WantOp(0x8A, 0x00)}, // MEM To see if its actually reversed
        };
        TestOpcodes(T, "All 8 bit ops", 2, Cases, ArrayLength(Cases));
    }
    {
        opcode_case Cases[] = {
            //   8bit reg. ESP, EBP, ESI, EDI not encodable for either arg
            {RR8(AND, REG, EAX, EDX, 0), WantOp(0x20, 0xD0)},
            {RR8(AND, REG, EBX, ECX, 0), WantOp(0x20, 0xCB)},
            {RR8(AND, REG, ECX, EBX, 0), WantOp(0x20, 0xD9)},
            {RR8(AND, REG, EDX, EAX, 0), WantOp(0x20, 0xC2)},
        };
        TestOpcodes(T, "Register direct mode, all registers (restricted set)", 2, Cases, ArrayLength(Cases));
    }
    {
        opcode_case Cases[] = {
            //   8bit mem. ESP, EBP, ESI, EDI not encodable for Src
            {RR8(AND, MEM, EAX, EDX, 0), WantOp(0x20, 0x10)},
            {RR8(AND, MEM, EBX, ECX, 0), WantOp(0x20, 0x0B)},
            {RR8(AND, MEM, ECX, EBX, 0), WantOp(0x20, 0x19)},
            {RR8(AND, MEM, EDX, EAX, 0), WantOp(0x20, 0x02)},
            {RR8(AND, MEM, ESP, EBX, 0), WantOp(0x20, 0x1C, 0x24)}, // Same special case as in single reg
            {RR8(AND, MEM, EBP, EAX, 0), WantOp(0x20, 0x45, 0x00)}, // Same special case as in single reg
            {RR8(AND, MEM, ESI, EDX, 0), WantOp(0x20, 0x16)},
            {RR8(AND, MEM, EDI, ECX, 0), WantOp(0x20, 0x0F)},
        };
        TestOpcodes(T, "Dereference register mode, all registers (restricted set)", 2, Cases, ArrayLength(Cases));
    }
    {
        opcode_case Cases[] = {
            //   8bit mem_disp8. ESP, EBP, ESI, EDI not encodable for src
            {RR8(AND, MEM_DISP8 , EAX, EAX, 0x42), WantOp(0x20, 0x40, 0x42)},
            {RR8(AND, MEM_DISP8 , EBX, EDX, 0x42), WantOp(0x20, 0x53, 0x42)},
            {RR8(AND, MEM_DISP8 , ECX, EBX, 0x42), WantOp(0x20, 0x59, 0x42)},
            {RR8(AND, MEM_DISP8 , EDX, ECX, 0x42), WantOp(0x20, 0x4A, 0x42)},
            {RR8(AND, MEM_DISP8 , ESP, EBX, 0x42), WantOp(0x20, 0x5C, 0x24, 0x42)},
            {RR8(AND, MEM_DISP8 , EBP, ECX, 0x42), WantOp(0x20, 0x4D, 0x42)},
            {RR8(AND, MEM_DISP8 , ESI, EDX, 0x42), WantOp(0x20, 0x56, 0x42)},
            {RR8(AND, MEM_DISP8 , EDI, EDX, 0x42), WantOp(0x20, 0x57, 0x42)},
        };
        TestOpcodes(T, "Dereference register with 8 bit displacement mode, all registers (restricted set)", 2, Cases, ArrayLength(Cases));
    }
    {
        opcode_case Cases[] = {
            //   8bit mem_disp32. ESP, EBP, ESI, EDI not encodable for src
            {RR8(AND, MEM_DISP32, EAX, EDX, 0x43424242), WantOp(0x20, 0x90, 0x42, 0x42, 0x42, 0x43)},
            {RR8(AND, MEM_DISP32, EBX, ECX, 0x43424242), WantOp(0x20, 0x8B, 0x42, 0x42, 0x42, 0x43)},
            {RR8(AND, MEM_DISP32, ECX, EBX, 0x43424242), WantOp(0x20, 0x99, 0x42, 0x42, 0x42, 0x43)},
            {RR8(AND, MEM_DISP32, EDX, EAX, 0x43424242), WantOp(0x20, 0x82, 0x42, 0x42, 0x42, 0x43)},
            {RR8(AND, MEM_DISP32, ESP, EBX, 0x43424242), WantOp(0x20, 0x9C, 0x24, 0x42, 0x42, 0x42, 0x43)},
            {RR8(AND, MEM_DISP32, EBP, ECX, 0x43424242), WantOp(0x20, 0x8D, 0x42, 0x42, 0x42, 0x43)},
            {RR8(AND, MEM_DISP32, ESI, EDX, 0x43424242), WantOp(0x20, 0x96, 0x42, 0x42, 0x42, 0x43)},
            {RR8(AND, MEM_DISP32, EDI, EDX, 0x43424242), WantOp(0x20, 0x97, 0x42, 0x42, 0x42, 0x43)},
        };
        TestOpcodes(T, "Dereference register with 32 bit displacement mode, all registers (restricted set)", 2, Cases, ArrayLength(Cases));
    }
}

void TestOpRegImm(tester_state *T) {
    Print("OpRegImm(..) - All register immediate instructions\n");
    const char *Indent1 = GetIndent(1);

    Print("%sAll 32 bit options\n", Indent1);
    {
        opcode_case Cases[] = {
            {RI32(AND, REG, EAX, /*disp*/0, /*imm*/0x88888888), WantOp(0x81, 0xE0, 0x88, 0x88, 0x88, 0x88)},
            {RI32(XOR, REG, EBX, /*disp*/0, /*imm*/0x3CAD3C00), WantOp(0x81, 0xF3, 0x00, 0x3C, 0xAD, 0x3C)},
            {RI32(CMP, REG, ECX, /*disp*/0, /*imm*/0x123),      WantOp(0x81, 0xF9, 0x23, 0x01, 0x00, 0x00)},
            {RI32(MOV, REG, EDX, /*disp*/0, /*imm*/0x1),        WantOp(0xBA, 0x01, 0x00, 0x00, 0x00)},
        };
        TestOpcodes(T, "All 32 bit ops", 2, Cases, ArrayLength(Cases));
    }
    {
        opcode_case Cases[] = {
            {RI32(CMP, REG, EAX, /*disp*/0, /*imm*/0xACEDACED), WantOp(0x81, 0xF8, 0xED, 0xAC, 0xED, 0xAC)},
            {RI32(CMP, REG, EBX, /*disp*/0, /*imm*/0x3CAD3C00), WantOp(0x81, 0xFB, 0x00, 0x3C, 0xAD, 0x3C)},
            {RI32(CMP, REG, ECX, /*disp*/0, /*imm*/0),          WantOp(0x81, 0xF9, 0x00, 0x00, 0x00, 0x00)},
            {RI32(CMP, REG, EDX, /*disp*/0, /*imm*/0x0FADFAD0), WantOp(0x81, 0xFA, 0xD0, 0xFA, 0xAD, 0x0F)},
        };
        TestOpcodes(T, "Register direct mode, all registers", 2, Cases, ArrayLength(Cases));
    }
    {
        opcode_case Cases[] = {
            {RI32(MOV, MEM, EAX, /*disp*/0, /*imm*/0xAB),       WantOp(0xC7, 0x00, 0xAB, 0x00, 0x00, 0x00)},
            {RI32(MOV, MEM, EBX, /*disp*/0, /*imm*/0xABCD),     WantOp(0xC7, 0x03, 0xCD, 0xAB, 0x00, 0x00)},
            {RI32(MOV, MEM, ECX, /*disp*/0, /*imm*/0xABCDEF),   WantOp(0xC7, 0x01, 0xEF, 0xCD, 0xAB, 0x00)},
            {RI32(MOV, MEM, EDX, /*disp*/0, /*imm*/0xBADF00D),  WantOp(0xC7, 0x02, 0x0D, 0xF0, 0xAD, 0x0B)},
            {RI32(MOV, MEM, ESP, /*disp*/0, /*imm*/0xDEADBEEF), WantOp(0xC7, 0x04, 0x24, 0xEF, 0xBE, 0xAD, 0xDE)},
            {RI32(MOV, MEM, EBP, /*disp*/0, /*imm*/0xCAFE),     WantOp(0xC7, 0x45, 0x00, 0xFE, 0xCA, 0x00, 0x00)},
            {RI32(MOV, MEM, ESI, /*disp*/0, /*imm*/0xC0FEFEE),  WantOp(0xC7, 0x06, 0xEE, 0xEF, 0x0F, 0x0C)},
            {RI32(MOV, MEM, EDI, /*disp*/0, /*imm*/0xFFFFFFFF), WantOp(0xC7, 0x07, 0xFF, 0xFF, 0xFF, 0xFF)},
        };
        TestOpcodes(T, "Dereference register mode, all registers", 2, Cases, ArrayLength(Cases));
    }
    {
        opcode_case Cases[] = {
            {RI32(MOV, MEM_DISP8, EAX, /*disp*/0x42, /*imm*/0xAB),       WantOp(0xC7, 0x40, 0x42, 0xAB, 0x00, 0x00, 0x00)},
            {RI32(MOV, MEM_DISP8, EBX, /*disp*/0,    /*imm*/0xABCD),     WantOp(0xC7, 0x43, 0x00, 0xCD, 0xAB, 0x00, 0x00)},
            {RI32(MOV, MEM_DISP8, ECX, /*disp*/0xC4, /*imm*/0xABCDEF),   WantOp(0xC7, 0x41, 0xC4, 0xEF, 0xCD, 0xAB, 0x00)},
            {RI32(MOV, MEM_DISP8, EDX, /*disp*/0xAB, /*imm*/0xBADF00D),  WantOp(0xC7, 0x42, 0xAB, 0x0D, 0xF0, 0xAD, 0x0B)},
            {RI32(MOV, MEM_DISP8, ESP, /*disp*/0x7,  /*imm*/0xDEADBEEF), WantOp(0xC7, 0x44, 0x24, 0x07, 0xEF, 0xBE, 0xAD, 0xDE)},
            {RI32(MOV, MEM_DISP8, EBP, /*disp*/0x32, /*imm*/0xCAFE),     WantOp(0xC7, 0x45, 0x32, 0xFE, 0xCA, 0x00, 0x00)},
            {RI32(MOV, MEM_DISP8, ESI, /*disp*/0x69, /*imm*/0xC0FEFEE),  WantOp(0xC7, 0x46, 0x69, 0xEE, 0xEF, 0x0F, 0x0C)},
            {RI32(MOV, MEM_DISP8, EDI, /*disp*/0x99, /*imm*/0xFFFFFFFF), WantOp(0xC7, 0x47, 0x99, 0xFF, 0xFF, 0xFF, 0xFF)},
        };
        TestOpcodes(T, "Dereference register with 8 bit displacement mode, all registers", 2, Cases, ArrayLength(Cases));
    }
    {
        opcode_case Cases[] = {
            {RI32(MOV, MEM_DISP32, EAX, /*disp*/0x8675309,  /*imm*/0xAB),       WantOp(0xC7, 0x80, 0x09, 0x53, 0x67, 0x08, 0xAB, 0x00, 0x00, 0x00)},
            {RI32(MOV, MEM_DISP32, EBX, /*disp*/0x11,       /*imm*/0xABCD),     WantOp(0xC7, 0x83, 0x11, 0x00, 0x00, 0x00, 0xCD, 0xAB, 0x00, 0x00)},
            {RI32(MOV, MEM_DISP32, ECX, /*disp*/0x1337,     /*imm*/0xABCDEF),   WantOp(0xC7, 0x81, 0x37, 0x13, 0x00, 0x00, 0xEF, 0xCD, 0xAB, 0x00)},
            {RI32(MOV, MEM_DISP32, EDX, /*disp*/0xCDCDCDCD, /*imm*/0xBADF00D),  WantOp(0xC7, 0x82, 0xCD, 0xCD, 0xCD, 0xCD, 0x0D, 0xF0, 0xAD, 0x0B)},
            {RI32(MOV, MEM_DISP32, ESP, /*disp*/0x12345678, /*imm*/0xDEADBEEF), WantOp(0xC7, 0x84, 0x24, 0x78, 0x56, 0x34, 0x12, 0xEF, 0xBE, 0xAD, 0xDE)},
            {RI32(MOV, MEM_DISP32, EBP, /*disp*/0xCADCADCA, /*imm*/0xCAFE),     WantOp(0xC7, 0x85, 0xCA, 0xAD, 0xDC, 0xCA, 0xFE, 0xCA, 0x00, 0x00)},
            {RI32(MOV, MEM_DISP32, ESI, /*disp*/0,          /*imm*/0xC0FEFEE),  WantOp(0xC7, 0x86, 0x00, 0x00, 0x00, 0x00, 0xEE, 0xEF, 0x0F, 0x0C)},
            {RI32(MOV, MEM_DISP32, EDI, /*disp*/0x88888888, /*imm*/0xFFFFFFFF), WantOp(0xC7, 0x87, 0x88, 0x88, 0x88, 0x88, 0xFF, 0xFF, 0xFF, 0xFF)},
        };
        TestOpcodes(T, "Dereference register with 32 bit displacement mode, all registers", 2, Cases, ArrayLength(Cases));
    }

    // Usually there's an extra header here and an op sebsection, but there's only MOV here
    {
        opcode_case Cases[] = {
            {RI32(MOV, REG, EAX, /*disp*/0, /*imm*/0xAB),       WantOp(0xB8, 0xAB, 0x00, 0x00, 0x00)},
            {RI32(MOV, REG, ECX, /*disp*/0, /*imm*/0xABCD),     WantOp(0xB9, 0xCD, 0xAB, 0x00, 0x00)},
            {RI32(MOV, REG, EDX, /*disp*/0, /*imm*/0xABCDEF),   WantOp(0xBA, 0xEF, 0xCD, 0xAB, 0x00)},
            {RI32(MOV, REG, EBX, /*disp*/0, /*imm*/0xBADF00D),  WantOp(0xBB, 0x0D, 0xF0, 0xAD, 0x0B)},
            {RI32(MOV, REG, ESP, /*disp*/0, /*imm*/0xDEADBEEF), WantOp(0xBC, 0xEF, 0xBE, 0xAD, 0xDE)},
            {RI32(MOV, REG, EBP, /*disp*/0, /*imm*/0xCAFE),     WantOp(0xBD, 0xFE, 0xCA, 0x00, 0x00)},
            {RI32(MOV, REG, ESI, /*disp*/0, /*imm*/0xC0FEFEE),  WantOp(0xBE, 0xEE, 0xEF, 0x0F, 0x0C)},
            {RI32(MOV, REG, EDI, /*disp*/0, /*imm*/0),          WantOp(0xBF, 0x00, 0x00, 0x00, 0x00)},
        };
        TestOpcodes(T, "All ShortReg options (32 bit only) and opcodes", 1, Cases, ArrayLength(Cases));
    }

    Print("%sAll 8 bit options\n", Indent1);
    {
        opcode_case Cases[] = {
            {RI8(BT , REG, EAX, /*disp*/0, /*imm*/0x88), WantOp(0x0F, 0xBA, 0xE0, 0x88)}, // Note the register is still tested as 32 bits, but imm can only be 8 bits
            {RI8(AND, REG, EAX, /*disp*/0, /*imm*/0x88), WantOp(0x80, 0xE0, 0x88)},
            {RI8(XOR, REG, EBX, /*disp*/0, /*imm*/0x3C), WantOp(0x80, 0xF3, 0x3C)},
            {RI8(CMP, REG, ECX, /*disp*/0, /*imm*/0x12), WantOp(0x80, 0xF9, 0x12)},
            {RI8(MOV, REG, EDX, /*disp*/0, /*imm*/0x1),  WantOp(0xC6, 0xC2, 0x01)},
        };
        TestOpcodes(T, "All 8 bit ops", 2, Cases, ArrayLength(Cases));
    }
    {
        opcode_case Cases[] = {
            {RI8(MOV, REG, EAX, /*disp*/0, /*imm*/0x43), WantOp(0xC6, 0xC0, 0x43)},
            {RI8(MOV, REG, EBX, /*disp*/0, /*imm*/0),    WantOp(0xC6, 0xC3, 0x00)},
            {RI8(MOV, REG, ECX, /*disp*/0, /*imm*/0x13), WantOp(0xC6, 0xC1, 0x13)},
            {RI8(MOV, REG, EDX, /*disp*/0, /*imm*/0x77), WantOp(0xC6, 0xC2, 0x77)},
        };
        TestOpcodes(T, "Register direct mode, all registers", 2, Cases, ArrayLength(Cases));
    }
    {
        opcode_case Cases[] = {
            {RI8(CMP, MEM, EAX, /*disp*/0, /*imm*/0x43), WantOp(0x80, 0x38, 0x43)},
            {RI8(CMP, MEM, EBX, /*disp*/0, /*imm*/0xFF), WantOp(0x80, 0x3B, 0xFF)},
            {RI8(CMP, MEM, ECX, /*disp*/0, /*imm*/0x13), WantOp(0x80, 0x39, 0x13)},
            {RI8(CMP, MEM, EDX, /*disp*/0, /*imm*/0x77), WantOp(0x80, 0x3A, 0x77)},
            {RI8(CMP, MEM, ESP, /*disp*/0, /*imm*/0x43), WantOp(0x80, 0x3C, 0x24, 0x43)},
            {RI8(CMP, MEM, EBP, /*disp*/0, /*imm*/0xCC), WantOp(0x80, 0x7D, 0x00, 0xCC)},
            {RI8(CMP, MEM, ESI, /*disp*/0, /*imm*/0x13), WantOp(0x80, 0x3E, 0x13)},
            {RI8(CMP, MEM, EDI, /*disp*/0, /*imm*/0x77), WantOp(0x80, 0x3F, 0x77)},
        };
        TestOpcodes(T, "Dereference register mode, all registers", 2, Cases, ArrayLength(Cases));
    }
    {
        opcode_case Cases[] = {
            {RI8(MOV, MEM_DISP8, EAX, /*disp*/0x12, /*imm*/0x43), WantOp(0xC6, 0x40, 0x12, 0x43)},
            {RI8(MOV, MEM_DISP8, EBX, /*disp*/0x34, /*imm*/0xFF), WantOp(0xC6, 0x43, 0x34, 0xFF)},
            {RI8(MOV, MEM_DISP8, ECX, /*disp*/0x56, /*imm*/0x13), WantOp(0xC6, 0x41, 0x56, 0x13)},
            {RI8(MOV, MEM_DISP8, EDX, /*disp*/0x78, /*imm*/0x77), WantOp(0xC6, 0x42, 0x78, 0x77)},
            {RI8(MOV, MEM_DISP8, ESP, /*disp*/0x9A, /*imm*/0x43), WantOp(0xC6, 0x44, 0x24, 0x9A, 0x43)},
            {RI8(MOV, MEM_DISP8, EBP, /*disp*/0xBC, /*imm*/0xCC), WantOp(0xC6, 0x45, 0xBC, 0xCC)},
            {RI8(MOV, MEM_DISP8, ESI, /*disp*/0xDE, /*imm*/0x13), WantOp(0xC6, 0x46, 0xDE, 0x13)},
            {RI8(MOV, MEM_DISP8, EDI, /*disp*/0xF0, /*imm*/0x77), WantOp(0xC6, 0x47, 0xF0, 0x77)},
        };
        TestOpcodes(T, "Dereference register with 8 bit displacement mode, all registers", 2, Cases, ArrayLength(Cases));
    }
    {
        opcode_case Cases[] = {
            {RI8(MOV, MEM_DISP32, EAX, /*disp*/0x1337,     /*imm*/0x43), WantOp(0xC6, 0x80, 0x37, 0x13, 0x00, 0x00, 0x43)},
            {RI8(MOV, MEM_DISP32, EBX, /*disp*/0,          /*imm*/0xFF), WantOp(0xC6, 0x83, 0x00, 0x00, 0x00, 0x00, 0xFF)},
            {RI8(MOV, MEM_DISP32, ECX, /*disp*/0x55555555, /*imm*/0x13), WantOp(0xC6, 0x81, 0x55, 0x55, 0x55, 0x55, 0x13)},
            {RI8(MOV, MEM_DISP32, EDX, /*disp*/0x666,      /*imm*/0x77), WantOp(0xC6, 0x82, 0x66, 0x06, 0x00, 0x00, 0x77)},
            {RI8(MOV, MEM_DISP32, ESP, /*disp*/0x42,       /*imm*/0x43), WantOp(0xC6, 0x84, 0x24, 0x42, 0x00, 0x00, 0x00, 0x43)},
            {RI8(MOV, MEM_DISP32, EBP, /*disp*/0xABEABE,   /*imm*/0xCC), WantOp(0xC6, 0x85, 0xBE, 0xEA, 0xAB, 0x00, 0xCC)},
            {RI8(MOV, MEM_DISP32, ESI, /*disp*/0x12345678, /*imm*/0x13), WantOp(0xC6, 0x86, 0x78, 0x56, 0x34, 0x12, 0x13)},
            {RI8(MOV, MEM_DISP32, EDI, /*disp*/0x3,        /*imm*/0x77), WantOp(0xC6, 0x87, 0x03, 0x00, 0x00, 0x00, 0x77)},
        };
        TestOpcodes(T, "Dereference register with 32 bit displacement mode, all registers", 2, Cases, ArrayLength(Cases));
    }
}

void TestOpJump(tester_state *T) {
    {
        opcode_case Cases[] = {
            {JD(JMP, /*instrIdx=*/5), WantOp(0xEB, 0x08)},
            {JD(JNC, /*instrIdx=*/4), WantOp(0x73, 0x04)},
            {JD(JE , /*instrIdx=*/2), WantOp(0x74, 0xFE)},
            {JD(JNE, /*instrIdx=*/1), WantOp(0x75, 0xFA)},
            {JD(JL , /*instrIdx=*/0), WantOp(0x7C, 0xF6)},
            {JD(JG , /*instrIdx=*/3), WantOp(0x7F, 0xFA)},
        };
        TestOpcodes(T, "OpJump8(..) - All 8 bit jumps with offset", 0, Cases, ArrayLength(Cases));
    }
    {
        opcode_case Cases[] = {
            {JD(JMP, /*instrIdx=*/14), WantOp(0xE9, 0x22, 0x00, 0x00, 0x00)},
            {JD(JNC, /*instrIdx=*/15), WantOp(0x0F, 0x83, 0x22, 0x00, 0x00, 0x00)},
            {JD(JE , /*instrIdx=*/16), WantOp(0x0F, 0x84, 0x22, 0x00, 0x00, 0x00)},

            // Filler instructions to jump past
            // Need 11 because 11 * MAX_OPCODE_LEN > 128, that's when it switches to 32 bit
            {RR8(XOR , REG, EAX, EAX, 0), WantOp(0x30, 0xC0)},
            {RR8(XOR , REG, EAX, EAX, 0), WantOp(0x30, 0xC0)},
            {RR8(XOR , REG, EAX, EAX, 0), WantOp(0x30, 0xC0)},
            {RR8(XOR , REG, EAX, EAX, 0), WantOp(0x30, 0xC0)},
            {RR8(XOR , REG, EAX, EAX, 0), WantOp(0x30, 0xC0)},
            {RR8(XOR , REG, EAX, EAX, 0), WantOp(0x30, 0xC0)},
            {RR8(XOR , REG, EAX, EAX, 0), WantOp(0x30, 0xC0)},
            {RR8(XOR , REG, EAX, EAX, 0), WantOp(0x30, 0xC0)},
            {RR8(XOR , REG, EAX, EAX, 0), WantOp(0x30, 0xC0)},
            {RR8(XOR , REG, EAX, EAX, 0), WantOp(0x30, 0xC0)},
            {RR8(XOR , REG, EAX, EAX, 0), WantOp(0x30, 0xC0)},

            {JD(JNE, /*instrIdx=*/0), WantOp(0x0F, 0x85, 0xD3, 0xFF, 0xFF, 0xFF)},
            {JD(JL , /*instrIdx=*/1), WantOp(0x0F, 0x8C, 0xD2, 0xFF, 0xFF, 0xFF)},
            {JD(JG , /*instrIdx=*/2), WantOp(0x0F, 0x8F, 0xD2, 0xFF, 0xFF, 0xFF)},
        };
        TestOpcodes(T, "OpJump32(..) - All 32 bit jumps with offset (plus some filler instructions)", 0, Cases, ArrayLength(Cases));
    }
    // Note the J(..) macro isn't tested because it is just JD but with 0 offset (for filling later)
}

void x86_opcode_RunTests(tester_state *T) {
    TestOpNoarg(T);
    TestOpReg(T);
    TestOpRegReg(T);
    TestOpRegImm(T);
    TestOpJump(T);
}
