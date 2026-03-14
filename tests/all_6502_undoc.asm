// Test all instructions for 6502_undoc
 // Format matched to KickAssembler illegal opcodes table A.4

    .cpu _6502
    * = $1000

    // Standard 6502 (Subset)
    lda #$12
    lda $1234
    lda $1234,x
    lda ($12,x)
    lda ($12),y
    nop
    nop

    // Illegal Mnemonics from Table A.4
    sha ($12),y
    ahx ($12),y
    ahx $1234,y
    
    alr #$12
    asr #$4b // alr and asr share $4b
    
    anc #$0b
    anc2 #$2b
    
    arr #$6b
    
    axs #$cb
    sbx #$cb // axs and sbx share $cb
    
    dcp $c7
    dcm $c7
    dcp $d7,x
    dcp $cf
    dcp $df,x
    dcp $db,y
    dcp ($c3,x)
    dcp ($d3),y
    
    isc $e7
    ins $e7
    isb $e7
    isc $f7,x
    isc $ef
    isc $ff,x
    isc $fb,y
    isc ($e3,x)
    isc ($f3),y
    
    las $1234,y
    lae $1234,y
    lds $1234,y
    
    lax #$ab
    lxa #$ab
    lax $a7
    lax $b7,y
    lax $af
    lax $bf,y
    lax ($a3,x)
    lax ($b3),y
    
    rla $27
    rla $37,x
    rla $2f
    rla $3f,x
    rla $3b,y
    rla ($23,x)
    rla ($33),y
    
    rra $67
    rra $77,x
    rra $6f
    rra $7f,x
    rra $7b,y
    rra ($63,x)
    rra ($73),y
    
    sax $87
    sax $97,y
    sax $8f
    sax ($83,x)
    
    sbc2 #$eb
    
    shx $1234,y
    shy $1234,x
    
    slo $07
    slo $17,x
    slo $0f
    slo $1f,x
    slo $1b,y
    slo ($03,x)
    slo ($13),y
    
    sre $47
    sre $57,x
    sre $4f
    sre $5f,x
    sre $5b,y
    sre ($43,x)
    sre ($53),y
    
    tas $1234,y
    shs $1234,y
    
    xaa #$8b
    ane #$8b

    rts
