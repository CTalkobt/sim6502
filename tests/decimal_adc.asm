; EXPECT: A=13 X=00 Y=00 S=FF PC=0206
; BCD addition: $08 + $05 = $13 (8+5=13 in decimal)
CLC
SED
LDA #$08
ADC #$05
BRK
