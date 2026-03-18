#ifndef INSTRUCTION_DESCRIPTIONS_H
#define INSTRUCTION_DESCRIPTIONS_H

#include <string>
#include <map>

inline std::map<std::string, std::string> GetInstructionDetails() {
    std::map<std::string, std::string> details;

    details["ADC"] = "Add with Carry\n\nOperations: A + M + C -> A, C\n\nAdds the contents of a memory location to the accumulator together with the carry bit. If overflow occurs the carry flag is set, this enables multiple-precision addition.";
    details["AND"] = "Logical AND\n\nOperations: A AND M -> A\n\nA logical AND is performed, bit by bit, on the accumulator contents using the contents of a byte of memory.";
    details["ASL"] = "Arithmetic Shift Left\n\nOperations: C <- [76543210] <- 0\n\nThis operation shifts all the bits of the accumulator or memory contents one bit left. Bit 0 is set to 0 and bit 7 is placed in the carry flag.";
    details["BCC"] = "Branch if Carry Clear\n\nOperations: If C = 0, PC = address\n\nIf the carry flag is clear then add the relative displacement to the program counter to cause a branch to a new location.";
    details["BCS"] = "Branch if Carry Set\n\nOperations: If C = 1, PC = address\n\nIf the carry flag is set then add the relative displacement to the program counter to cause a branch to a new location.";
    details["BEQ"] = "Branch if Equal\n\nOperations: If Z = 1, PC = address\n\nIf the zero flag is set then add the relative displacement to the program counter to cause a branch to a new location.";
    details["BIT"] = "Bit Test\n\nOperations: A AND M, M7 -> N, M6 -> V\n\nThis instructions is used to test if one or more bits are set in a target memory location. The mask is in the accumulator. The N and V flags are set from bits 7 and 6 of the memory value.";
    details["BMI"] = "Branch if Minus\n\nOperations: If N = 1, PC = address\n\nIf the negative flag is set then add the relative displacement to the program counter to cause a branch to a new location.";
    details["BNE"] = "Branch if Not Equal\n\nOperations: If Z = 0, PC = address\n\nIf the zero flag is clear then add the relative displacement to the program counter to cause a branch to a new location.";
    details["BPL"] = "Branch if Plus\n\nOperations: If N = 0, PC = address\n\nIf the negative flag is clear then add the relative displacement to the program counter to cause a branch to a new location.";
    details["BRK"] = "Force Break\n\nOperations: Interrupt, PC + 2 -> Stack, P -> Stack\n\nThe BRK instruction forces the generation of an interrupt request. The program counter and processor status are pushed on the stack then the IRQ interrupt vector at $FFFE/F is loaded into the PC.";
    details["BVC"] = "Branch if Overflow Clear\n\nOperations: If V = 0, PC = address\n\nIf the overflow flag is clear then add the relative displacement to the program counter to cause a branch to a new location.";
    details["BVS"] = "Branch if Overflow Set\n\nOperations: If V = 1, PC = address\n\nIf the overflow flag is set then add the relative displacement to the program counter to cause a branch to a new location.";
    details["CLC"] = "Clear Carry Flag\n\nOperations: 0 -> C\n\nSets the carry flag to zero.";
    details["CLD"] = "Clear Decimal Mode\n\nOperations: 0 -> D\n\nSets the decimal mode flag to zero.";
    details["CLI"] = "Clear Interrupt Disable\n\nOperations: 0 -> I\n\nClears the interrupt disable flag allowing normal interrupt requests to be serviced.";
    details["CLV"] = "Clear Overflow Flag\n\nOperations: 0 -> V\n\nClears the overflow flag.";
    details["CMP"] = "Compare\n\nOperations: A - M\n\nThis instruction compares the contents of the accumulator with another memory held value and sets the zero and carry flags as appropriate.";
    details["CPX"] = "Compare X Register\n\nOperations: X - M\n\nThis instruction compares the contents of the X register with another memory held value and sets the zero and carry flags as appropriate.";
    details["CPY"] = "Compare Y Register\n\nOperations: Y - M\n\nThis instruction compares the contents of the Y register with another memory held value and sets the zero and carry flags as appropriate.";
    details["DEC"] = "Decrement Memory\n\nOperations: M - 1 -> M\n\nSubtracts one from the value held at a specified memory location setting the zero and negative flags as appropriate.";
    details["DEX"] = "Decrement X Register\n\nOperations: X - 1 -> X\n\nSubtracts one from the X register setting the zero and negative flags as appropriate.";
    details["DEY"] = "Decrement Y Register\n\nOperations: Y - 1 -> Y\n\nSubtracts one from the Y register setting the zero and negative flags as appropriate.";
    details["EOR"] = "Exclusive OR\n\nOperations: A EOR M -> A\n\nAn exclusive OR is performed, bit by bit, on the accumulator contents using the contents of a byte of memory.";
    details["INC"] = "Increment Memory\n\nOperations: M + 1 -> M\n\nAdds one to the value held at a specified memory location setting the zero and negative flags as appropriate.";
    details["INX"] = "Increment X Register\n\nOperations: X + 1 -> X\n\nAdds one to the X register setting the zero and negative flags as appropriate.";
    details["INY"] = "Increment Y Register\n\nOperations: Y + 1 -> Y\n\nAdds one to the Y register setting the zero and negative flags as appropriate.";
    details["JMP"] = "Jump\n\nOperations: address -> PC\n\nSets the program counter to the address specified by the operand.";
    details["JSR"] = "Jump to Subroutine\n\nOperations: PC + 2 -> Stack, address -> PC\n\nThe JSR instruction pushes the address (minus one) of the return point on to the stack and then sets the program counter to the target memory address.";
    details["LDA"] = "Load Accumulator\n\nOperations: M -> A\n\nLoads a byte of memory into the accumulator setting the zero and negative flags as appropriate.";
    details["LDX"] = "Load X Register\n\nOperations: M -> X\n\nLoads a byte of memory into the X register setting the zero and negative flags as appropriate.";
    details["LDY"] = "Load Y Register\n\nOperations: M -> Y\n\nLoads a byte of memory into the Y register setting the zero and negative flags as appropriate.";
    details["LSR"] = "Logical Shift Right\n\nOperations: 0 -> [76543210] -> C\n\nEach of the bits in choose accumulator or memory is shifted one place to the right. Bit 0 is filled with 0 and bit 7 is placed in the carry flag.";
    details["NOP"] = "No Operation\n\nOperations: None\n\nThe NOP instruction causes no changes to the processor other than the normal incrementing of the program counter.";
    details["ORA"] = "Logical Inclusive OR\n\nOperations: A OR M -> A\n\nAn inclusive OR is performed, bit by bit, on the accumulator contents using the contents of a byte of memory.";
    details["PHA"] = "Push Accumulator on Stack\n\nOperations: A -> Stack\n\nPushes a copy of the accumulator on to the stack.";
    details["PHP"] = "Push Processor Status on Stack\n\nOperations: P -> Stack\n\nPushes a copy of the processor status register (with the break flag set) on to the stack.";
    details["PLA"] = "Pull Accumulator from Stack\n\nOperations: Stack -> A\n\nPulls an 8 bit value from the stack and into the accumulator. The zero and negative flags are set as appropriate.";
    details["PLP"] = "Pull Processor Status from Stack\n\nOperations: Stack -> P\n\nPulls an 8 bit value from the stack and into the processor status register. The flags will take on new states as determined by the value pulled.";
    details["ROL"] = "Rotate Left\n\nOperations: C <- [76543210] <- C\n\nMove each of the bits in either the accumulator or memory one place to the left. Bit 0 is filled with the current value of the carry flag whilst the old bit 7 becomes the new carry flag value.";
    details["ROR"] = "Rotate Right\n\nOperations: C -> [76543210] <- C\n\nMove each of the bits in either the accumulator or memory one place to the right. Bit 7 is filled with the current value of the carry flag whilst the old bit 0 becomes the new carry flag value.";
    details["RTI"] = "Return from Interrupt\n\nOperations: Stack -> P, Stack -> PC\n\nThe RTI instruction is used at the end of an interrupt processing routine. It pulls the processor flags from the stack followed by the program counter.";
    details["RTS"] = "Return from Subroutine\n\nOperations: Stack -> PC, PC + 1 -> PC\n\nThe RTS instruction is used at the end of a subroutine. It pulls the program counter (minus one) from the stack.";
    details["SBC"] = "Subtract with Carry\n\nOperations: A - M - C -> A\n\nThis instruction subtracts the contents of a memory location to the accumulator together with the not of the carry bit. If overflow occurs the carry flag is clear, this enables multiple-precision subtraction.";
    details["SEC"] = "Set Carry Flag\n\nOperations: 1 -> C\n\nSets the carry flag to one.";
    details["SED"] = "Set Decimal Flag\n\nOperations: 1 -> D\n\nSets the decimal mode flag to one.";
    details["SEI"] = "Set Interrupt Disable\n\nOperations: 1 -> I\n\nSets the interrupt disable flag to one. New interrupts will be ignored until the flag is cleared.";
    details["STA"] = "Store Accumulator\n\nOperations: A -> M\n\nStores the contents of the accumulator into memory.";
    details["STX"] = "Store X Register\n\nOperations: X -> M\n\nStores the contents of the X register into memory.";
    details["STY"] = "Store Y Register\n\nOperations: Y -> M\n\nStores the contents of the Y register into memory.";
    details["TAX"] = "Transfer Accumulator to X\n\nOperations: A -> X\n\nCopies the current contents of the accumulator into the X register and sets the zero and negative flags as appropriate.";
    details["TAY"] = "Transfer Accumulator to Y\n\nOperations: A -> Y\n\nCopies the current contents of the accumulator into the Y register and sets the zero and negative flags as appropriate.";
    details["TSX"] = "Transfer Stack Pointer to X\n\nOperations: S -> X\n\nCopies the current contents of the stack register into the X register and sets the zero and negative flags as appropriate.";
    details["TXA"] = "Transfer X to Accumulator\n\nOperations: X -> A\n\nCopies the current contents of the X register into the accumulator and sets the zero and negative flags as appropriate.";
    details["TXS"] = "Transfer X to Stack Pointer\n\nOperations: X -> S\n\nCopies the current contents of the X register into the stack register.";
    details["TYA"] = "Transfer Y to Accumulator\n\nOperations: Y -> A\n\nCopies the current contents of the Y register into the accumulator and sets the zero and negative flags as appropriate.";

    // 65C02 / 65CE02 / 45GS02
    details["BRA"] = "Branch Always\n\nOperations: PC = address\n\nAdds the relative displacement to the program counter to cause a branch to a new location.";
    details["PHX"] = "Push X on Stack\n\nOperations: X -> Stack\n\nPushes a copy of the X register on to the stack.";
    details["PLX"] = "Pull X from Stack\n\nOperations: Stack -> X\n\nPulls an 8 bit value from the stack and into the X register.";
    details["PHY"] = "Push Y on Stack\n\nOperations: Y -> Stack\n\nPushes a copy of the Y register on to the stack.";
    details["PLY"] = "Pull Y from Stack\n\nOperations: Stack -> Y\n\nPulls an 8 bit value from the stack and into the Y register.";
    details["STZ"] = "Store Zero to Memory\n\nOperations: 0 -> M\n\nWrites the value zero to a specified memory location.";
    details["TRB"] = "Test and Reset Bits\n\nOperations: NOT A AND M -> Z, M AND NOT A -> M\n\nTests bits in memory using the accumulator as a mask. The Zero flag is set if (NOT A AND M) is zero. Then, bits in memory are cleared where the corresponding bit in the accumulator is set.";
    details["TSB"] = "Test and Set Bits\n\nOperations: A AND M -> Z, M OR A -> M\n\nTests bits in memory using the accumulator as a mask. The Zero flag is set if (A AND M) is zero. Then, bits in memory are set where the corresponding bit in the accumulator is set.";
    details["BBR"] = "Branch if Bit Reset\n\nOperations: If Bit n of M = 0, PC = address\n\nTests a specific bit (0-7) of a memory location. If the bit is clear, it branches to the target address.";
    details["BBS"] = "Branch if Bit Set\n\nOperations: If Bit n of M = 1, PC = address\n\nTests a specific bit (0-7) of a memory location. If the bit is set, it branches to the target address.";
    details["RMB"] = "Reset Memory Bit\n\nOperations: M AND NOT (1 << n) -> M\n\nClears a specific bit (0-7) of a memory location.";
    details["SMB"] = "Set Memory Bit\n\nOperations: M OR (1 << n) -> M\n\nSets a specific bit (0-7) of a memory location.";

    details["LDZ"] = "Load Z Register\n\nOperations: M -> Z\n\nLoads a byte of memory into the Z register setting the zero and negative flags as appropriate.";
    details["TAZ"] = "Transfer Accumulator to Z\n\nOperations: A -> Z\n\nCopies the current contents of the accumulator into the Z register.";
    details["TZA"] = "Transfer Z to Accumulator\n\nOperations: Z -> A\n\nCopies the current contents of the Z register into the accumulator.";
    details["INZ"] = "Increment Z Register\n\nOperations: Z + 1 -> Z\n\nAdds one to the Z register setting the zero and negative flags as appropriate.";
    details["DEZ"] = "Decrement Z Register\n\nOperations: Z - 1 -> Z\n\nSubtracts one from the Z register setting the zero and negative flags as appropriate.";
    
    // 45GS02 / 65CE02 specific
    details["CLE"] = "Clear E Flag\n\nOperations: 0 -> E\n\nSets the emulation flag to 0. On the 45GS02/65CE02, this enables 6502 compatibility mode (8-bit stack pointer).";
    details["SEE"] = "Set E Flag\n\nOperations: 1 -> E\n\nSets the emulation flag to 1. On the 45GS02/65CE02, this enables native mode (16-bit stack pointer).";
    details["NEG"] = "Negate Accumulator\n\nOperations: 0 - A -> A\n\nPerforms a two's complement negation of the accumulator contents.";
    details["ASW"] = "Arithmetic Shift Word\n\nOperations: C <- [Word] <- 0\n\nShifts a 16-bit word in memory one bit to the left. Bit 0 is set to 0 and bit 15 is placed in the carry flag.";
    details["INW"] = "Increment Word\n\nOperations: M16 + 1 -> M16\n\nAdds one to a 16-bit word in memory.";
    details["DEW"] = "Decrement Word\n\nOperations: M16 - 1 -> M16\n\nSubtracts one from a 16-bit word in memory.";
    details["BSR"] = "Branch to Subroutine\n\nOperations: PC + 2 -> Stack, PC = address\n\nPushes the return address onto the stack and branches to the target address using relative displacement.";
    details["RTN"] = "Return from Subroutine\n\nOperations: Pull PC, SP += n\n\nReturns from a subroutine and optionally adjusts the stack pointer by a specified number of bytes.";
    details["MAP"] = "Map Memory\n\nOperations: Configure Memory Mapping\n\nUsed on the 45GS02 to configure the memory controller and mapping of the 28-bit address space.";
    details["EOM"] = "End of Map / Prefix\n\nOperations: End of Mapping operation\n\nA prefix or standalone instruction used on the 45GS02 for extended addressing and mapping control.";
    details["PHZ"] = "Push Z on Stack\n\nOperations: Z -> Stack\n\nPushes a copy of the Z register on to the stack.";
    details["PLZ"] = "Pull Z from Stack\n\nOperations: Stack -> Z\n\nPulls an 8 bit value from the stack and into the Z register.";
    details["TAB"] = "Transfer Accumulator to B\n\nOperations: A -> B\n\nCopies the accumulator to the B (Base) register.";
    details["TBA"] = "Transfer B to Accumulator\n\nOperations: B -> A\n\nCopies the B (Base) register to the accumulator.";
    details["TSY"] = "Transfer Stack Pointer to Y\n\nOperations: S -> Y\n\nCopies the stack pointer to the Y register.";
    details["TYS"] = "Transfer Y to Stack Pointer\n\nOperations: Y -> S\n\nCopies the Y register to the stack pointer.";

    details["ASR"] = "Arithmetic Shift Right\n\nOperations: [7] -> [76543210] -> C\n\nShifts the accumulator or memory contents one bit to the right. Bit 7 is unchanged (preserving the sign) and bit 0 is placed in the carry flag.";
    details["BRL"] = "Branch Relative Long\n\nOperations: PC = address\n\nAdds a 16-bit displacement to the program counter to branch to a target within +/- 32KB.";
    details["LBRA"] = "Long Branch Always\n\nOperations: PC = address\n\nAdds a 16-bit displacement to the program counter to cause an unconditional branch to a new location.";
    details["PHW"] = "Push Word on Stack\n\nOperations: M16 -> Stack\n\nPushes a 16-bit word onto the stack.";
    details["ROW"] = "Rotate Word Right\n\nOperations: C -> [Word] -> C\n\nRotates a 16-bit word in memory one bit to the right through the carry flag.";
    details["DEA"] = "Decrement Accumulator\n\nOperations: A - 1 -> A\n\nSubtracts one from the accumulator.";
    details["INA"] = "Increment Accumulator\n\nOperations: A + 1 -> A\n\nAdds one to the accumulator.";

    // Long Branches
    details["LBCC"] = "Long Branch if Carry Clear\n\nOperations: If C = 0, PC = address\n\nIf the carry flag is clear then add the 16-bit relative displacement to the program counter to cause a branch to a new location.";
    details["LBCS"] = "Long Branch if Carry Set\n\nOperations: If C = 1, PC = address\n\nIf the carry flag is set then add the 16-bit relative displacement to the program counter to cause a branch to a new location.";
    details["LBEQ"] = "Long Branch if Equal\n\nOperations: If Z = 1, PC = address\n\nIf the zero flag is set then add the 16-bit relative displacement to the program counter to cause a branch to a new location.";
    details["LBMI"] = "Long Branch if Minus\n\nOperations: If N = 1, PC = address\n\nIf the negative flag is set then add the 16-bit relative displacement to the program counter to cause a branch to a new location.";
    details["LBNE"] = "Long Branch if Not Equal\n\nOperations: If Z = 0, PC = address\n\nIf the zero flag is clear then add the 16-bit relative displacement to the program counter to cause a branch to a new location.";
    details["LBPL"] = "Long Branch if Plus\n\nOperations: If N = 0, PC = address\n\nIf the negative flag is clear then add the 16-bit relative displacement to the program counter to cause a branch to a new location.";
    details["LBVC"] = "Long Branch if Overflow Clear\n\nOperations: If V = 0, PC = address\n\nIf the overflow flag is clear then add the 16-bit relative displacement to the program counter to cause a branch to a new location.";
    details["LBVS"] = "Long Branch if Overflow Set\n\nOperations: If V = 1, PC = address\n\nIf the overflow flag is set then add the 16-bit relative displacement to the program counter to cause a branch to a new location.";
    details["LBSR"] = "Long Branch to Subroutine\n\nOperations: PC + 2 -> Stack, PC = address\n\nPushes the return address onto the stack and branches to the target address using a 16-bit relative displacement.";

    // Quad Instructions (32-bit)
    details["ADCQ"] = "Add Quad with Carry\n\nOperations: Q + M32 + C -> Q, C\n\nAdds the contents of four consecutive memory locations to the quad register (A:X:Y:Z) together with the carry bit.";
    details["ANDQ"] = "Logical AND Quad\n\nOperations: Q AND M32 -> Q\n\nA logical AND is performed, bit by bit, on the 32-bit quad register using the contents of four bytes of memory.";
    details["ASLQ"] = "Arithmetic Shift Left Quad\n\nOperations: C <- [Quad] <- 0\n\nShifts all 32 bits of the quad register or 32-bit memory contents one bit left.";
    details["BITQ"] = "Bit Test Quad\n\nOperations: Q AND M32, M31 -> N, M30 -> V\n\nPerforms a bitwise AND between the quad register and 32-bit memory to set the zero flag, and updates N and V from the high bits of memory.";
    details["CPQ"] = "Compare Quad\n\nOperations: Q - M32\n\nCompares the 32-bit quad register with four bytes of memory and sets flags accordingly.";
    details["DEQ"] = "Decrement Quad\n\nOperations: Q - 1 -> Q\n\nSubtracts one from the 32-bit quad register.";
    details["EORQ"] = "Exclusive OR Quad\n\nOperations: Q EOR M32 -> Q\n\nPerforms a bitwise exclusive OR between the 32-bit quad register and memory.";
    details["INQ"] = "Increment Quad\n\nOperations: Q + 1 -> Q\n\nAdds one to the 32-bit quad register.";
    details["LDQ"] = "Load Quad\n\nOperations: M32 -> Q\n\nLoads a 32-bit value from memory into the quad register (A:X:Y:Z).";
    details["LSRQ"] = "Logical Shift Right Quad\n\nOperations: 0 -> [Quad] -> C\n\nShifts all 32 bits of the quad register or memory contents one bit right.";
    details["ORQ"] = "Logical Inclusive OR Quad\n\nOperations: Q OR M32 -> Q\n\nPerforms a bitwise inclusive OR between the 32-bit quad register and memory.";
    details["ROLQ"] = "Rotate Left Quad\n\nOperations: C <- [Quad] <- C\n\nRotates the 32 bits of the quad register or memory one bit left through the carry flag.";
    details["RORQ"] = "Rotate Right Quad\n\nOperations: C -> [Quad] -> C\n\nRotates the 32 bits of the quad register or memory one bit right through the carry flag.";
    details["STQ"] = "Store Quad\n\nOperations: Q -> M32\n\nStores the 32-bit quad register (A:X:Y:Z) into four consecutive bytes of memory.";

    return details;
}

#endif
