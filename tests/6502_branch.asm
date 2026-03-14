* = $0200
 // EXPECT: A=02 X=00 Y=00 S=FF PC=020A

    lda #$01
    sec
    bcs label
    lda #$05
    label:
    clc
    adc #$01
    rts
