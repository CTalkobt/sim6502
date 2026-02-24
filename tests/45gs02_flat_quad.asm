; EXPECT: A=78 X=56 Y=34 Z=00 B=00 PC=0024
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

; Set Q = $00345678 (Z=0 so STQ [$10],Z writes at $00020000 with no offset)
LDA #$78
LDX #$56
LDY #$34
LDZ #$00

; Write Q to far address $00020000 using flat STQ [$10],Z (Z=0)
STQ [$10],Z

; Clear Q and Z
LDA #$00
LDX #$00
LDY #$00
LDZ #$00

; Read Q back from far address $00020000 using flat LDQ [$10],Z (Z=0)
LDQ [$10],Z

BRK
