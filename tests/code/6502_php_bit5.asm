* = $0200
 // EXPECT: A=34 X=00 Y=00 S=FF PC=0202

 // PHP must push P with FLAG_B (0x10) and the always-1 bit 5 (0x20) set.

 // With reset P=0x24 (FLAG_I|FLAG_U): pushed byte = 0x24 | 0x10 | 0x20 = 0x34.

    php
    pla
    rts
