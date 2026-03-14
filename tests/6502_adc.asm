* = $0200
 // EXPECT: A=03 X=00 Y=00 S=FF PC=0205

    clc
    lda #$01
    adc #$02
    rts
