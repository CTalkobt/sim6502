* = $0200
 // EXPECT: A=15 X=00 Y=00 S=FF PC=0206

 // BCD subtraction: $20 - $05 = $15 (20-5=15 in decimal)

    sec
    sed
    lda #$20
    sbc #$05
    rts
