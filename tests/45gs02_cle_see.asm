* = $0200
 // EXPECT: A=10 X=00 Y=00 Z=00 B=00 S=FF PC=0206

    .cpu _45gs02
 // CLE/SEE control the E flag (bit 5 of P), switching between:

 //   E=1 (emulation): 8-bit SP, stack fixed on page 1 (0x0100-0x01FF)

 //   E=0 (extended):  16-bit SP, stack at arbitrary 16-bit address

 //

 // SEE first: emulation mode (E=1), page-1 stack

    see // P = 0x20 (FLAG_E set)

    php // push P|FLAG_B = 0x30 to addr 0x01FF, S -> FE

    pla // pop from addr 0x01FF, A = 0x30, S -> FF

 // CLE: extended mode (E=0), full 16-bit stack

    cle // P = 0x00 (FLAG_E clear)

    php // push P|FLAG_B = 0x10 to addr 0x00FF, S -> FE

    pla // pop from addr 0x00FF, A = 0x10, S -> FF

    rts
