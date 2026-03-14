* = $0200
 // EXPECT: A=42 X=10 Y=20 S=FF PC=0206

    lda #$42
    ldx #$10
    ldy #$20
    rts
