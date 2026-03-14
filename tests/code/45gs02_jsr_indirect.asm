* = $0200
 // EXPECT: A=02 X=02 Y=00 Z=00 B=00 S=FF PC=0225

    .cpu _45gs02
    jmp main
    sub:
    lda #$01
    rts
    sub2:
    lda #$02
    rts
    main:
 // sub is at $0203

    lda #$03
    sta $1002
    lda #$02
    sta $1003
    ldx #$02
    jsr ($1000,x)
 // sub2 is at $0206

    lda #$06
    sta $1004
    lda #$02
    sta $1005
    jsr ($1004)
    rts
