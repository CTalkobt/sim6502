; EXPECT: A=AB X=00 Y=00 S=FF PC=0203
; Test .org — relocate data section to $0800

.org $0200
    LDA data        ; forward ref, resolved to $0800 in pass 2
    BRK

.org $0800
data:
    .byte $AB       ; $AB placed at $0800
