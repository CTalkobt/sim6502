#include "patterns.h"
#include <string.h>

/* =========================================================================
 * add16
 * ========================================================================= */
static const char s_add16[] =
"// add16 — 16-bit addition\n"
"//\n"
"// Adds a 16-bit source value to a 16-bit destination, both in zero page.\n"
"//\n"
".label SRC_LO  = $10\n"
".label SRC_HI  = $11\n"
".label DST_LO  = $12\n"
".label DST_HI  = $13\n"
"\n"
"add16:\n"
"    clc\n"
"    lda DST_LO\n"
"    adc SRC_LO\n"
"    sta DST_LO\n"
"    lda DST_HI\n"
"    adc SRC_HI\n"
"    sta DST_HI\n"
"    rts\n";

/* =========================================================================
 * sub16
 * ========================================================================= */
static const char s_sub16[] =
"// sub16 — 16-bit subtraction\n"
"//\n"
"// Subtracts a 16-bit source from a 16-bit destination, both in zero page.\n"
"//\n"
".label SRC_LO  = $10\n"
".label SRC_HI  = $11\n"
".label DST_LO  = $12\n"
".label DST_HI  = $13\n"
"\n"
"sub16:\n"
"    sec\n"
"    lda DST_LO\n"
"    sbc SRC_LO\n"
"    sta DST_LO\n"
"    lda DST_HI\n"
"    sbc SRC_HI\n"
"    sta DST_HI\n"
"    rts\n";

/* =========================================================================
 * mul8  — software 8×8 = 16-bit (shift-and-add), all 6502 variants
 * ========================================================================= */
static const char s_mul8[] =
"// mul8 — unsigned 8x8 = 16-bit multiply (software, all 6502 variants)\n"
"//\n"
".label FACTOR_A  = $10\n"
".label FACTOR_B  = $11\n"
".label RESULT_LO = $12\n"
".label RESULT_HI = $13\n"
".label TMP_HI    = $14\n"
"\n"
"mul8:\n"
"    lda #0\n"
"    sta RESULT_LO\n"
"    sta RESULT_HI\n"
"    sta TMP_HI\n"
"    ldx #8              // 8 bit-iterations\n"
"mul8_loop:\n"
"    lsr FACTOR_B        // shift multiplier right; bit 0 -> carry\n"
"    bcc mul8_skip       // bit was 0: skip add\n"
"    clc\n"
"    lda RESULT_LO\n"
"    adc FACTOR_A\n"
"    sta RESULT_LO\n"
"    lda RESULT_HI\n"
"    adc TMP_HI\n"
"    sta RESULT_HI\n"
"mul8_skip:\n"
"    asl FACTOR_A        // shift multiplicand left (16-bit)\n"
"    rol TMP_HI\n"
"    dex\n"
"    bne mul8_loop\n"
"    rts\n";

/* =========================================================================
 * mul8_mega65 — hardware 8×8 multiply using MEGA65 math coprocessor
 * ========================================================================= */
static const char s_mul8_mega65[] =
"// mul8_mega65 — 8x8 = 16-bit multiply using MEGA65 hardware math unit\n"
"// MEGA65 registers: MULTINA=$D770-$D773 (32-bit), MULTINB=$D774-$D777 (32-bit)\n"
"// MULTOUT=$D778-$D77F (64-bit product, updated on any write to $D770-$D777)\n"
"//\n"
".label FACTOR_A     = $10\n"
".label FACTOR_B     = $11\n"
".label RESULT_LO    = $12\n"
".label RESULT_HI    = $13\n"
"\n"
".label MULTINA_0 = $D770   // MULTINA byte 0 (LSB)\n"
".label MULTINA_1 = $D771   // MULTINA byte 1 (zero for 8-bit)\n"
".label MULTINA_2 = $D772   // MULTINA byte 2 (zero for 8-bit)\n"
".label MULTINA_3 = $D773   // MULTINA byte 3 (zero for 8-bit)\n"
".label MULTINB_0 = $D774   // MULTINB byte 0 (LSB)\n"
".label MULTINB_1 = $D775   // MULTINB byte 1 (zero for 8-bit)\n"
".label MULTINB_2 = $D776   // MULTINB byte 2 (zero for 8-bit)\n"
".label MULTINB_3 = $D777   // MULTINB byte 3 (zero for 8-bit)\n"
".label MULTOUT_0 = $D778   // result byte 0 (LSB)\n"
".label MULTOUT_1 = $D779   // result byte 1\n"
"\n"
"mul8_mega65:\n"
"    lda #0\n"
"    sta MULTINA_1       // zero high bytes of A\n"
"    sta MULTINA_2\n"
"    sta MULTINA_3\n"
"    sta MULTINB_1       // zero high bytes of B\n"
"    sta MULTINB_2\n"
"    sta MULTINB_3\n"
"    lda FACTOR_A\n"
"    sta MULTINA_0       // write A\n"
"    lda FACTOR_B\n"
"    sta MULTINB_0       // write B; product updated immediately\n"
"    lda MULTOUT_0\n"
"    sta RESULT_LO\n"
"    lda MULTOUT_1\n"
"    sta RESULT_HI\n"
"    rts\n";

/* =========================================================================
 * div16 — software 16÷8 = 16-bit quotient + 8-bit remainder
 * ========================================================================= */
static const char s_div16[] =
"// div16 — unsigned 16÷8 division (software, all 6502 variants)\n"
"//\n"
".label DIVIDEND_LO = $10\n"
".label DIVIDEND_HI = $11\n"
".label DIVISOR     = $12\n"
".label REMAINDER   = $13\n"
"\n"
"div16:\n"
"    lda #0\n"
"    sta REMAINDER\n"
"    ldx #16             // 16 bit-iterations\n"
"div16_loop:\n"
"    asl DIVIDEND_LO     // shift dividend/quotient left (carry <- bit 15)\n"
"    rol DIVIDEND_HI\n"
"    rol REMAINDER       // shift carry into remainder\n"
"    sec\n"
"    lda REMAINDER\n"
"    sbc DIVISOR\n"
"    bcc div16_no_sub    // remainder < divisor: restore\n"
"    sta REMAINDER       // keep subtracted remainder\n"
"    inc DIVIDEND_LO     // set quotient bit (bit 0 was just shifted in as 0)\n"
"div16_no_sub:\n"
"    dex\n"
"    bne div16_loop\n"
"    rts\n";

/* =========================================================================
 * div16_mega65 — hardware 16÷16 using MEGA65 math coprocessor
 * ========================================================================= */
static const char s_div16_mega65[] =
"// div16_mega65 — 16÷16 division using MEGA65 hardware math unit\n"
"//\n"
".label DIVIDEND_LO   = $10\n"
".label DIVIDEND_HI   = $11\n"
".label DIVISOR_LO    = $12\n"
".label DIVISOR_HI    = $13\n"
".label RESULT_LO     = $14\n"
".label RESULT_HI     = $15\n"
".label REMAINDER     = $16\n"
"\n"
".label MULTINA_0 = $D770   // dividend byte 0 (LSB)\n"
".label MULTINA_1 = $D771   // dividend byte 1\n"
".label MULTINA_2 = $D772   // dividend byte 2 (zero for 16-bit)\n"
".label MULTINA_3 = $D773   // dividend byte 3 (zero for 16-bit)\n"
".label MULTINB_0 = $D774   // divisor byte 0 (LSB)\n"
".label MULTINB_1 = $D775   // divisor byte 1\n"
".label MULTINB_2 = $D776   // divisor byte 2 (zero for 16-bit)\n"
".label MULTINB_3 = $D777   // divisor byte 3 (zero for 16-bit)\n"
".label DIVREM_0  = $D768   // remainder byte 0 (LSB)\n"
".label DIVREM_1  = $D769   // remainder byte 1\n"
".label DIVOUT_0  = $D76C   // quotient byte 0 (LSB)\n"
".label DIVOUT_1  = $D76D   // quotient byte 1\n"
"\n"
"div16_mega65:\n"
"    lda #0\n"
"    sta MULTINA_2       // zero high bytes of dividend\n"
"    sta MULTINA_3\n"
"    sta MULTINB_2       // zero high bytes of divisor\n"
"    sta MULTINB_3\n"
"    lda DIVIDEND_LO\n"
"    sta MULTINA_0\n"
"    lda DIVIDEND_HI\n"
"    sta MULTINA_1\n"
"    lda DIVISOR_LO\n"
"    sta MULTINB_0\n"
"    lda DIVISOR_HI\n"
"    sta MULTINB_1       // write triggers division\n"
"div16_mega65_wait:\n"
"    bit $D70F           // bit 7 = DIVBUSY\n"
"    bmi div16_mega65_wait\n"
"    lda DIVOUT_0\n"
"    sta RESULT_LO\n"
"    lda DIVOUT_1\n"
"    sta RESULT_HI\n"
"    lda DIVREM_0\n"
"    sta REMAINDER\n"
"    rts\n";

/* =========================================================================
 * bin_to_bcd — 8-bit binary to 3-digit BCD (subtraction loop, all 6502)
 * ========================================================================= */
static const char s_bin_to_bcd[] =
"// bin_to_bcd — convert 8-bit binary (0..255) to 3 decimal digits\n"
".label INPUT    = $10\n"
".label HUNDREDS = $11\n"
".label TENS     = $12\n"
".label UNITS    = $13\n"
"\n"
"bin_to_bcd:\n"
"    ldx #0\n"
"    ldy #0\n"
"    lda INPUT\n"
"bcd_100s:\n"
"    cmp #100\n"
"    bcc bcd_10s\n"
"    sbc #100\n"
"    inx\n"
"    jmp bcd_100s\n"
"bcd_10s:\n"
"    cmp #10\n"
"    bcc bcd_done\n"
"    sbc #10\n"
"    iny\n"
"    jmp bcd_10s\n"
"bcd_done:\n"
"    sta UNITS\n"
"    stx HUNDREDS\n"
"    sty TENS\n"
"    rts\n";

/* =========================================================================
 * bin_to_bcd_65c02 — 65C02 optimised: STZ, BRA, reliable decimal flags
 * ========================================================================= */
static const char s_bin_to_bcd_65c02[] =
"// bin_to_bcd_65c02 — 8-bit binary to 3-digit BCD (65C02 optimised)\n"
".label INPUT    = $10\n"
".label HUNDREDS = $11\n"
".label TENS     = $12\n"
".label UNITS    = $13\n"
"\n"
"bin_to_bcd_65c02:\n"
"    ldx #0\n"
"    ldy #0\n"
"    lda INPUT\n"
"bcd_100s:\n"
"    cmp #100\n"
"    bcc bcd_10s\n"
"    sbc #100\n"
"    inx\n"
"    bra bcd_100s\n"
"bcd_10s:\n"
"    cmp #10\n"
"    bcc bcd_done\n"
"    sbc #10\n"
"    iny\n"
"    bra bcd_10s\n"
"bcd_done:\n"
"    sta UNITS\n"
"    stx HUNDREDS\n"
"    sty TENS\n"
"    rts\n";

/* =========================================================================
 * bcd_to_bin — 2-digit packed BCD to 8-bit binary
 * ========================================================================= */
static const char s_bcd_to_bin[] =
"// bcd_to_bin — convert 2 BCD digits to 8-bit binary\n"
".label TENS   = $10\n"
".label UNITS  = $11\n"
".label RESULT = $12\n"
"\n"
"bcd_to_bin:\n"
"    lda TENS\n"
"    asl\n"
"    sta RESULT\n"
"    asl\n"
"    asl\n"
"    clc\n"
"    adc RESULT\n"
"    adc UNITS\n"
"    sta RESULT\n"
"    rts\n";

/* =========================================================================
 * str_out — print null-terminated string via C64 KERNAL CHROUT
 * ========================================================================= */
static const char s_str_out[] =
"// str_out — print a null-terminated PETSCII string via C64 KERNAL\n"
".label STR_LO = $10\n"
".label STR_HI = $11\n"
".label CHROUT = $FFD2\n"
"\n"
"str_out:\n"
"    ldy #0\n"
"str_loop:\n"
"    lda (STR_LO),y\n"
"    beq str_done\n"
"    jsr CHROUT\n"
"    iny\n"
"    bne str_loop\n"
"str_done:\n"
"    rts\n";

/* =========================================================================
 * memcopy — copy N bytes (up to 255) from source to destination
 * ========================================================================= */
static const char s_memcopy[] =
"// memcopy — copy N bytes (1..255) from source to destination\n"
".label SRC_LO = $10\n"
".label SRC_HI = $11\n"
".label DST_LO = $12\n"
".label DST_HI = $13\n"
".label COUNT  = $14\n"
"\n"
"memcopy:\n"
"    lda COUNT\n"
"    beq mc_done\n"
"    tax\n"
"    ldy #0\n"
"mc_loop:\n"
"    lda (SRC_LO),y\n"
"    sta (DST_LO),y\n"
"    iny\n"
"    dex\n"
"    bne mc_loop\n"
"mc_done:\n"
"    rts\n";

/* =========================================================================
 * memfill — fill N bytes with a constant value
 * ========================================================================= */
static const char s_memfill[] =
"// memfill — fill N bytes (1..255) with a constant value\n"
".label DST_LO = $10\n"
".label DST_HI = $11\n"
".label VALUE  = $12\n"
".label COUNT  = $13\n"
"\n"
"memfill:\n"
"    lda COUNT\n"
"    beq mf_done\n"
"    tax\n"
"    lda VALUE\n"
"    ldy #0\n"
"mf_loop:\n"
"    sta (DST_LO),y\n"
"    iny\n"
"    dex\n"
"    bne mf_loop\n"
"mf_done:\n"
"    rts\n";

/* =========================================================================
 * delay — busy-wait delay using two nested countdown loops
 * ========================================================================= */
static const char s_delay[] =
"// delay — busy-wait delay via nested countdown loops\n"
".label OUTER = $10\n"
".label INNER = $11\n"
"\n"
"delay:\n"
"    ldy OUTER\n"
"delay_outer:\n"
"    ldx INNER\n"
"delay_inner:\n"
"    dex\n"
"    bne delay_inner\n"
"    dey\n"
"    bne delay_outer\n"
"    rts\n";

/* =========================================================================
 * bitmap_init — initialise C64 / MEGA65 hi-res 1bpp bitmap mode
 * ========================================================================= */
static const char s_bitmap_init[] =
"// bitmap_init — initialise C64-compatible hi-res 1bpp bitmap mode\n"
"bitmap_init:\n"
"    lda #$03\n"
"    sta $DD02\n"
"    sta $DD00\n"
"    lda #$3B\n"
"    sta $D011\n"
"    lda #$C8\n"
"    sta $D016\n"
"    lda #$18\n"
"    sta $D018\n"
"    lda #$00\n"
"    sta $D020\n"
"    sta $D021\n"
"    lda #$10\n"
"    ldx #$00\n"
"bi_fill:\n"
"    sta $0400,x\n"
"    sta $0500,x\n"
"    sta $0600,x\n"
"    inx\n"
"    bne bi_fill\n"
"    ldx #$00\n"
"bi_tail:\n"
"    sta $0700,x\n"
"    inx\n"
"    cpx #$E8\n"
"    bne bi_tail\n"
"    rts\n";

/* =========================================================================
 * bitmap_plot — set, clear, or toggle a pixel in the 1bpp bitmap
 * ========================================================================= */
static const char s_bitmap_plot[] =
"// bitmap_plot — set/clear/toggle one pixel in the 320×200 1bpp bitmap\n"
".label PX       = $14\n"
".label PY       = $15\n"
".label BADDR_LO = $16\n"
".label BADDR_HI = $17\n"
".label BMASK    = $18\n"
".label PLOT_OP  = $19\n"
"\n"
".label ROW_LO_TBL = $4000\n"
".label ROW_HI_TBL = $4019\n"
".label BMASK_TBL  = $4032\n"
"\n"
"bitmap_plot:\n"
"    lda PY\n"
"    lsr\n"
"    lsr\n"
"    lsr\n"
"    tax\n"
"    lda ROW_LO_TBL,x\n"
"    sta BADDR_LO\n"
"    lda ROW_HI_TBL,x\n"
"    sta BADDR_HI\n"
"    lda PX\n"
"    and #$F8\n"
"    clc\n"
"    adc BADDR_LO\n"
"    sta BADDR_LO\n"
"    bcc bplot_nc1\n"
"    inc BADDR_HI\n"
"bplot_nc1:\n"
"    lda PY\n"
"    and #$07\n"
"    clc\n"
"    adc BADDR_LO\n"
"    sta BADDR_LO\n"
"    bcc bplot_nc2\n"
"    inc BADDR_HI\n"
"bplot_nc2:\n"
"    lda PX\n"
"    and #$07\n"
"    tax\n"
"    lda BMASK_TBL,x\n"
"    sta BMASK\n"
"    ldy #0\n"
"    lda PLOT_OP\n"
"    bne bplot_not_clear\n"
"    lda BMASK\n"
"    eor #$FF\n"
"    and (BADDR_LO),y\n"
"    sta (BADDR_LO),y\n"
"    rts\n"
"bplot_not_clear:\n"
"    cmp #2\n"
"    bne bplot_set\n"
"    lda BMASK\n"
"    eor (BADDR_LO),y\n"
"    sta (BADDR_LO),y\n"
"    rts\n"
"bplot_set:\n"
"    lda BMASK\n"
"    ora (BADDR_LO),y\n"
"    sta (BADDR_LO),y\n"
"    rts\n"
"\n"
"* = $4000\n"
"row_base_lo:\n"
".byte $00,$40,$80,$C0,$00,$40,$80,$C0,$00,$40,$80,$C0,$00\n"
".byte $40,$80,$C0,$00,$40,$80,$C0,$00,$40,$80,$C0,$00\n"
"row_base_hi:\n"
".byte $20,$21,$22,$23,$25,$26,$27,$28,$2A,$2B,$2C,$2D,$2F\n"
".byte $30,$31,$32,$34,$35,$36,$37,$39,$3A,$3B,$3C,$3E\n"
"bitmask_table:\n"
".byte $80,$40,$20,$10,$08,$04,$02,$01\n";

/* =========================================================================
 * bitmap_setcell — set 8×8 cell foreground / background colour
 * ========================================================================= */
static const char s_bitmap_setcell[] =
"// bitmap_setcell — set the colour of an 8x8 cell in screen RAM\n"
".label CELL_X = $10\n"
".label CELL_Y = $11\n"
".label FG_COL = $12\n"
".label BG_COL = $13\n"
".label ROW_LO = $20\n"
".label ROW_HI = $21\n"
".label TEMP_R = $22\n"
"\n"
"bitmap_setcell:\n"
"    lda CELL_Y\n"
"    lsr\n"
"    lsr\n"
"    lsr\n"
"    sta TEMP_R\n"
"    lda CELL_Y\n"
"    and #$07\n"
"    asl\n"
"    asl\n"
"    asl\n"
"    asl\n"
"    asl\n"
"    sta ROW_LO\n"
"    lda CELL_Y\n"
"    asl\n"
"    asl\n"
"    asl\n"
"    clc\n"
"    adc ROW_LO\n"
"    sta ROW_LO\n"
"    lda TEMP_R\n"
"    adc #0\n"
"    sta ROW_HI\n"
"    lda CELL_X\n"
"    clc\n"
"    adc ROW_LO\n"
"    sta ROW_LO\n"
"    bcc bsc_ok\n"
"    inc ROW_HI\n"
"bsc_ok:\n"
"    lda #$04\n"
"    clc\n"
"    adc ROW_HI\n"
"    sta ROW_HI\n"
"    lda FG_COL\n"
"    asl\n"
"    asl\n"
"    asl\n"
"    asl\n"
"    ora BG_COL\n"
"    ldy #0\n"
"    sta (ROW_LO),y\n"
"    rts\n";

/* =========================================================================
 * bitmap_clear — zero all 8000 bytes of the 320×200 bitmap (software)
 * ========================================================================= */
static const char s_bitmap_clear[] =
"// bitmap_clear — zero the 320×200 hi-res bitmap (software loop)\n"
".label PTR_LO = $10\n"
".label PTR_HI = $11\n"
"\n"
"bitmap_clear:\n"
"    lda #$00\n"
"    sta PTR_LO\n"
"    lda #$20\n"
"    sta PTR_HI\n"
"    lda #0\n"
"    ldx #31\n"
"bclr_pages:\n"
"    ldy #0\n"
"bclr_page:\n"
"    sta (PTR_LO),y\n"
"    iny\n"
"    bne bclr_page\n"
"    inc PTR_HI\n"
"    dex\n"
"    bne bclr_pages\n"
"    ldy #63\n"
"bclr_last:\n"
"    sta (PTR_LO),y\n"
"    dey\n"
"    bpl bclr_last\n"
"    rts\n";

/* =========================================================================
 * bitmap_clear_mega65 — zero the bitmap in one DMA fill (~8000 cycles)
 * ========================================================================= */
static const char s_bitmap_clear_mega65[] =
"// bitmap_clear_mega65 — zero the 320×200 bitmap using MEGA65 enhanced DMA\n"
".label DMA_ADDRHI   = $D701\n"
".label DMA_ADDRMID  = $D702\n"
".label DMA_ADDRMBHI = $D704\n"
".label DMA_TRIGGER  = $D705\n"
"\n"
"bitmap_clear_mega65:\n"
"    lda #$1F\n"
"    sta DMA_ADDRHI\n"
"    lda #$00\n"
"    sta DMA_ADDRMID\n"
"    sta DMA_ADDRMBHI\n"
"    lda #$D0\n"
"    sta DMA_TRIGGER\n"
"    rts\n"
"\n"
"* = $1FD0\n"
"bm_clr_job:\n"
"    .byte $0B, $00\n"
"    .byte $03\n"
"    .byte $40, $1F\n"
"    .byte $00\n"
"    .byte $00, $00\n"
"    .byte $00\n"
"    .byte $20\n"
"    .byte $00\n"
"    .byte $00\n"
"    .byte $00, $00\n";

/* =========================================================================
 * dma_copy — bulk copy using MEGA65 enhanced DMA ($D705 trigger)
 * ========================================================================= */
static const char s_dma_copy[] =
"// dma_copy — copy N bytes using MEGA65 enhanced DMA\n"
".label SRC_LO = $10\n"
".label SRC_HI = $11\n"
".label DST_LO = $12\n"
".label DST_HI = $13\n"
".label CNT_LO = $14\n"
".label CNT_HI = $15\n"
"\n"
".label JOB_BASE     = $1FF0\n"
".label DMA_ADDRHI   = $D701\n"
".label DMA_ADDRMID  = $D702\n"
".label DMA_ADDRMBHI = $D704\n"
".label DMA_TRIGGER  = $D705\n"
"\n"
"dma_copy:\n"
"    lda #$0B\n"
"    sta $1FF0\n"
"    lda #$00\n"
"    sta $1FF1\n"
"    sta $1FF2\n"
"    lda CNT_LO\n"
"    sta $1FF3\n"
"    lda CNT_HI\n"
"    sta $1FF4\n"
"    lda SRC_LO\n"
"    sta $1FF5\n"
"    lda SRC_HI\n"
"    sta $1FF6\n"
"    lda #$00\n"
"    sta $1FF7\n"
"    lda DST_LO\n"
"    sta $1FF8\n"
"    lda DST_HI\n"
"    sta $1FF9\n"
"    lda #$00\n"
"    sta $1FFA\n"
"    sta $1FFB\n"
"    sta $1FFC\n"
"    sta $1FFD\n"
"    lda #$1F\n"
"    sta DMA_ADDRHI\n"
"    lda #$00\n"
"    sta DMA_ADDRMID\n"
"    sta DMA_ADDRMBHI\n"
"    lda #$F0\n"
"    sta DMA_TRIGGER\n"
"    rts\n";

/* =========================================================================
 * dma_fill — fill N bytes with a constant using MEGA65 enhanced DMA
 * ========================================================================= */
static const char s_dma_fill[] =
"// dma_fill — fill N bytes with a constant using MEGA65 enhanced DMA\n"
".label FILL_VAL = $10\n"
".label DST_LO   = $11\n"
".label DST_HI   = $12\n"
".label CNT_LO   = $13\n"
".label CNT_HI   = $14\n"
"\n"
".label JOB_BASE     = $1FE0\n"
".label DMA_ADDRHI   = $D701\n"
".label DMA_ADDRMID  = $D702\n"
".label DMA_ADDRMBHI = $D704\n"
".label DMA_TRIGGER  = $D705\n"
"\n"
"dma_fill:\n"
"    lda #$0B\n"
"    sta $1FE0\n"
"    lda #$00\n"
"    sta $1FE1\n"
"    lda #$03\n"
"    sta $1FE2\n"
"    lda CNT_LO\n"
"    sta $1FE3\n"
"    lda CNT_HI\n"
"    sta $1FE4\n"
"    lda FILL_VAL\n"
"    sta $1FE5\n"
"    lda #$00\n"
"    sta $1FE6\n"
"    sta $1FE7\n"
"    lda DST_LO\n"
"    sta $1FE8\n"
"    lda DST_HI\n"
"    sta $1FE9\n"
"    lda #$00\n"
"    sta $1FEA\n"
"    sta $1FEB\n"
"    sta $1FEC\n"
"    sta $1FED\n"
"    lda #$1F\n"
"    sta DMA_ADDRHI\n"
"    lda #$00\n"
"    sta DMA_ADDRMID\n"
"    sta DMA_ADDRMBHI\n"
"    lda #$E0\n"
"    sta DMA_TRIGGER\n"
"    rts\n";

/* =========================================================================
 * dma_copy_far — 28-bit copy using MEGA65 DMA Memory Block (MB) tokens
 * ========================================================================= */
static const char s_dma_copy_far[] =
"// dma_copy_far — copy between 28-bit MEGA65 addresses using DMA\n"
".label DMA_ADDRHI   = $D701\n"
".label DMA_ADDRMID  = $D702\n"
".label DMA_ADDRMBHI = $D704\n"
".label DMA_TRIGGER  = $D705\n"
"\n"
"dma_copy_far:\n"
"    lda #$1F\n"
"    sta DMA_ADDRHI\n"
"    lda #$00\n"
"    sta DMA_ADDRMID\n"
"    sta DMA_ADDRMBHI\n"
"    lda #$B0\n"
"    sta DMA_TRIGGER\n"
"    rts\n"
"\n"
"* = $1FB0\n"
"dma_far_job:\n"
"    .byte $0B\n"
"    .byte $80, $08\n"
"    .byte $81, $00\n"
"    .byte $00\n"
"    .byte $00\n"
"    .byte $00, $01\n"
"    .byte $00\n"
"    .byte $00\n"
"    .byte $00\n"
"    .byte $00\n"
"    .byte $00\n"
"    .byte $00\n"
"    .byte $00\n"
"    .byte $00\n"
"    .byte $00\n";

/* =========================================================================
 * sprite_init
 * ========================================================================= */
static const char s_sprite_init[] =
"// sprite_init — full VIC-II sprite initialisation\n"
".label SPRITE_NUM = $10\n"
".label SPR_X_LO   = $11\n"
".label SPR_X_HI   = $12\n"
".label SPR_Y      = $13\n"
".label SPR_COLOR  = $14\n"
".label SPR_PTR    = $15\n"
".label BIT_MASK   = $16\n"
"\n"
"sprite_init:\n"
"    lda #1\n"
"    ldx SPRITE_NUM\n"
"    beq si_got_mask\n"
"si_shift:\n"
"    asl\n"
"    dex\n"
"    bne si_shift\n"
"si_got_mask:\n"
"    sta BIT_MASK\n"
"    lda SPRITE_NUM\n"
"    asl\n"
"    tax\n"
"    lda SPR_X_LO\n"
"    sta $D000,x\n"
"    lda SPR_Y\n"
"    sta $D001,x\n"
"    lda SPR_X_HI\n"
"    and #$01\n"
"    beq si_x_clear\n"
"    lda $D010\n"
"    ora BIT_MASK\n"
"    bne si_x_done\n"
"si_x_clear:\n"
"    lda BIT_MASK\n"
"    eor #$FF\n"
"    and $D010\n"
"si_x_done:\n"
"    sta $D010\n"
"    lda $D015\n"
"    ora BIT_MASK\n"
"    sta $D015\n"
"    ldx SPRITE_NUM\n"
"    lda SPR_COLOR\n"
"    sta $D027,x\n"
"    lda SPR_PTR\n"
"    sta $07F8,x\n"
"    rts\n";

/* =========================================================================
 * sprite_move
 * ========================================================================= */
static const char s_sprite_move[] =
"// sprite_move — update VIC-II sprite position (X + Y + X MSB)\n"
".label SPRITE_NUM = $10\n"
".label SPR_X_LO   = $11\n"
".label SPR_X_HI   = $12\n"
".label SPR_Y      = $13\n"
".label BIT_MASK   = $14\n"
"\n"
"sprite_move:\n"
"    lda #1\n"
"    ldx SPRITE_NUM\n"
"    beq smv_got_mask\n"
"smv_shift:\n"
"    asl\n"
"    dex\n"
"    bne smv_shift\n"
"smv_got_mask:\n"
"    sta BIT_MASK\n"
"    lda SPRITE_NUM\n"
"    asl\n"
"    tax\n"
"    lda SPR_X_LO\n"
"    sta $D000,x\n"
"    lda SPR_Y\n"
"    sta $D001,x\n"
"    lda SPR_X_HI\n"
"    and #$01\n"
"    beq smv_x_clear\n"
"    lda $D010\n"
"    ora BIT_MASK\n"
"    bne smv_x_done\n"
"smv_x_clear:\n"
"    lda BIT_MASK\n"
"    eor #$FF\n"
"    and $D010\n"
"smv_x_done:\n"
"    sta $D010\n"
"    rts\n";

/* =========================================================================
 * sprite_setcolor
 * ========================================================================= */
static const char s_sprite_setcolor[] =
"// sprite_setcolor — set a VIC-II sprite's foreground colour\n"
".label SPRITE_NUM = $10\n"
".label SPR_COLOR  = $11\n"
"\n"
"sprite_setcolor:\n"
"    ldx SPRITE_NUM\n"
"    lda SPR_COLOR\n"
"    sta $D027,x\n"
"    rts\n";

/* =========================================================================
 * sprite_mc_init
 * ========================================================================= */
static const char s_sprite_mc_init[] =
"// sprite_mc_init — enable VIC-II multicolour mode for one sprite\n"
".label SPRITE_NUM = $10\n"
".label SPR_COLOR  = $11\n"
".label MC_COLOR0  = $12\n"
".label MC_COLOR1  = $13\n"
".label BIT_MASK   = $14\n"
"\n"
"sprite_mc_init:\n"
"    lda #1\n"
"    ldx SPRITE_NUM\n"
"    beq smc_got_mask\n"
"smc_shift:\n"
"    asl\n"
"    dex\n"
"    bne smc_shift\n"
"smc_got_mask:\n"
"    sta BIT_MASK\n"
"    lda $D01C\n"
"    ora BIT_MASK\n"
"    sta $D01C\n"
"    ldx SPRITE_NUM\n"
"    lda SPR_COLOR\n"
"    sta $D027,x\n"
"    lda MC_COLOR0\n"
"    sta $D025\n"
"    lda MC_COLOR1\n"
"    sta $D026\n"
"    rts\n";

/* =========================================================================
 * sprite_data
 * ========================================================================= */
static const char s_sprite_data[] =
"// sprite_data — 64-byte diamond sprite shape\n"
"* = $2080\n"
"spr_diamond:\n"
"    .byte $00,$18,$00\n"
"    .byte $00,$3C,$00\n"
"    .byte $00,$7E,$00\n"
"    .byte $00,$FF,$00\n"
"    .byte $01,$FF,$80\n"
"    .byte $03,$FF,$C0\n"
"    .byte $07,$FF,$E0\n"
"    .byte $0F,$FF,$F0\n"
"    .byte $1F,$FF,$F8\n"
"    .byte $3F,$FF,$FC\n"
"    .byte $7F,$FF,$FE\n"
"    .byte $3F,$FF,$FC\n"
"    .byte $1F,$FF,$F8\n"
"    .byte $0F,$FF,$F0\n"
"    .byte $07,$FF,$E0\n"
"    .byte $03,$FF,$C0\n"
"    .byte $01,$FF,$80\n"
"    .byte $00,$FF,$00\n"
"    .byte $00,$7E,$00\n"
"    .byte $00,$3C,$00\n"
"    .byte $00,$18,$00\n"
"    .byte $00\n";

/* =========================================================================
 * sprite_mega65_16col
 * ========================================================================= */
static const char s_sprite_mega65_16col[] =
"// sprite_mega65_16col — enable MEGA65 VIC-IV 16-colour mode for one sprite\n"
".label SPRITE_NUM = $10\n"
"\n"
"sprite_mega65_16col:\n"
"    lda #1\n"
"    ldx SPRITE_NUM\n"
"    beq s16_got_mask\n"
"s16_shift:\n"
"    asl\n"
"    dex\n"
"    bne s16_shift\n"
"s16_got_mask:\n"
"    ora $D04B\n"
"    sta $D04B\n"
"    rts\n";

/* =========================================================================
 * Snippet table
 * ========================================================================= */
const snippet_t g_snippets[] = {
    { "add16",           "math",    "16-bit addition (zero-page)",                "6502",    NULL,            s_add16           },
    { "sub16",           "math",    "16-bit subtraction (zero-page)",             "6502",    NULL,            s_sub16           },
    { "mul8",            "math",    "8x8 = 16-bit multiply (software)",           "6502",    NULL,            s_mul8            },
    { "mul8_mega65",     "math",    "8x8 = 16-bit multiply (MEGA65 hardware)",    "45gs02",  "mega65_math",   s_mul8_mega65     },
    { "div16",           "math",    "16/8 = 16-bit quotient + 8-bit remainder",   "6502",    NULL,            s_div16           },
    { "div16_mega65",    "math",    "16/16 division (MEGA65 hardware)",           "45gs02",  "mega65_math",   s_div16_mega65    },
    { "bin_to_bcd",      "math",    "8-bit binary to 3-digit BCD",               "6502",    NULL,            s_bin_to_bcd      },
    { "bin_to_bcd_65c02","math",    "8-bit binary to 3-digit BCD (65C02 opt.)",  "65c02",   NULL,            s_bin_to_bcd_65c02},
    { "bcd_to_bin",      "math",    "2-digit BCD to 8-bit binary",               "6502",    NULL,            s_bcd_to_bin      },
    { "str_out",         "io",      "Print null-terminated string (C64 KERNAL)", "6502",    NULL,            s_str_out         },
    { "memcopy",         "memory",  "Copy N bytes (forward, non-overlapping)",   "6502",    NULL,            s_memcopy         },
    { "memfill",         "memory",  "Fill N bytes with a constant value",        "6502",    NULL,            s_memfill         },
    { "delay",           "time",    "Busy-wait delay via nested loops",          "6502",    NULL,            s_delay           },
    { "bitmap_init",          "bitmap",  "Init C64/MEGA65 hi-res 1bpp bitmap mode",   "6502",    "vic2",          s_bitmap_init          },
    { "bitmap_plot",          "bitmap",  "Set/clear/toggle pixel in 1bpp bitmap",     "6502",    "vic2",          s_bitmap_plot          },
    { "bitmap_setcell",       "bitmap",  "Set 8x8 cell fg/bg colour in screen RAM",   "6502",    "vic2",          s_bitmap_setcell       },
    { "bitmap_clear",         "bitmap",  "Clear 8000-byte 1bpp bitmap (software)",    "6502",    "vic2",          s_bitmap_clear         },
    { "bitmap_clear_mega65",  "bitmap",  "Clear 1bpp bitmap via MEGA65 DMA fill",     "45gs02",  "mega65_dma",    s_bitmap_clear_mega65  },
    { "dma_copy",        "dma",     "Copy N bytes via MEGA65 enhanced DMA",      "45gs02",  "mega65_dma",    s_dma_copy        },
    { "dma_fill",        "dma",     "Fill N bytes with a constant via DMA",      "45gs02",  "mega65_dma",    s_dma_fill        },
    { "dma_copy_far",    "dma",     "28-bit copy with MB option tokens",         "45gs02",  "mega65_dma",    s_dma_copy_far    },
    { "sprite_init",         "sprite",  "Full VIC-II sprite init (pos/enable/colour/ptr)", "6502",    "vic2",          s_sprite_init         },
    { "sprite_move",         "sprite",  "Update sprite X/Y position and X MSB",           "6502",    "vic2",          s_sprite_move         },
    { "sprite_setcolor",     "sprite",  "Set sprite foreground colour ($D027+N)",          "6502",    "vic2",          s_sprite_setcolor     },
    { "sprite_mc_init",      "sprite",  "Enable VIC-II multicolour mode for one sprite",  "6502",    "vic2",          s_sprite_mc_init      },
    { "sprite_data",         "sprite",  "64-byte diamond sprite shape at $2080",           "6502",    "vic2",          s_sprite_data         },
    { "sprite_mega65_16col", "sprite",  "Enable MEGA65 VIC-IV 16-colour (4bpp) for sprite","45gs02",  "vic2",          s_sprite_mega65_16col },
};

const int g_snippet_count = (int)(sizeof(g_snippets) / sizeof(g_snippets[0]));

const snippet_t *snippet_find(const char *name) {
    for (int i = 0; i < g_snippet_count; i++) {
        if (strcmp(g_snippets[i].name, name) == 0)
            return &g_snippets[i];
    }
    return NULL;
}
