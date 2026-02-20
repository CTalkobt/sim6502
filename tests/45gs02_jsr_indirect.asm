; EXPECT: A=02 X=02 Y=00 Z=00 B=00 PC=0025
.processor 45gs02
  JMP main
sub:
  LDA #$01
  RTS
sub2:
  LDA #$02
  RTS
main:
  ; sub is at $0003
  LDA #$03
  STA $1002
  LDA #$00
  STA $1003
  LDX #$02
  JSR ($1000,X)
  ; sub2 is at $0006
  LDA #$06
  STA $1004
  LDA #$00
  STA $1005
  JSR ($1004)
  BRK
