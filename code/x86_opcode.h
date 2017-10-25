// Copyright (c) 2016-2017 Andrew Kallmeyer <fsmv@sapium.net>
// Provided under the MIT License: https://mit-license.org

#ifndef X86_OPCODE_H_

// Primary Reference:
//
//   Intel® 64 and IA-32 Architectures Software Developer Manuals, Volume 2
//
// Which can be downloaded as a pdf for free at:
//
//   https://software.intel.com/en-us/articles/intel-sdm
//
// Bookmarks:
//  - Instruction format Description      Vol. 2A Chapter 2.1
//    - Format diagram                    Section 2.1, Figure 2-1
//    - Addressing Modes Constants Table  Section 2.1.5, Tables 2-1, 2-2, 2-3
//    - REX Prefixes are for 64bit mode   Section 2.2
//  - Instruction set constants and args  Chapters 3-5
//    - Understanding the tables          Section 3.1

struct opcode_unpacked {
    // uint8_t Prefixes[4]; // not used
    uint8_t Opcode[2]; // 6 bits last two are direction and operand length
    bool HasModRM;
    uint8_t ModRM; // set operands: mod 2 bit, reg/opcode 3 bits, R/M 3 bits
    uint8_t SIB; // addressing modes: scale 2 bits, index 4 bits, base 3 bits
    uint8_t DispCount;
    uint8_t Displacement[4];
    uint8_t ImmCount;
    uint8_t Immediate[4];
};

enum addressing_mode {
    // ModRM bits 7,6 (0 is LSB)
    MEM        = 0x00,
    MEM_DISP8  = 0x40,
    MEM_DISP32 = 0x80,
    REG        = 0xC0,
    MODE_NONE  = 0xFF,
};

// no strings (for addressing_mode), it doesn't work with indexes

enum reg {
    // ModRM bits 2,1,0 (dest operand) or 5,4,3 (src operand) (0 is LSB)
    // SIB bits 5,4,3, 2,1,0
    EAX    = 0x00, // Clobbered, Return value, Accumulator
    ECX    = 0x01, // Clobbered, Counter
    EDX    = 0x02, // Clobbered, Data
    EBX    = 0x03, // Callee save, Base
    // 
    ESP    = 0x04, // Stack Pointer
    EBP    = 0x05, // Callee Save, Base Pointer
    ESI    = 0x06, // Callee save, Source Index
    EDI    = 0x07, // Callee save, Destination Index
    R_NONE = 0xF0,
};

const char *reg_strings[] = {
    "EAX", "ECX", "EDX", "EBX", "ESP", "EBP", "ESI", "EDI"
};

// Instruction operation constants to use when using this lib to generate code
//
// Order matters, values are indices into constant arrays below.
//
// Comments key:
//  - MR        - Dest a is register or dereferenced register, Src is a register
//  - RM        - Op writes to Src and reads from Dest, Dest can be dereferenced
//  - RI8, RI32 - Register and 8 or 32 bit immediate
//  - M, R      - Register dereference or small register only instruction
//  - SRI32     - Small register only Dest plus 32 bit immediate Src
enum op {
    // Non-Jump ops
    // Values are indices into op_strings and opcode_* (except opcode_Jmp*)

    // Arithmetic
    AND = 0,  // MR, RI8, RI32
    OR,       // MR, RI8, RI32
    XOR,      // MR, RI8, RI32
    INC,      // M, R
    NOT,      // M, R
    // Comparison
    BT,       // RI8
    CMP,      // MR, RI8, RI32
    // Memory
    MOV,      // MR, SRI32, RI8, RI32
    MOVR,     // RM, Dest and Src are reversed
    PUSH,     // M, R (is16 == true only)
    POP,      // M, R (is16 == true only)
    // No args
    RET,

    // Jumps
    // Values are indices into jmp_strings and opcode_Jmp*

    // Jump enums start at zero again because the jump opcodes don't have the
    // same encoding options as the non-jump ops. See: opcode_Jmp8, opcode_jmp16
    JMP = 0, JNC, JE, JNE, JL, JG
};

const char *op_strings[] = {
    "AND ", "OR  ", "XOR ", "INC ", "NOT ",
    "BT  ", "CMP ",
    "MOV ", "MOVR", "PUSH", "POP ",
    "RET ",
};

// TODO: Combine jmp_strings with op_strings and make the enum not restart at 0
// for jumps. Just subtract JMP from the other jump opcodes in this file to keep
// the indexes for the separated opcode arrays the way they are.

// Strings separated because the jmp enums (in op) start at 0.
const char *jmp_strings[] = {
    "JMP ", "JNC ", "JE  ", "JNE ", "JL  ", "JG  "
};

enum op_type {
    JUMP,
    ONE_REG,
    TWO_REG,
    REG_IMM,
    NOARG,
};

struct instruction {
    addressing_mode Mode;

    op Op;
    op_type Type;

    // TODO: We could do a union thing here to save like, a few bytes
    reg Dest;
    reg Src;
    bool Is16;
    uint32_t Imm;
    int32_t Disp;
    size_t JumpDestIdx;
};

// Non jump opcode constants in order of the op enum
//
//  - These are the 8 bit opcodes, add 1 to get the 16/32 bit opcode
//  - Different arrays are for different argument types (addressing modes)
//    (reg = Register, mem = dereference register, imm = immediate (a constant))

// Opcodes for args (reg, reg/mem), (reg/mem)
const uint8_t opcode_MemReg[] =
{ 0x20, 0x08, 0x30, 0xFE, 0xF6,
  0x00, 0x38,
  0x88, 0x8A, 0xFE, 0x8E,
  0xC3};
// Opcodes for (reg/mem, imm), (imm)
const uint16_t opcode_Imm[] =
{ 0x0080, 0x0080, 0x0080, 0x0000, 0x0000,
  0x0FBA, 0x0080,
  0x00C6, 0x0000, 0x0000, 0x0000,
  0x0000};
// Used for (reg/mem, imm), (reg/mem) instructions.
// It's the "/digit" in the Opcode column in the intel manual
const uint8_t opcode_Extra[] = 
{ 0x04, 0x01, 0x06, 0x00, 0x02,
  0x04, 0x07,
  0x00, 0x00, 0x06, 0x00,
  0x00};
// Opcodes for encoding the register in the opcode to save a byte.
// Available for some (reg) and (reg, imm) instructions
//
// Add the register code to the opcode to encode register selection
const uint8_t opcode_ShortReg[] =
{ 0x00, 0x00, 0x00, 0x40, 0x00,
  0x00, 0x00,
  0xB8, 0x00, 0x50, 0x58,
  0x00};

// Has separate index space from the other arrays because they are encoded
// differently.
// The two arrays are: 8 bit offset, or 16/32 bit offset
const uint8_t  opcode_Jmp8[] =  {   0xEB,   0x73,   0x74,   0x75,   0x7C,   0x7F};
const uint16_t opcode_Jmp16[] = { 0x00E9, 0x0F83, 0x0F84, 0x0F85, 0x0F8C, 0x0F8F};

opcode_unpacked OpJump8(op Op, int8_t Offs) {
    opcode_unpacked Result = {};

    Result.Opcode[1] = opcode_Jmp8[Op];
    Result.Immediate[0] = (uint8_t) Offs;
    Result.HasModRM = false;
    Result.ImmCount = 1;

    return Result;
}

opcode_unpacked OpJump32(op Op, int32_t Offs) {
    opcode_unpacked Result = {};

    uint16_t Code = opcode_Jmp16[Op];
    Result.Opcode[0] = (uint8_t)((Code & 0xFF00) >> 8);
    Result.Opcode[1] = (uint8_t) (Code &   0xFF);

    Result.Immediate[0] = (uint8_t) (Offs &       0xFF);
    Result.Immediate[1] = (uint8_t)((Offs &     0xFF00) >>  8);
    Result.Immediate[2] = (uint8_t)((Offs &   0xFF0000) >> 16);
    Result.Immediate[3] = (uint8_t)((Offs & 0xFF000000) >> 24);
    Result.ImmCount = 4;
    Result.HasModRM = false;

    return Result;
}

opcode_unpacked OpNoarg(op Op) {
    Assert(Op == RET);

    opcode_unpacked Result = {};
    Result.Opcode[1] = opcode_MemReg[Op];
    Result.HasModRM = false;

    return Result;
}

opcode_unpacked OpReg(op Op, addressing_mode Mode, reg Reg, int32_t Displacement, bool is16) {
    Assert(Op == INC || Op == NOT || (Op == PUSH && is16) || (Op == POP && is16));
    opcode_unpacked Result = {};

    if (Mode == REG && opcode_ShortReg[Op] != 0) {
        Result.Opcode[1] = opcode_ShortReg[Op] + (uint8_t)Reg;
        Result.HasModRM = false;
    } else {
        if (Mode == REG && !is16) {
            Assert(Reg != ESI && Reg != EDI && Reg != EBP && Reg != ESP);
        }
        Result.Opcode[1] = opcode_MemReg[Op] + (is16 ? 1 : 0);

        Result.ModRM |= Mode;
        Result.ModRM |= (opcode_Extra[Op] & 0x07) << 3;
        Result.ModRM |= Reg;
        Result.HasModRM = true;

        if (Mode != REG && Reg == ESP) {
            Result.SIB = 0x24; // ESP with no scaled index register
        }
        if (Mode == MEM_DISP8) {
            Result.DispCount = 1;
            Result.Displacement[0] = (uint8_t) (Displacement & 0xFF);
        } else if (Mode == MEM_DISP32) {
            Result.DispCount = 4;
            Result.Displacement[0] = (uint8_t) (Displacement &       0xFF);
            Result.Displacement[1] = (uint8_t)((Displacement &     0xFF00) >>  8);
            Result.Displacement[2] = (uint8_t)((Displacement &   0xFF0000) >> 16);
            Result.Displacement[3] = (uint8_t)((Displacement & 0xFF000000) >> 24);
        }
    }

    return Result;
}

opcode_unpacked OpRegReg(op Op, addressing_mode Mode, reg DestReg, int32_t Displacement, reg SrcReg, bool is16) {
    Assert(Op == AND || Op == OR || Op == XOR || Op == CMP || Op == MOV || Op == MOVR);

    opcode_unpacked Result = {};

    if (!is16) {
        Assert(DestReg != ESI && DestReg != EDI && DestReg != EBP && DestReg != ESP);
        if (Mode != REG) {
            Assert(SrcReg != ESI && SrcReg != EDI && SrcReg != EBP && SrcReg != ESP);
        }
    }

    Result.Opcode[1] = opcode_MemReg[Op] + (is16 ? 1 : 0);

    Result.ModRM |= Mode;
    Result.ModRM |= SrcReg << 3;
    Result.ModRM |= DestReg;
    Result.HasModRM = true;

    if (Mode != REG && DestReg == ESP) {
        Result.SIB = 0x24; // ESP with no scaled index register
    }
    if (Mode == MEM_DISP8) {
        Result.DispCount = 1;
        Result.Displacement[0] = (uint8_t) (Displacement & 0xFF);
    } else if (Mode == MEM_DISP32) {
        Result.DispCount = 4;
        Result.Displacement[0] = (uint8_t) (Displacement &       0xFF);
        Result.Displacement[1] = (uint8_t)((Displacement &     0xFF00) >>  8);
        Result.Displacement[2] = (uint8_t)((Displacement &   0xFF0000) >> 16);
        Result.Displacement[3] = (uint8_t)((Displacement & 0xFF000000) >> 24);
    }

    return Result;
}

opcode_unpacked OpRegI8(op Op, addressing_mode Mode, reg DestReg, int32_t Displacement, uint8_t Imm) {
    Assert(Op == BT || Op == AND || Op == OR || Op == XOR || Op == CMP || Op == MOV);
    opcode_unpacked Result = {};

    uint16_t Code = opcode_Imm[Op];
    Result.Opcode[0] = (uint8_t)((Code & 0xFF00) >> 8);
    Result.Opcode[1] = (uint8_t) (Code &   0xFF);

    Result.ModRM |= Mode;
    Result.ModRM |= (opcode_Extra[Op] & 0x07) << 3;
    Result.ModRM |= DestReg;
    Result.HasModRM = true;

    if (Mode != REG && DestReg == ESP) {
        Result.SIB = 0x24; // ESP with no scaled index register
    }
    if (Mode == MEM_DISP8) {
        Result.DispCount = 1;
        Result.Displacement[0] = (uint8_t) (Displacement & 0xFF);
    } else if (Mode == MEM_DISP32) {
        Result.DispCount = 4;
        Result.Displacement[0] = (uint8_t) (Displacement &       0xFF);
        Result.Displacement[1] = (uint8_t)((Displacement &     0xFF00) >>  8);
        Result.Displacement[2] = (uint8_t)((Displacement &   0xFF0000) >> 16);
        Result.Displacement[3] = (uint8_t)((Displacement & 0xFF000000) >> 24);
    }

    Result.Immediate[0] = Imm;
    Result.ImmCount = 1;

    return Result;
}

opcode_unpacked OpRegI32(op Op, addressing_mode Mode, reg DestReg, int32_t Displacement, uint32_t Imm) {
    Assert(Op == AND || Op == OR || Op == XOR || Op == CMP || Op == MOV);
    opcode_unpacked Result = {};

    if (Mode == REG && opcode_ShortReg[Op] != 0) {
        Result.Opcode[1] = opcode_ShortReg[Op] + (uint8_t)DestReg;
        Result.HasModRM = false;
    } else {
        uint16_t Code = opcode_Imm[Op] + 1;
        Result.Opcode[0] = (uint8_t)((Code & 0xFF00) >> 8);
        Result.Opcode[1] = (uint8_t) (Code &   0xFF);

        Result.ModRM |= Mode;
        Result.ModRM |= (opcode_Extra[Op] & 0x07) << 3;
        Result.ModRM |= DestReg;
        Result.HasModRM = true;

        if (Mode != REG && DestReg == ESP) {
            Result.SIB = 0x24; // ESP with no scaled index register
        }
        if (Mode == MEM_DISP8) {
            Result.DispCount = 1;
            Result.Displacement[0] = (uint8_t) (Displacement & 0xFF);
        } else if (Mode == MEM_DISP32) {
            Result.DispCount = 4;
            Result.Displacement[0] = (uint8_t) (Displacement &       0xFF);
            Result.Displacement[1] = (uint8_t)((Displacement &     0xFF00) >>  8);
            Result.Displacement[2] = (uint8_t)((Displacement &   0xFF0000) >> 16);
            Result.Displacement[3] = (uint8_t)((Displacement & 0xFF000000) >> 24);
        }
    }
    Result.Immediate[0] = (uint8_t) (Imm &       0xFF);
    Result.Immediate[1] = (uint8_t)((Imm &     0xFF00) >>  8);
    Result.Immediate[2] = (uint8_t)((Imm &   0xFF0000) >> 16);
    Result.Immediate[3] = (uint8_t)((Imm & 0xFF000000) >> 24);
    Result.ImmCount = 4;

    return Result;
}

size_t SizeOpcode(opcode_unpacked Opcode) {
    size_t Result = 0;
    Result += (Opcode.Opcode[0] == 0) ? 1 : 2;

    if (Opcode.HasModRM) {
        Result += 1;
    }

    if (Opcode.SIB) {
        Result += 1;
    }

    Result += Opcode.DispCount;
    Result += Opcode.ImmCount;

    return Result;
}


int32_t ComputeJumpOffset(opcode_unpacked *UnpackedOpcodes, size_t JumpIdx, size_t JumpDestIdx) {
    size_t Start = 0, End = 0;
    if (JumpDestIdx < JumpIdx) { // backwards
        Start = JumpDestIdx;
        End = JumpIdx + 1;
    } else { // forwards (or equal)
        Start = JumpIdx + 1;
        End = JumpDestIdx;
    }

    size_t JumpOffset = 0;
    for (size_t Idx = Start; Idx < End; ++Idx) {
        JumpOffset += SizeOpcode(UnpackedOpcodes[Idx]);
    }

    int32_t Result = (int32_t) JumpOffset;
    if (JumpDestIdx < JumpIdx) { // backwards
        Result *= -1;
    }
    return Result;
}

// TODO Figure out what the actual max is, I think it's 6
#define MAX_OPCODE_LEN 10

void AssembleInstructions(instruction *Instructions, size_t NumInstructions, opcode_unpacked *UnpackedOpcodes) {
    opcode_unpacked *NextOpcode = UnpackedOpcodes;
    for (size_t Idx = 0; Idx < NumInstructions; ++Idx) {
        instruction *Inst = &Instructions[Idx];

        switch(Inst->Type) {
            case JUMP:
            {
                int32_t JumpDist = (int32_t) (Inst->JumpDestIdx - Idx);
                // We don't know how big the ops between here and the dest are yet
                int32_t JumpOffsetUpperBound = JumpDist * MAX_OPCODE_LEN;

                if (JumpOffsetUpperBound < -128 || JumpOffsetUpperBound > 127) {
                    *(NextOpcode++) = OpJump32(Inst->Op, 0);
                } else {
                    *(NextOpcode++) = OpJump8(Inst->Op, 0);
                }
            }break;
            case ONE_REG:
                *(NextOpcode++) = OpReg(Inst->Op, Inst->Mode, Inst->Dest, Inst->Disp, Inst->Is16);
                break;
            case TWO_REG:
                *(NextOpcode++) = OpRegReg(Inst->Op, Inst->Mode, Inst->Dest, Inst->Disp, Inst->Src, Inst->Is16);
                break;
            case REG_IMM:
                if (Inst->Is16) {
                    *(NextOpcode++) = OpRegI32(Inst->Op, Inst->Mode, Inst->Dest, Inst->Disp, Inst->Imm);
                } else {
                    *(NextOpcode++) = OpRegI8(Inst->Op, Inst->Mode, Inst->Dest, Inst->Disp, (uint8_t) Inst->Imm);
                }
                break;
            case NOARG:
                *(NextOpcode++) = OpNoarg(Inst->Op);
                break;
        }
    }

    for (size_t Idx = 0; Idx < NumInstructions; ++Idx) {
        instruction *Inst = &Instructions[Idx];
        if (Inst->Type == JUMP) {
            int32_t JumpOffset = ComputeJumpOffset(UnpackedOpcodes, Idx, Inst->JumpDestIdx);

            if (UnpackedOpcodes[Idx].ImmCount == 1) {
                Assert(-128 <= JumpOffset && JumpOffset <= 127);
                UnpackedOpcodes[Idx] = OpJump8(Inst->Op, (int8_t) JumpOffset);
            } else if (UnpackedOpcodes[Idx].ImmCount == 4) {
                UnpackedOpcodes[Idx] = OpJump32(Inst->Op, JumpOffset);
            }
        }
    }
}

inline uint8_t Copy32(uint8_t *Dest, uint8_t *Src, uint8_t Count) {
    Assert(Count <= 4);

    switch(Count) {
    case 4:
        Dest[3] = Src[3];
    case 3:
        Dest[2] = Src[2];
    case 2:
        Dest[1] = Src[1];
    case 1:
        Dest[0] = Src[0];
    case 0:
        ;
    }

    return Count;
}

size_t PackCode(opcode_unpacked *Opcodes, size_t NumOpcodes, uint8_t *Dest) {
    uint8_t *DestStart = Dest;
    for (size_t Idx = 0; Idx < NumOpcodes; ++Idx, ++Opcodes) {
        if (Opcodes->Opcode[0] == 0) { // size is 1
            Dest += Copy32(Dest, Opcodes->Opcode + 1, 1);
        } else { // size is 2
            Dest += Copy32(Dest, Opcodes->Opcode, 2);
        }

        if (Opcodes->HasModRM) {
            *Dest++ = Opcodes->ModRM;
        }

        if (Opcodes->SIB) {
            *Dest++ = Opcodes->SIB;
        }

        Dest += Copy32(Dest, Opcodes->Displacement, Opcodes->DispCount);
        Dest += Copy32(Dest, Opcodes->Immediate, Opcodes->ImmCount);
    }
    return (size_t)(Dest - DestStart);
}

#define R8(op, mode, reg, disp) (instruction{(mode), (op), ONE_REG, (reg),  R_NONE, false, 0, (int32_t)(disp), 0})
#define R32(op, mode, reg, disp) (instruction{(mode), (op), ONE_REG, (reg), R_NONE, true, 0, (int32_t)(disp), 0})

#define RR8(op, mode, dest, src, disp) (instruction{(mode), (op), TWO_REG, (dest), (src), false, 0, (int32_t)(disp), 0})
#define RR32(op, mode, dest, src, disp) (instruction{(mode), (op), TWO_REG, (dest), (src), true, 0, (int32_t)(disp), 0})

#define RI8(op, mode, dest, disp, imm) (instruction{(mode), (op), REG_IMM, (dest), R_NONE, false, (uint32_t)(imm), (int32_t)(disp), 0})
#define RI32(op, mode, dest, disp, imm) (instruction{(mode), (op), REG_IMM, (dest), R_NONE, true, (imm), (int32_t)(disp), 0})
#define J(op) (instruction{MODE_NONE, (op), JUMP, R_NONE, R_NONE, false, 0, 0, 0})
#define JD(op, dest) (instruction{MODE_NONE, (op), JUMP, R_NONE, R_NONE, false, 0, 0, (dest)})

#define RET (instruction{MODE_NONE, RET, NOARG, R_NONE, R_NONE, false, 0, 0, 0})

#define X86_OPCODE_H_
#endif
