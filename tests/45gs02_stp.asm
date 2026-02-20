; EXPECT: A=42 X=00 Y=00 Z=00 B=00 S=FF PC=0002
.processor 45gs02
; STP must halt execution; the LDA #$FF after it must NOT run.
LDA #$42
STP
LDA #$FF
BRK
