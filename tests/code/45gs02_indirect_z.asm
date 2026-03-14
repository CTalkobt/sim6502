* = $0200
 // EXPECT: A=55 X=00 Y=00 Z=00 B=00 S=FF PC=023D

    .cpu _45gs02

 // Set up a pointer in zero page $10

    lda #$00
    sta $10
    lda #$20
    sta $11

 // Write value $AA at $2000

    lda #$aa
    sta $2000

 // Test LDA (zp),Z

    ldz #$00
    lda ($10),z
 // Expect A = $AA


 // Test STA (zp),Z

    lda #$bb
    ldz #$05
    sta ($10),z
 // Expect $2005 = $BB


 // Test ADC (zp),Z

    lda #$10
    ldz #$00
    clc
    adc ($10),z
 // Expect A = $10 + $AA = $BA


 // Test SBC (zp),Z

    lda #$ff
    ldz #$05
    sec
    sbc ($10),z
 // Expect A = $FF - $BB = $44


 // Test CMP (zp),Z

    lda #$aa
    ldz #$00
    cmp ($10),z
 // Expect Z flag set


 // Test ORA (zp),Z

    lda #$55
    ldz #$00
    ora ($10),z
 // Expect A = $55 | $AA = $FF


 // Test AND (zp),Z

    lda #$0f
    ldz #$00
    and ($10),z
 // Expect A = $0F & $AA = $0A


 // Test EOR (zp),Z

    lda #$ff
    ldz #$00
    eor ($10),z
 // Expect A = $FF ^ $AA = $55


    rts
