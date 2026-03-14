* = $0200
 // EXPECT: A=13 X=00 Y=00 S=FF PC=0206

 // BCD addition: $08 + $05 = $13 (8+5=13 in decimal)

    clc
    sed
    lda #$08
    adc #$05
    rts
