; EXPECT: A=48 X=0A Y=00 S=FF PC=0206
; Test .byte and .word pseudo-ops

    LDA bytedata    ; load 'H' = $48
    LDX wordlo      ; load lo byte of .word 10 = $0A
    BRK

bytedata:
    .byte 'H'           ; $48 at $0208
wordlo:
    .word 10            ; $0A,$00 at $0209
