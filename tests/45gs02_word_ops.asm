* = $0200
 // EXPECT: A=12 X=00 Y=00 Z=00 B=00 S=FF PC=0232

    .cpu _45gs02

 // 1. Test PHW Immediate

    phw #$1234
 // S should be $FD


 // 2. Test INW (ZP)

    lda #$ff
    sta $20
    lda #$00
    sta $21
    inw $20
 // Result $0100 at $20,$21


 // 3. Test DEW (ZP)

    dew $20
 // Result $00FF at $20,$21


 // 4. Test ASW (ABS)

    lda #$00
    sta $1000
    lda #$80
    sta $1001
    asw $1000
 // Result $0000 at $1000, Carry = 1


 // 5. Test ROW (ABS)

    sec
    lda #$01
    sta $1002
    lda #$00
    sta $1003
    row $1002
 // Result $8000 at $1002, Carry = 1


 // Verify ASW/ROW results via registers

    lda $1001 // Should be $00

    lda $1003 // Should be $80

 // Pop PHW'd bytes off stack (S: $FD -> $FF)
    pla
    pla
    rts
