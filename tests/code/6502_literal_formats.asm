* = $0200
 // EXPECT: A=41 X=0A Y=FF S=FF PC=0208

    lda #255 // decimal 255 = $FF

    ldx #10 // decimal 10 = $0A

    ldy #%11111111 // binary %11111111 = $FF

    lda #$41 // character 'A' = $41

    rts
