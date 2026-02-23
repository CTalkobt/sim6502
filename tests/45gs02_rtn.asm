; EXPECT: A=42 X=00 Y=00 Z=00 B=00 S=FF PC=0006
; PROCESSOR: 45gs02
.processor 45gs02

; Test RTN #n: like RTS but also adjusts SP by n after pulling the return address.
; Push one dummy byte, call SUBR, which returns with RTN #1 to also discard the param.
; Layout:
;   $0000 LDA #$AA     (2 bytes)
;   $0002 PHA          (1 byte)  S: FF->FE
;   $0003 JSR SUBR     (3 bytes) pushes ret=$0005, S: FE->FC
;   $0006 BRK          <- RTN returns here (PC=0006), S restored to FF
;   $0008 SUBR: LDA #$42
;   $000A         RTN #$01

        LDA #$AA
        PHA
        JSR SUBR
        BRK

SUBR:   LDA #$42
        RTN #$01
