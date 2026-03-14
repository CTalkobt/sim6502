* = $0200
 // EXPECT: A=42 X=00 Y=00 S=FF PC=0207

 //

 // Self-modifying code: overwrite the operand byte of a subsequent LDA instruction.

 //

 // Layout at $0200:

 //   $0200-$0201  LDA #$42       ; opcode $A9, operand $42

 //   $0202-$0204  STA $0206      ; write $42 to the operand of the LDA #$FF below

 //   $0205-$0206  LDA #$FF       ; operand byte at $0206 will be overwritten → LDA #$42

 //   $0207        BRK

 //

 // If execution reads from mem[] (SMC works), A ends up $42.

 // If execution reads from a stale ROM copy,  A ends up $FF.


    lda #$42
    sta $0206
    lda #$ff
    rts
