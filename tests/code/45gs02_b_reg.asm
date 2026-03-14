* = $0200
 // EXPECT: A=42 X=00 Y=00 Z=00 B=42 S=FF PC=0206

    .cpu _45gs02
    lda #$42
    tab
    lda #$00
    tba
    rts
