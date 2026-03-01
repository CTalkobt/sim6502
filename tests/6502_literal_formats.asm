; EXPECT: A=41 X=0A Y=FF S=FF PC=0208
LDA #255        ; decimal 255 = $FF
LDX #10         ; decimal 10 = $0A
LDY #%11111111  ; binary %11111111 = $FF
LDA #'A'        ; character 'A' = $41
BRK
