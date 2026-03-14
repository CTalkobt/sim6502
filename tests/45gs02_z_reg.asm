* = $0200
 // EXPECT: A=00 X=00 Y=00 Z=42 B=00 S=FF PC=0205

    .cpu _45gs02
    lda #$42
    taz
    lda #$00
    rts
