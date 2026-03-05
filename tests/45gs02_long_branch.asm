; EXPECT: A=01 X=00 Y=00 Z=00 B=00 S=FF PC=0209
.processor 45gs02
CLC
LBCC target
LDA #$FF
BRK
target:
LDA #$01
BRK
