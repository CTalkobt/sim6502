* = $0200
 // EXPECT: A=01 X=00 Y=00 S=FF PC=0203

 // FLAGS: -b "$0203 .C == 1"

    sec
    lda #$01
    rts
