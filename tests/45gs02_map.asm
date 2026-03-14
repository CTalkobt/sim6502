* = $2000
 // EXPECT: A=42 X=10 Y=00 Z=00 B=00 S=FF PC=2010

 // PROCESSOR: 45gs02

 // FLAGS: -a 0x2000

    .cpu _45gs02

 // Write $42 to physical $1000 (MAP not yet active, virtual == physical)

    lda #$42
    sta $1000

 // Load MAP registers:

 //   X = $10  hi nibble = $1 (lo_select: bit 0 = map $0000-$1FFF)

 //            lo nibble = $0 (lo_offset bits [19:16] = 0)

 //   A = $10  lo_offset bits [15:8] = $10  →  lo_offset = $1000

 //   Z = $00  (hi_select = 0, hi_offset bits [19:16] = 0)

 //   Y = $00  hi_offset bits [15:8] = 0

    lda #$10
    ldx #$10
    ldz #$00
    ldy #$00
    map

 // Now virtual $0000 translates to physical $0000 + $1000 = $1000

    lda $0000

    rts
