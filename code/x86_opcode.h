// Copyright (c) 2016 Andrew Kallmeyer <fsmv@sapium.net>
// Provided under the MIT License: https://mit-license.org

#ifndef X86_OPCODE_H
#define X86_OPCODE_H

struct opcode_unpacked {
    uint8_t Prefixes[4];
    uint8_t Opcode[3];//6 bits last twa are direction and operand lengnth
    bool HasModRM;
    uint8_t ModRM; // set operands: mod 2 bit, reg/opcode 3 bits, R/M 3 bits
    uint8_t SIB; // addressing modes: scale 2 bits, index 4 bits, base 3 bits
    uint8_t Displacement[4];
    int ImmCount;
    uint8_t Immediate[4];
};

enum addressing_mode {
    REG = 0xC0,
    MEM = 0x00,
};

enum reg {
    EAX = 0x00,
    ECX = 0x01,
    EDX = 0x02,
    EBX = 0x03
};

enum op {
    AND = 0,  // MR, I8, I32
    OR,       // MR, I8, I32
    XOR,      // MR, I8, I32

    INC,      // MR

    BT,       // I8
    CMP,      // MR, I8, I32

    MOV,      // MR, I8, I32
};

enum jop {
    JMP = 0, JNC, JE, JNE, JL, JG
};

const uint8_t Op_MemReg[] = 
{ 0x20, 0x08, 0x30,
  0xFE,
  0x00, 0x38,
  0x88};
const uint16_t Op_Imm[] =
{ 0x0080, 0x0080, 0x0080,
  0x0000,
  0x0FBA, 0x0080,
  0x00C6 };
const uint8_t Op_Extra[] = 
{ 0x04, 0x01, 0x06,
  0x00,
  0x04, 0x07,
  0x00 };
const uint8_t  Op_Jmp8[] =  {   0xEB,   0x73,   0x74,   0x75,   0x7C,   0x7F};
const uint16_t Op_Jmp16[] = { 0x00E9, 0x0F83, 0x0F84, 0x0F85, 0x0F8C, 0x0F8F};

opcode_unpacked OpJump8(jop Op, int8_t Offs) {
    opcode_unpacked Result = {};

    Result.Opcode[0] = Op_Jmp8[Op];
    Result.Immediate[0] = (uint8_t) Offs;
    Result.HasModRM = false;
    Result.ImmCount = 1;

    return Result;
}

opcode_unpacked OpJump32(jop Op, int32_t Offs) {
    opcode_unpacked Result = {};

    uint16_t Code = Op_Jmp16[Op];

    Result.Opcode[0] = (Code & 0xFF00) >> 8;
    Result.Opcode[1] = (Code & 0xFF);
    if (Result.Opcode[0] == 0) {
        Result.Opcode[0] = Result.Opcode[1];
        Result.Opcode[1] = 0;
    }

    uint32_t OffsUn = (uint32_t) Offs;

    Result.Immediate[0] = (uint8_t)  (OffsUn & 0xFF);
    Result.Immediate[1] = (uint8_t) ((OffsUn & 0xFF00) >> 8);
    Result.Immediate[2] = (uint8_t) ((OffsUn & 0xFF0000) >> 16);
    Result.Immediate[3] = (uint8_t) ((OffsUn & 0xFF000000) >> 24);

    Result.HasModRM = false;
    Result.ImmCount = 4;

    return Result;
}

opcode_unpacked OpReg(op Op, addressing_mode Mode, reg Reg, bool is16) {
    Assert(Op == INC);

    opcode_unpacked Result = {};

    Result.Opcode[0] = Op_MemReg[Op] + (is16 ? 1 : 0);

    Result.ModRM |= Mode;
    Result.ModRM |= Reg;

    Result.HasModRM = true;

    return Result;
}

opcode_unpacked OpRegReg(op Op, addressing_mode Mode, reg DestReg, reg SrcReg, bool is16) {
    Assert(Op == AND || Op == OR || Op == XOR || Op == CMP || Op == MOV);

    opcode_unpacked Result = {};

    Result.Opcode[0] = Op_MemReg[Op] + (is16 ? 1 : 0);

    Result.ModRM |= Mode;
    Result.ModRM |= SrcReg << 3;
    Result.ModRM |= DestReg;

    Result.HasModRM = true;

    return Result;
}

opcode_unpacked OpRegI8(op Op, addressing_mode Mode, reg DestReg, uint8_t Imm) {
    Assert(Op == BT || Op == AND || Op == OR || Op == XOR || Op == CMP || Op == MOV);

    opcode_unpacked Result = {};

    uint16_t Code = Op_Imm[Op];
    Result.Opcode[0] = (Code & 0xFF00) >> 8;
    Result.Opcode[1] = (Code & 0xFF);
    if (Result.Opcode[0] == 0) {
        Result.Opcode[0] = Result.Opcode[1];
        Result.Opcode[1] = 0;
    }

    Result.ModRM |= Mode;
    Result.ModRM |= (Op_Extra[Op] & 0x07) << 3;
    Result.ModRM |= DestReg;

    Result.Immediate[0] = Imm;

    Result.HasModRM = true;
    Result.ImmCount = 1;

    return Result;
}

opcode_unpacked OpRegI32(op Op, addressing_mode Mode, reg DestReg, uint32_t Imm) {
    Assert(Op == AND || Op == OR || Op == XOR || Op == CMP || Op == MOV);

    opcode_unpacked Result = {};

    uint16_t Code = Op_Imm[Op] + 1;
    Result.Opcode[0] = (Code & 0xFF00) >> 8;
    Result.Opcode[1] = (Code & 0xFF);
    if (Result.Opcode[0] == 0) {
        Result.Opcode[0] = Result.Opcode[1];
        Result.Opcode[1] = 0;
    }

    Result.ModRM |= Mode;
    Result.ModRM |= (Op_Extra[Op] & 0x07) << 3;
    Result.ModRM |= DestReg;

    Result.Immediate[0] = (uint8_t) (Imm & 0xFF);
    Result.Immediate[1] = (uint8_t) ((Imm & 0xFF00) >> 8);
    Result.Immediate[2] = (uint8_t) ((Imm & 0xFF0000) >> 16);
    Result.Immediate[3] = (uint8_t) ((Imm & 0xFF000000) >> 24);

    Result.HasModRM = true;
    Result.ImmCount = 4;

    return Result;
}

inline uint8_t *WriteUpToZero(uint8_t Array[], uint8_t *Dest) {
    for (size_t Idx = 0;
         Idx < ArrayLength(Array);
         ++Idx)
    {
        uint8_t Val = Array[Idx];

        if (!Val) {
            break;
        }

        *Dest++ = Val;
    }

    return Dest;
}

uint8_t *WriteOpcode(opcode_unpacked Opcode, uint8_t *Dest) {
    Dest = WriteUpToZero(Opcode.Prefixes, Dest);
    Dest = WriteUpToZero(Opcode.Opcode, Dest);

    if (Opcode.HasModRM) {
        *Dest++ = Opcode.ModRM;
    }

    if (Opcode.SIB) {
        *Dest++ = Opcode.SIB;
    }

    Dest = WriteUpToZero(Opcode.Displacement, Dest);

    for (int ImmIdx = 0; ImmIdx < Opcode.ImmCount; ++ImmIdx) {
        *Dest++ = Opcode.Immediate[ImmIdx];
    }

    return Dest;
}

#define J8(op, offs) Code = (WriteOpcode(OpJump8((op), (offs)), Code))
#define J32(op, offs) Code = (WriteOpcode(OpJump32((op), (offs)), Code))

#define INC8(mode, reg) Code = (WriteOpcode(OpReg(INC, (mode), (reg), false), Code))
#define INC32(mode, reg) Code = (WriteOpcode(OpReg(INC, (mode), (reg), true), Code))

#define RR8(op, mode, dest, src) Code = (WriteOpcode(OpRegReg((op), (mode), (dest), (src), false), Code))
#define RR32(op, mode, dest, src) Code = (WriteOpcode(OpRegReg((op), (mode), (dest), (src), true), Code))

#define RI8(op, mode, dest, imm) Code = (WriteOpcode(OpRegI8((op), (mode), (dest), (imm)), Code))
#define RI32(op, mode, dest, imm) Code = (WriteOpcode(OpRegI32((op), (mode), (dest), (imm)), Code))

#endif
