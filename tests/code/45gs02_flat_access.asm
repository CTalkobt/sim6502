* = $0200
 // EXPECT: A=42 X=00 Y=00 Z=00 B=00 S=FF PC=021A

    .cpu _45gs02

 // Store 32-bit far address $00012345 in ZP $10-$13

    lda #$45
    sta $10
    lda #$23
    sta $11
    lda #$01
    sta $12
    lda #$00
    sta $13

 // Write $42 to far address $00012345 using flat STA [$10],Z (Z=0)

    lda #$42
    sta (($10)),z

 // Clear A to prove the subsequent LDA actually reads from far memory

    lda #$00

 // Read back $42 from far address $00012345 using flat LDA [$10],Z

    lda (($10)),z

    rts
