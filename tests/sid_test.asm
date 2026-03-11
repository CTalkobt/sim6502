.target c64
LDA #$00
STA $D400 ; Freq Lo
LDA #$10
STA $D401 ; Freq Hi
LDA #$11
STA $D404 ; Triangle + Gate
LDA #$0F
STA $D418 ; Volume Max

loop:
    JMP loop
