* = $0200
 // EXPECT: A=30 X=00 Y=00 S=FF PC=0202

 // PHP must push P with FLAG_B (0x10) and the always-1 bit 5 (0x20) set.

 // With a freshly reset P=0x00: pushed byte = 0x00 | 0x10 | 0x20 = 0x30.

    php
    pla
    rts
