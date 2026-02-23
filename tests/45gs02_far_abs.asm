; EXPECT: A=42 X=10 Y=F0 Z=AB B=00 PC=0034
.processor 45gs02

; Write $42 directly to physical address $20000 (above 64KB)
LDA #$42
STA $020000

; Write $AB to physical address $30000
LDA #$AB
STZ $030001       ; store 0 at $030001 first
LDA #$AB
STA $030000

; Write $10 to far address + X offset: $020010,X where X=0 -> $020010
LDX #$00
LDA #$10
STA $020010,X

; Verify: load back A from $020000 (should be $42)
LDA $020000

; Load X from far address $020010 (should be $10)
LDX $020010

; Store Y=$F0 to $030002, then load Z from $030000 (=$AB)
LDY #$F0
STY $030002
LDZ $030000

BRK
