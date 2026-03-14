* = $0200
 // EXPECT: A=01 X=06 Y=04 S=FF PC=0209

    .cpu _65c02
    lda #$00
    inc
    ldx #$05
    inx
    ldy #$05
    dey
    rts
