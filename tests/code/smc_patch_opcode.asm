* = $0200
 // EXPECT: A=CA X=04 Y=00 S=FF PC=0208

 //

 // Self-modifying code: overwrite the opcode byte of the next instruction.

 //

 // Layout at $0200:

 //   $0200-$0201  LDA #$CA       ; DEX opcode value

 //   $0202-$0204  STA $0207      ; write $CA (DEX) over the INX opcode at $0207

 //   $0205-$0206  LDX #$05       ; X = 5

 //   $0207        INX            ; opcode at $0207 patched to $CA (DEX) → X = 4

 //   $0208        BRK

 //

 // If SMC works: opcode $E8 (INX) becomes $CA (DEX), so X goes 5→4.

 // If stale ROM:  INX executes,  X goes 5→6.


    lda #$ca
    sta $0207
    ldx #$05
    inx
    rts
