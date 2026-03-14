* = $0200
 // EXPECT: A=00 X=01 Y=00 Z=00 B=00 S=FF PC=020C

 // Test STQ absolute and BEQ with $addr notation

    .cpu _45gs02

    lda #$00 // A=0, sets Z

    ldx #$01 // X=1, clears Z

    stq $1900 // store Q (A,X,Y,Z) to $1900-$1903

    beq sec_path // Z=0 after LDX, not taken — falls to CLC

    clc
    rts

    sec_path:
    sec // branch target at $000C (not reached)

    rts
