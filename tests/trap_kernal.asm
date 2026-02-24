; EXPECT: A=41 X=00 Y=00 PC=0005
; FLAGS: --preset c64
; Test: TRAP intercept via --preset c64.
; JSR to CHROUT ($FFD2) should be trapped, dump registers, and simulate RTS.
; The program continues as if CHROUT returned normally.

        LDA #$41    ; 'A' — 2 bytes at $0000-$0001
        JSR $FFD2   ; CHROUT — 3 bytes at $0002-$0004; trap simulates RTS → PC=$0005
        BRK         ; 1 byte at $0005 — halts
