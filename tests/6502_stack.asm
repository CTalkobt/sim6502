* = $0200
 // EXPECT: A=42 X=00 Y=00 S=FF PC=0206

    lda #$42
    pha
    lda #$00
    pla
    rts
