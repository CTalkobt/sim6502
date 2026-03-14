* = $0200
 // EXPECT: A=AA X=F0 Y=02 Z=00 B=00 S=FF PC=0224

    .cpu _45gs02
 // Pointer at $01F0 = $2000

    lda #$00
    sta $01f0
    lda #$20
    sta $01f1
 // Set S = $F0, so SP points to $01F0

    ldx #$f0
    txs
 // Value $42 at $2005

    lda #$42
    sta $2005
    ldy #$05
 // Load from ($00,SP),Y => $2005

    lda ($00,sp),y
 // Now test STA

    lda #$aa
    ldy #$02
 // Store $AA at ($00,SP),Y => $2002

    sta ($00,sp),y
 // Check $2002

    lda $2002
 // Restore S to $FF for exit detection
    ldx #$ff
    txs
    ldx #$f0
    rts
