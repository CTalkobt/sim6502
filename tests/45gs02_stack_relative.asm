; EXPECT: A=AA X=F0 Y=02 Z=00 B=00 PC=001F
.processor 45gs02
; Pointer at $01F0 = $2000
LDA #$00
STA $01F0
LDA #$20
STA $01F1
; Set S = $F0, so SP points to $01F0
LDX #$F0
TXS
; Value $42 at $2005
LDA #$42
STA $2005
LDY #$05
; Load from ($00,SP),Y => $2005
LDA ($00,SP),Y
; Now test STA
LDA #$AA
LDY #$02
; Store $AA at ($00,SP),Y => $2002
STA ($00,SP),Y
; Check $2002
LDA $2002
BRK
