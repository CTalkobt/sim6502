 // EXPECT: A=42 X=00 Y=00 S=FF PC=0402

    .cpu _65c02
    * = $0200

    start:
 // Set up pointer at $02FF and $0300

 // We want JMP ($02FF) to jump to $0400

    lda #$00
    sta $02ff
    lda #$04
    sta $0300

 // If bug exists (NMOS style), it will read high byte from $0200

 // $0200 contains 'start' ($A9)

 // So it would jump to $A900


    jmp ($02ff)

    * = $0400
    target:
    lda #$42
    rts
