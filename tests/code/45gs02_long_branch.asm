* = $0200
 // EXPECT: A=01 X=00 Y=00 Z=00 B=00 S=FF PC=0209

    .cpu _45gs02
    clc
    lbcc target
    lda #$ff
    rts
    target:
    lda #$01
    rts
