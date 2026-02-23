; EXPECT: A=78 X=56 Y=34 Z=12 B=00 PC=0024
.processor 45gs02

; Store 32-bit far address $00020000 in ZP $10-$13 first,
; so subsequent LDA instructions don't clobber Q before STQ
LDA #$00
STA $10
STA $11
LDA #$02
STA $12
LDA #$00
STA $13

; Set Q = $12345678
LDA #$78
LDX #$56
LDY #$34
LDZ #$12

; Write Q to far address $00020000 using flat STQ [$10]
STQ [$10]

; Clear Q and Z
LDA #$00
LDX #$00
LDY #$00
LDZ #$00

; Read Q back from far address $00020000 using flat LDQ [$10],Z (Z=0)
LDQ [$10],Z

BRK
