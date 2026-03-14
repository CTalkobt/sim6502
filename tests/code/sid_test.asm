* = $0200

    lda #$00
    sta $d400 // Freq Lo

    lda #$10
    sta $d401 // Freq Hi

    lda #$11
    sta $d404 // Triangle + Gate

    lda #$0f
    sta $d418 // Volume Max


    loop:
    jmp loop
