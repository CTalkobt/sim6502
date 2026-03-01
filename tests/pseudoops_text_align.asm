; EXPECT: A=48 X=42 Y=00 S=FF PC=0206
; Test .text and .align
;
; Memory layout:
;   $0200  LDA text_data  (3 bytes)
;   $0203  LDX aligned_val (3 bytes)
;   $0206  BRK            (2 bytes)
;   $0208  .align 16 pads 8 bytes to reach $0210
;   $0210  aligned_val: .byte $42
;   $0211  text_data: .text "Hello"  -> 'H'=48 'e' 'l' 'l' 'o'

.org $0200
    LDA text_data       ; first char of "Hello" = 'H' = $48
    LDX aligned_val     ; $42
    BRK

    .align 16           ; $0208 -> $0210 (pad 8 zero bytes)
aligned_val:
    .byte $42

text_data:
    .text "Hello"
