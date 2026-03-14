* = $0200
 // EXPECT: A=01 X=00 Y=00 S=FF PC=020C

    lda #$01
    sta $10
    lda #$01
    bit $10
    beq error
    lda #$01
    rts
    error:
    lda #$ff
    rts
