; EXPECT: A=42 X=00 Y=00 Z=00 B=00 PC=001A
.processor 45gs02

; Store 32-bit far address $00012345 in ZP $10-$13
LDA #$45
STA $10
LDA #$23
STA $11
LDA #$01
STA $12
LDA #$00
STA $13

; Write $42 to far address $00012345 using flat STA [$10],Z (Z=0)
LDA #$42
STA [$10],Z

; Clear A to prove the subsequent LDA actually reads from far memory
LDA #$00

; Read back $42 from far address $00012345 using flat LDA [$10],Z
LDA [$10],Z

BRK
