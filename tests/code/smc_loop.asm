* = $0200
 // EXPECT: A=0A X=05 Y=00 S=FF PC=020F

 //

 // Self-modifying loop: each iteration patches the ADC operand to the current

 // loop counter X, accumulating 0+1+2+3+4 = 10 ($0A) over 5 iterations.

 //

 // Layout at $0200:

 //   $0200-$0201  LDA #$00       ; A = 0 (running total)

 //   $0202-$0203  LDX #$00       ; X = loop counter

 //   $0204        loop: INX      ; X = 1..5

 //   $0205        CLC

 //   $0206-$0207  ADC #$00       ; operand byte at $0207 patched each iteration

 //   $0208-$020A  STX $0207      ; write X into ADC operand (absolute $8E encoding)

 //   $020B-$020C  CPX #$05

 //   $020D-$020E  BNE loop       ; branch back while X < 5

 //   $020F        BRK

 //

 // Iteration results (X → ADC operand used → running A):

 //   1 → ADC #$00 → A=0    (patch operand to 1)

 //   2 → ADC #$01 → A=1    (patch operand to 2)

 //   3 → ADC #$02 → A=3    (patch operand to 3)

 //   4 → ADC #$03 → A=6    (patch operand to 4)

 //   5 → ADC #$04 → A=10   (patch operand to 5, then exit)


    lda #$00
    ldx #$00
    loop: inx
    clc
    adc #$00
    stx $0207
    cpx #$05
    bne loop
    rts
