* = $0200
 // EXPECT: A=42 X=00 Y=00 S=FF PC=020D

 //

 // Self-modifying code: redirect a JMP by overwriting its lo address byte.

 //

 // Layout at $0200:

 //   $0200-$0201  LDA #<right    ; lo byte of 'right' ($020B)

 //   $0202-$0204  STA $0206      ; overwrite lo byte of JMP target (at $0206)

 //   $0205-$0207  JMP wrong      ; initially JMP $0208, patched to JMP $020B

 //   $0208-$0209  wrong: LDA #$FF  ; wrong path — never reached after patch

 //   $020A        BRK

 //   $020B-$020C  right: LDA #$42  ; right path — taken after patch

 //   $020D        BRK

 //

 // Hi byte at $0207 is already $02 for both targets; only lo byte needs patching.


    lda #<right
    sta $0206
    jmp wrong
    wrong: lda #$ff
    rts
    right: lda #$42
    rts
