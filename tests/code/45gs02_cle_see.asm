* = $0200
 // EXPECT: A=14 X=00 Y=00 Z=00 B=00 S=FF PC=0206

    .cpu _45gs02
 // CLE/SEE control the E flag (bit 5 of P), switching between:

 //   E=1 (emulation): 8-bit SP, stack fixed on page 1 (0x0100-0x01FF)

 //   E=0 (extended):  16-bit SP, stack at arbitrary 16-bit address

 //

 // SEE first: emulation mode (E=1), page-1 stack

    see // P = 0x24 (FLAG_I|FLAG_E already set on reset, SEE is a no-op here)

    php // push P|FLAG_B = 0x34 to addr 0x01FF, S -> FE

    pla // pop from addr 0x01FF, A = 0x34, S -> FF

 // CLE: extended mode (E=0), full 16-bit stack

    cle // P = 0x04 (FLAG_E cleared, FLAG_I remains)

    php // push P|FLAG_B = 0x14 to addr 0x00FF, S -> FE

    pla // pop from addr 0x00FF, A = 0x14, S -> FF

    rts
