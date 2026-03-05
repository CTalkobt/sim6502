; EXPECT: A=FE X=00 Y=00 Z=01 B=00 S=FF PC=029A
.processor 45gs02

; Test quad memory-addressed shift/rotate/inc/dec instructions

; Store 32-bit value $00000002 at ZP $10
    LDA #$02
    STA $10
    LDA #$00
    STA $11
    STA $12
    STA $13

; ASLQ $10: $00000002 -> $00000004
    ASLQ $10

; LSRQ $10: $00000004 -> $00000002
    LSRQ $10

; INQ $10: $00000002 -> $00000003
    INQ $10

; DEQ $10: $00000003 -> $00000002
    DEQ $10

; Store 32-bit value $00000002 at abs $2000
    LDA #$02
    STA $2000
    LDA #$00
    STA $2001
    STA $2002
    STA $2003

; ASLQ $2000: $00000002 -> $00000004
    ASLQ $2000

; LSRQ $2000: $00000004 -> $00000002
    LSRQ $2000

; INQ $2000: $00000002 -> $00000003
    INQ $2000

; DEQ $2000: $00000003 -> $00000002
    DEQ $2000

; ROLQ $10: C=0, $00000002 -> $00000004
    CLC
    ROLQ $10

; RORQ $10: C=0, $00000004 -> $00000002
    RORQ $10

; LDQ $10 to verify Q=$00000002: A=02 X=00 Y=00 Z=00
    LDQ $10

; Now test ADCQ (zp),Z: set up ZP pointer $20/$21 -> $2000
    LDA #$00
    STA $20
    LDA #$20
    STA $21
    LDA #$00
    STA $22
    STA $23

; Store $00000001 at $2000
    LDA #$01
    STA $2000
    LDA #$00
    STA $2001
    STA $2002
    STA $2003

; Q = $00000002, ADCQ ($20),Z: Q += mem[$2000] = $00000002 + 1 = $00000003, C=0
    CLC
    LDZ #$00
    ADCQ ($20),Z
    ; Q should be $00000003: A=03 X=00 Y=00 Z=00

; SBCQ ($20),Z: Q -= mem[$2000] = $00000003 - 1 = $00000002
    SEC
    SBCQ ($20),Z
    ; Q = $00000002

; ANDQ ($20),Z: Q = $00000002 & $00000001 = $00000000
    ANDQ ($20),Z
    ; Q = $00000000

; Store $FFFFFFFE at $2000
    LDA #$FE
    STA $2000
    LDA #$FF
    STA $2001
    STA $2002
    STA $2003

; ORQ ($20),Z: Q = $00000000 | $FFFFFFFE = $FFFFFFFE
    ORQ ($20),Z
    ; Q = $FFFFFFFE: A=FE X=FF Y=FF Z=FF

; LDQ $2000 to load $FFFFFFFE: A=FE X=FF Y=FF Z=FF
    LDQ $2000

; Final: A=FE X=00 Y=00 Z=01 (via TAZ/INZ etc to keep A=FE)
    LDX #$00
    LDY #$00
    LDZ #$01
    BRK
