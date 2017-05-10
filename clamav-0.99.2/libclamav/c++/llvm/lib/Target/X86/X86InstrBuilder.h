//===-- X86InstrBuilder.h - Functions to aid building x86 insts -*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file exposes functions that may be used with BuildMI from the
// MachineInstrBuilder.h file to handle X86'isms in a clean way.
//
// The BuildMem function may be used with the BuildMI function to add entire
// memory references in a single, typed, function call.  X86 memory references
// can be very complex expressions (described in the README), so wrapping them
// up behind an easier to use interface makes sense.  Descriptions of the
// functions are included below.
//
// For reference, the order of operands for memory references is:
// (Operand), Base, Scale, Index, Displacement.
//
//===----------------------------------------------------------------------===//

#ifndef X86INSTRBUILDER_H
#define X86INSTRBUILDER_H

#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineMemOperand.h"
#include "llvm/CodeGen/PseudoSourceValue.h"

namespace llvm {

/// X86AddressMode - This struct holds a generalized full x86 address mode.
/// The base register can be a frame index, which will eventually be replaced
/// with BP or SP and Disp being offsetted accordingly.  The displacement may
/// also include the offset of a global value.
struct X86AddressMode {
  enum {
    RegBase,
    FrameIndexBase
  } BaseType;

  union {
    unsigned Reg;
    int FrameIndex;
  } Base;

  unsigned Scale;
  unsigned IndexReg;
  int Disp;
  const GlobalValue *GV;
  unsigned GVOpFlags;

  X86AddressMode()
    : BaseType(RegBase), Scale(1), IndexReg(0), Disp(0), GV(0), GVOpFlags(0) {
    Base.Reg = 0;
  }
};

/// addDirectMem - This function is used to add a direct memory reference to the
/// current instruction -- that is, a dereference of an address in a register,
/// with no scale, index or displacement. An example is: DWORD PTR [EAX].
///
static inline const MachineInstrBuilder &
addDirectMem(const MachineInstrBuilder &MIB, unsigned Reg) {
  // Because memory references are always represented with five
  // values, this adds: Reg, 1, NoReg, 0, NoReg to the instruction.
  return MIB.addReg(Reg).addImm(1).addReg(0).addImm(0).addReg(0);
}


static inline const MachineInstrBuilder &
addOffset(const MachineInstrBuilder &MIB, int Offset) {
  return MIB.addImm(1).addReg(0).addImm(Offset).addReg(0);
}

/// addRegOffset - This function is used to add a memory reference of the form
/// [Reg + Offset], i.e., one with no scale or index, but with a
/// displacement. An example is: DWORD PTR [EAX + 4].
///
static inline const MachineInstrBuilder &
addRegOffset(const MachineInstrBuilder &MIB,
             unsigned Reg, bool isKill, int Offset) {
  return addOffset(MIB.addReg(Reg, getKillRegState(isKill)), Offset);
}

/// addRegReg - This function is used to add a memory reference of the form:
/// [Reg + Reg].
static inline const MachineInstrBuilder &addRegReg(const MachineInstrBuilder &MIB,
                                            unsigned Reg1, bool isKill1,
                                            unsigned Reg2, bool isKill2) {
  return MIB.addReg(Reg1, getKillRegState(isKill1)).addImm(1)
    .addReg(Reg2, getKillRegState(isKill2)).addImm(0).addReg(0);
}

static inline const MachineInstrBuilder &
addFullAddress(const MachineInstrBuilder &MIB,
               const X86AddressMode &AM) {
  assert(AM.Scale == 1 || AM.Scale == 2 || AM.Scale == 4 || AM.Scale == 8);
  
  if (AM.BaseType == X86AddressMode::RegBase)
    MIB.addReg(AM.Base.Reg);
  else if (AM.BaseType == X86AddressMode::FrameIndexBase)
    MIB.addFrameIndex(AM.Base.FrameIndex);
  else
    assert (0);
  MIB.addImm(AM.Scale).addReg(AM.IndexReg);
  if (AM.GV)
    MIB.addGlobalAddress(AM.GV, AM.Disp, AM.GVOpFlags);
  else
    MIB.addImm(AM.Disp);
    
  return MIB.addReg(0);
}

/// addFrameReference - This function is used to add a reference to the base of
/// an abstract object on the stack frame of the current function.  This
/// reference has base register as the FrameIndex offset until it is resolved.
/// This allows a constant offset to be specified as well...
///
static inline const MachineInstrBuilder &
addFrameReference(const MachineInstrBuilder &MIB, int FI, int Offset = 0) {
  MachineInstr *MI = MIB;
  MachineFunction &MF = *MI->getParent()->getParent();
  MachineFrameInfo &MFI = *MF.getFrameInfo();
  const TargetInstrDesc &TID = MI->getDesc();
  unsigned Flags = 0;
  if (TID.mayLoad())
    Flags |= MachineMemOperand::MOLoad;
  if (TID.mayStore())
    Flags |= MachineMemOperand::MOStore;
  MachineMemOperand *MMO =
    MF.getMachineMemOperand(PseudoSourceValue::getFixedStack(FI),
                            Flags, Offset,
                            MFI.getObjectSize(FI),
                            MFI.getObjectAlignment(FI));
  return addOffset(MIB.addFrameIndex(FI), Offset)
            .addMemOperand(MMO);
}

/// addConstantPoolReference - This function is used to add a reference to the
/// base of a constant value spilled to the per-function constant pool.  The
/// reference uses the abstract ConstantPoolIndex which is retained until
/// either machine code emission or assembly output. In PIC mode on x86-32,
/// the GlobalBaseReg parameter can be used to make this a
/// GlobalBaseReg-relative reference.
///
static inline const MachineInstrBuilder &
addConstantPoolReference(const MachineInstrBuilder &MIB, unsigned CPI,
                         unsigned GlobalBaseReg, unsigned char OpFlags) {
  //FIXME: factor this
  return MIB.addReg(GlobalBaseReg).addImm(1).addReg(0)
    .addConstantPoolIndex(CPI, 0, OpFlags).addReg(0);
}

} // End llvm namespace

#endif
