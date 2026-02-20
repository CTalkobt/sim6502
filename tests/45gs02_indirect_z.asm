; EXPECT: A=55 X=00 Y=00 Z=00 B=00 PC=003F
.processor 45gs02

; Set up a pointer in zero page $10
LDA #$00
STA $10
LDA #$20
STA $11

; Write value $AA at $2000
LDA #$AA
STA $2000

; Test LDA (zp),Z
LDZ #$00
LDA ($10),Z
; Expect A = $AA

; Test STA (zp),Z
LDA #$BB
LDZ #$05
STA ($10),Z
; Expect $2005 = $BB

; Test ADC (zp),Z
LDA #$10
LDZ #$00
CLC
ADC ($10),Z
; Expect A = $10 + $AA = $BA

; Test SBC (zp),Z
LDA #$FF
LDZ #$05
SEC
SBC ($10),Z
; Expect A = $FF - $BB = $44

; Test CMP (zp),Z
LDA #$AA
LDZ #$00
CMP ($10),Z
; Expect Z flag set

; Test ORA (zp),Z
LDA #$55
LDZ #$00
ORA ($10),Z
; Expect A = $55 | $AA = $FF

; Test AND (zp),Z
LDA #$0F
LDZ #$00
AND ($10),Z
; Expect A = $0F & $AA = $0A

; Test EOR (zp),Z
LDA #$FF
LDZ #$00
EOR ($10),Z
; Expect A = $FF ^ $AA = $55

BRK
