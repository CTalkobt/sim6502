; EXPECT: A=01 X=00 Y=00 S=FF PC=0203
; FLAGS: -b "$0203 .C == 1"
    SEC
    LDA #$01
    BRK
