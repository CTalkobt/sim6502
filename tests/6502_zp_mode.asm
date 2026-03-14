* = $0200
 // EXPECT: A=42 X=00 Y=00 S=FF PC=0206

 // Verify $xx addresses use zero-page mode (2-byte instructions).

 // With broken abs-mode: STA $10 and LDA $10 would each be 3 bytes → BRK at $0008.

 // With correct ZP mode:  STA $10 and LDA $10 are each 2 bytes  → BRK at $0006.

    lda #$42
    sta $10
    lda $10
    rts
