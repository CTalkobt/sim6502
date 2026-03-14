* = $0200
 // EXPECT: A=0A X=00 Y=00 Z=00 B=00 S=FF PC=022C

    .cpu _45gs02

 // Store 0x00000005 at ZP $10

    lda #$05
    sta $10
    lda #$00
    sta $11
    sta $12
    sta $13

 // Store 0x0000000F at ZP $20

    lda #$0f
    sta $20
    lda #$00
    sta $21
    sta $22
    sta $23

 // LDQ $10 -> Q = 0x00000005 (A=05 X=00 Y=00 Z=00)

    ldq $10

 // EORQ $20 -> Q = 0x05 XOR 0x0F = 0x0A (A=0A X=00 Y=00 Z=00)

    eorq $20

 // ASLQ -> Q = 0x0A << 1 = 0x14 (A=14)

    aslq

 // LSRQ -> Q = 0x14 >> 1 = 0x0A (A=0A)

    lsrq

 // INQ -> Q = 0x0B (A=0B)

    inq

 // DEQ -> Q = 0x0A (A=0A)

    deq

    rts
