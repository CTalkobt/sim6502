 // EXPECT: A=AB X=00 Y=00 S=FF PC=0203

 // Test .org — relocate data section to $0800


    * = $0200
    lda data // forward ref, resolved to $0800 in pass 2

    rts

    * = $0800
    data:
    .byte $ab // $AB placed at $0800

