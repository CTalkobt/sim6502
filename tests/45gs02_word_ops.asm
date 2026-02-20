; EXPECT: A=80 X=00 Y=00 Z=00 B=00 S=FD PC=0034
.processor 45gs02

; 1. Test PHW Immediate
PHW #$1234
; S should be $FD

; 2. Test INW (ZP)
LDA #$FF
STA $20
LDA #$00
STA $21
INW $20
; Result $0100 at $20,$21

; 3. Test DEW (ZP)
DEW $20
; Result $00FF at $20,$21

; 4. Test ASW (ABS)
LDA #$00
STA $1000
LDA #$80
STA $1001
ASW $1000
; Result $0000 at $1000, Carry = 1

; 5. Test ROW (ABS)
SEC
LDA #$01
STA $1002
LDA #$00
STA $1003
ROW $1002
; Result $8000 at $1002, Carry = 1

; Verify ASW/ROW results via registers
LDA $1001 ; Should be $00
LDA $1003 ; Should be $80
BRK
