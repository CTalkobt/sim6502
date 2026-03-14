 // EXPECT: A=42 X=00 Y=00 S=FF PC=0203

 // Test .bin pseudo-op: include tests/data/three_bytes.bin ($42 $AB $FF)

 // at the current PC and verify the first byte is loaded into A.


    * = $0200
    lda bin_data // first byte of included binary = $42

    rts

    bin_data:
    .import binary "tests/code/data/three_bytes.bin"
