; EXPECT: A=42 X=00 Y=00 S=FF PC=0203
; Test .bin pseudo-op: include tests/data/three_bytes.bin ($42 $AB $FF)
; at the current PC and verify the first byte is loaded into A.

.org $0200
    LDA bin_data        ; first byte of included binary = $42
    BRK

bin_data:
    .bin "tests/data/three_bytes.bin"
