; EXPECT: A=00 X=01 Y=00 Z=00 B=00 PC=000A
; Test STQ absolute and BEQ with $addr notation
.processor 45gs02

LDA #$00        ; A=0, sets Z
LDX #$01        ; X=1, clears Z
STQ $1900       ; store Q (A,X,Y,Z) to $1900-$1903
BEQ sec_path    ; Z=0 after LDX, not taken — falls to CLC
CLC
BRK

sec_path:
SEC             ; branch target at $000C (not reached)
BRK
