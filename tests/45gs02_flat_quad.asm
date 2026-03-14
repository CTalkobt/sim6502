* = $0200
 // EXPECT: A=78 X=56 Y=34 Z=00 B=00 S=FF PC=0228

    .cpu _45gs02

 // Store 32-bit far address $00020000 in ZP $10-$13 first,

 // so subsequent LDA instructions don't clobber Q before STQ

    lda #$00
    sta $10
    sta $11
    lda #$02
    sta $12
    lda #$00
    sta $13

 // Set Q = $00345678 (Z=0 so STQ [$10],Z writes at $00020000 with no offset)

    lda #$78
    ldx #$56
    ldy #$34
    ldz #$00

 // Write Q to far address $00020000 using flat STQ [$10],Z (Z=0)

    stq (($10))

 // Clear Q and Z

    lda #$00
    ldx #$00
    ldy #$00
    ldz #$00

 // Read Q back from far address $00020000 using flat LDQ [$10],Z (Z=0)

    ldq (($10)),z

    rts
