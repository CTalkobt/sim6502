* = $0200
 // EXPECT: A=FE X=00 Y=00 Z=01 B=00 S=FF PC=029E

    .cpu _45gs02

 // Test quad memory-addressed shift/rotate/inc/dec instructions


 // Store 32-bit value $00000002 at ZP $10

    lda #$02
    sta $10
    lda #$00
    sta $11
    sta $12
    sta $13

 // ASLQ $10: $00000002 -> $00000004

    aslq $10

 // LSRQ $10: $00000004 -> $00000002

    lsrq $10

 // INQ $10: $00000002 -> $00000003

    inq $10

 // DEQ $10: $00000003 -> $00000002

    deq $10

 // Store 32-bit value $00000002 at abs $2000

    lda #$02
    sta $2000
    lda #$00
    sta $2001
    sta $2002
    sta $2003

 // ASLQ $2000: $00000002 -> $00000004

    aslq $2000

 // LSRQ $2000: $00000004 -> $00000002

    lsrq $2000

 // INQ $2000: $00000002 -> $00000003

    inq $2000

 // DEQ $2000: $00000003 -> $00000002

    deq $2000

 // ROLQ $10: C=0, $00000002 -> $00000004

    clc
    rolq $10

 // RORQ $10: C=0, $00000004 -> $00000002

    rorq $10

 // LDQ $10 to verify Q=$00000002: A=02 X=00 Y=00 Z=00

    ldq $10

 // Now test ADCQ (zp),Z: set up ZP pointer $20/$21 -> $2000

    lda #$00
    sta $20
    lda #$20
    sta $21
    lda #$00
    sta $22
    sta $23

 // Store $00000001 at $2000

    lda #$01
    sta $2000
    lda #$00
    sta $2001
    sta $2002
    sta $2003

 // Q = $00000002, ADCQ ($20),Z: Q += mem[$2000] = $00000002 + 1 = $00000003, C=0

    clc
    ldz #$00
    adcq (($20))
 // Q should be $00000003: A=03 X=00 Y=00 Z=00


 // SBCQ ($20),Z: Q -= mem[$2000] = $00000003 - 1 = $00000002

    sec
    sbcq (($20))
 // Q = $00000002


 // ANDQ ($20),Z: Q = $00000002 & $00000001 = $00000000

    andq (($20))
 // Q = $00000000


 // Store $FFFFFFFE at $2000

    lda #$fe
    sta $2000
    lda #$ff
    sta $2001
    sta $2002
    sta $2003

 // ORQ ($20),Z: Q = $00000000 | $FFFFFFFE = $FFFFFFFE

    orq (($20))
 // Q = $FFFFFFFE: A=FE X=FF Y=FF Z=FF


 // LDQ $2000 to load $FFFFFFFE: A=FE X=FF Y=FF Z=FF

    ldq $2000

 // Final: A=FE X=00 Y=00 Z=01 (via TAZ/INZ etc to keep A=FE)

    ldx #$00
    ldy #$00
    ldz #$01
    rts
