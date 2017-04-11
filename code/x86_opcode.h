// Copyright (c) 2016-2017 Andrew Kallmeyer <fsmv@sapium.net>
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
    size_t ImmCount;
    uint8_t Immediate[4];
};

enum addressing_mode {
    REG = 0xC0,
    MEM = 0x00,
    MODE_NONE = 0xFF,
};

// no strings, it doesn't work with indexes because REG == 0xC0

enum reg {
    EAX = 0x00,
    ECX = 0x01,
    EDX = 0x02,
    EBX = 0x03,
    R_NONE = 0xF0,
};

const char *reg_strings[] = {
    "$EAX", "$ECX", "$EDX", "$EBX"
};

enum op {
    AND = 0,  // MR, I8, I32
    OR,       // MR, I8, I32
    XOR,      // MR, I8, I32

    INC,      // MR

    BT,       // I8
    CMP,      // MR, I8, I32

    MOV,      // MR, I8, I32

    RET,

    JMP = 0, JNC, JE, JNE, JL, JG
};

const char *op_strings[] = {
    "AND", "OR ", "XOR",
    "INC",
    "BT ", "CMP",
    "MOV",
    "RET",
};

const char *jmp_strings[] = {
    "JMP", "JNC", "JE ", "JNE", "JL ", "JG "
};

enum op_type {
    JUMP,
    ONE_REG,
    TWO_REG,
    REG_IMM,
    NOARG,
};

/*
CheckChar:

EpsilonArcs_[DisableState]: (short)
DotArcs_[DisableState]: (short)

RangeArcs_[AB]_[DisableState] (short)
MatchArcs_[A]_[DisableState] (short)

NotInRange_[AB] (short)

- switch statement
  Match_[A] (a long list of jumps to these first)
  Match_End (jumps to this are break)
*/

struct instruction {
    addressing_mode Mode;

    op Op;
    op_type Type;

    // TODO: We could do a union thing here to save like, a few bytes
    reg Dest;
    reg Src;
    bool Int32;
    uint32_t Imm;
    instruction *JumpDest;

    instruction(addressing_mode Mode, op Op, op_type Type, reg Dest, reg Src, bool Int32, uint32_t Imm)
        : Mode(Mode), Op(Op), Type(Type), Dest(Dest), Src(Src), Int32(Int32), Imm(Imm), JumpDest(0) {}
};

const uint8_t Op_MemReg[] =
{ 0x20, 0x08, 0x30,
  0xFE,
  0x00, 0x38,
  0x88,
  0xC3};
const uint16_t Op_Imm[] =
{ 0x0080, 0x0080, 0x0080,
  0x0000,
  0x0FBA, 0x0080,
  0x00C6,
  0x0000,};
const uint8_t Op_Extra[] = 
{ 0x04, 0x01, 0x06,
  0x00,
  0x04, 0x07,
  0x00,
  0x00};
const uint8_t  Op_Jmp8[] =  {   0xEB,   0x73,   0x74,   0x75,   0x7C,   0x7F};
const uint16_t Op_Jmp16[] = { 0x00E9, 0x0F83, 0x0F84, 0x0F85, 0x0F8C, 0x0F8F};

opcode_unpacked OpJump8(op Op, int8_t Offs) {
    opcode_unpacked Result = {};

    Result.Opcode[0] = Op_Jmp8[Op];
    Result.Immediate[0] = (uint8_t) Offs;
    Result.HasModRM = false;
    Result.ImmCount = 1;

    return Result;
}

opcode_unpacked OpJump32(op Op, int32_t Offs) {
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

opcode_unpacked OpNoarg(op Op) {
    Assert(Op == RET);

    opcode_unpacked Result = {};
    Result.Opcode[0] = Op_MemReg[Op];
    Result.HasModRM = false;

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

inline uint8_t *WriteNullTerm(uint8_t Array[], uint8_t *Dest) {
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

inline size_t CountNullTerm(uint8_t Array[]) {
    size_t Count = 0;
    for (;
         Count < ArrayLength(Array);
         ++Count)
    {
        if (!Array[Count]) {
            break;
        }
    }

    return Count;
}

size_t SizeOpcode(opcode_unpacked Opcode) {
    size_t Result = 0;
    Result += CountNullTerm(Opcode.Prefixes);
    Result += CountNullTerm(Opcode.Opcode);

    if (Opcode.HasModRM) {
        Result += 1;
    }

    if (Opcode.SIB) {
        Result += 1;
    }

    Result += CountNullTerm(Opcode.Displacement);

    Result += Opcode.ImmCount;

    return Result;
}

uint8_t *WriteOpcode(opcode_unpacked Opcode, uint8_t *Dest) {
    Dest = WriteNullTerm(Opcode.Prefixes, Dest);
    Dest = WriteNullTerm(Opcode.Opcode, Dest);

    if (Opcode.HasModRM) {
        *Dest++ = Opcode.ModRM;
    }

    if (Opcode.SIB) {
        *Dest++ = Opcode.SIB;
    }

    Dest = WriteNullTerm(Opcode.Displacement, Dest);

    for (size_t ImmIdx = 0; ImmIdx < Opcode.ImmCount; ++ImmIdx) {
        *Dest++ = Opcode.Immediate[ImmIdx];
    }

    return Dest;
}

// TODO Figure out what the actual max is, I think it's 6
#define MAX_OPCODE_LEN 10

#define INC8(mode, reg) (*(Instructions++) = instruction((mode), INC, ONE_REG, (reg), R_NONE, false, 0))
#define INC32(mode, reg) (*(Instructions++) = instruction((mode), INC, ONE_REG, (reg), R_NONE, true, 0))

#define RR8(op, mode, dest, src) (*(Instructions++) = instruction((mode), (op), TWO_REG, (dest), (src), false, 0))
#define RR32(op, mode, dest, src) (*(Instructions++) = instruction((mode), (op), TWO_REG, (dest), (src), true, 0))

#define RI8(op, mode, dest, imm) (*(Instructions++) = instruction((mode), (op), REG_IMM, (dest), R_NONE, false, (imm)))
#define RI32(op, mode, dest, imm) (*(Instructions++) = instruction((mode), (op), REG_IMM, (dest), R_NONE, true, (imm)))

#define J(op) Instructions; (*(Instructions++) = instruction(MODE_NONE, (op), JUMP, R_NONE, R_NONE, false, 0))

#define RET (*(Instructions++) = instruction(MODE_NONE, RET, NOARG, R_NONE, R_NONE, false, 0))

#endif
