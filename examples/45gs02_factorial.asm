.cpu _45gs02
* = $0200

// Calculate N! where N! fits in 32 bits (N <= 12).
// Result is returned in Q register: A=byte0(LSB), X=byte1, Y=byte2, Z=byte3(MSB).
.label N_VALUE = 12

// MEGA65 Math Coprocessor Registers
.label MULTINA_0 = $D770   // 32-bit multiplicand A (input)
.label MULTINA_1 = $D771
.label MULTINA_2 = $D772
.label MULTINA_3 = $D773

.label MULTINB_0 = $D774   // 32-bit multiplicand B (input)
.label MULTINB_1 = $D775
.label MULTINB_2 = $D776
.label MULTINB_3 = $D777

.label MULTOUT_0 = $D778   // 64-bit product output (low 32 bits used)
.label MULTOUT_1 = $D779
.label MULTOUT_2 = $D77A
.label MULTOUT_3 = $D77B

// Zero Page layout (B=0)
.label COUNTER_LO = $10    // 16-bit loop counter (N down to 2), little-endian
.label COUNTER_HI = $11
.label ITER_COUNT = $12    // number of factors pushed to stack

start:
    // Stack setup
    ldx #$FF
    txs

    // Initialise counter = N, ITER_COUNT = 0
    lda #N_VALUE
    sta COUNTER_LO
    lda #0
    sta COUNTER_HI
    sta ITER_COUNT

    // Push each factor N, N-1, ..., 2 onto the stack as 16-bit words.
    // PHW pushes high byte then low byte; PLA will retrieve low byte first.
push_loop:
    lda COUNTER_LO          // 16-bit compare: COUNTER < 2 ?
    cmp #2
    lda COUNTER_HI
    sbc #0
    bcc start_calc          // branch when COUNTER < 2 (done pushing)
    phw COUNTER_LO          // push 16-bit word
    inc ITER_COUNT          // track number of factors (does not affect A,X,Y,Z)
    dew COUNTER_LO          // decrement 16-bit counter (does not affect A,X,Y,Z)
    bra push_loop

start_calc:
    // Handle trivial cases (N=0 or N=1): result is 1
    lda ITER_COUNT
    beq return_one

    // Initialise 32-bit result Q = 1
    lda #1
    ldx #0
    ldy #0
    ldz #0

    // Multiply running result by each factor popped from the stack.
    // NOTE: TSX is NOT used here — it would corrupt X (byte 1 of Q).
    //       ITER_COUNT drives the loop instead.
calc_loop:
    stq MULTINA_0           // store current Q (32-bit) into MULTINA
    pla
    sta MULTINB_0           // low byte of factor (PHW pushed high-then-low)
    pla
    sta MULTINB_1           // high byte of factor
    lda #0
    sta MULTINB_2           // zero the upper 16 bits of MULTINB
    sta MULTINB_3
    ldq MULTOUT_0           // load 32-bit product into Q
    dec ITER_COUNT          // decrement counter (does not affect A,X,Y,Z)
    bne calc_loop

done:
    // EXPECT: A=00 X=FC Y=8C Z=1C B=00 PC=024A  (12! = 479,001,600 = $1C8CFC00)
    rts

return_one:
    lda #1
    ldx #0
    ldy #0
    ldz #0
    rts
