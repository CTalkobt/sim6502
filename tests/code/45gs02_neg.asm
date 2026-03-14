* = $0200
 // EXPECT: A=FF X=00 Y=00 Z=00 B=00 S=FF PC=0203

    .cpu _45gs02
    lda #$01
    neg
    rts
