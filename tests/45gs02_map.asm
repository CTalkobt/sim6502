; EXPECT: A=42 X=10 Y=00 Z=00 B=00 PC=0011
.processor 45gs02

; Write $42 to physical $1000 (MAP not yet active, virtual == physical)
LDA #$42
STA $1000

; Load MAP registers:
;   X = $10  hi nibble = $1 (lo_select: bit 0 = map $0000-$1FFF)
;            lo nibble = $0 (lo_offset bits [19:16] = 0)
;   A = $10  lo_offset bits [15:8] = $10  →  lo_offset = $1000
;   Z = $00  (hi_select = 0, hi_offset bits [19:16] = 0)
;   Y = $00  hi_offset bits [15:8] = 0
LDA #$10
LDX #$10
LDZ #$00
LDY #$00
MAP

; Now virtual $0000 translates to physical $0000 + $1000 = $1000
LDA $0000

BRK
