; EXPECT: A=30 X=00 Y=00 Z=00 B=00 S=FF PC=0006
.processor 45gs02
; SEE sets bit 5 (E flag) of P; CLE clears it.
; PHP exposes P on the stack so PLA can verify it.
SEE         ; P = 0x20
PHP         ; push P|FLAG_B = 0x30,  S=FE
CLE         ; P = 0x00
PHP         ; push P|FLAG_B = 0x10,  S=FD
PLA         ; A = 0x10 (CLE result),  S=FE
PLA         ; A = 0x30 (SEE result),  S=FF
BRK
