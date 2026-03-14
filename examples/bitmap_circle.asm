
// bitmap_circle.asm
//
// Demonstrates VIC-II Standard Bitmap Mode by drawing a white circle
// (radius 80, centre 160,100) on the 320x200 screen using the
// midpoint (Bresenham) circle algorithm.
//
// Memory map
//   $0200  Code (fits well within $03FF)
//   $0400  Screen RAM    (D018 VM=1  -> bank 0 + 1x1024)
//   $0800  Row base-address table  (25 low bytes @ $0800,
//                                   25 high bytes @ $0819)
//   $0832  Bitmask table           (8 bytes)
//   $2000  Bitmap data             (D018 bit3=1 -> bank 0 + $2000)
//   $DD00  CIA2 Port A             (VIC bank select)
//
// VIC-II configuration
//   CIA2 $DD00 = $03  VIC bank 0 ($0000-$3FFF)
//   D011       = $3B  DEN=1 BMM=1 RSEL=1 yscroll=3
//   D016       = $C8  MCM=0 CSEL=1 xscroll=0
//   D018       = $18  screen=$0400 (VM=1), bitmap=$2000 (bit 3=1)
//   D020/D021  = $00  border and background: black
//
// Screen RAM colour format (standard bitmap mode)
//   High nibble = foreground colour for set pixels   -> 1 = white
//   Low  nibble = background colour for clear pixels -> 0 = black
//   All 1000 cells are filled with $10.
//
// Pixel-address formula (from C64 bitmap conventions)
//   addr = $2000 + 320*(py/8) + 8*(px/8) + (py & 7)
//        = row_base[py>>3] + (px & $F8) + (py & 7)
//   bit  = $80 >> (px & 7)          MSB = leftmost pixel in byte
//
// Zero-page layout ($10-$1A)
//   $10  circ_x   circle algorithm x  (decrements from radius to 0)
//   $11  circ_y   circle algorithm y  (increments from 0 to radius)
//   $12  param_lo decision parameter, low byte
//   $13  param_hi decision parameter, high byte  ($FF=neg, $00=pos)
//   $14  px       pixel X to plot  (0-255, max 240 used)
//   $15  py       pixel Y to plot  (0-199, max 180 used)
//   $16  baddr_lo bitmap byte address, low
//   $17  baddr_hi bitmap byte address, high
//   $18  bmask    pixel bitmask
//   $19  dlo      delta accumulator, low byte
//   $1A  dhi      delta accumulator, high byte (sign extension)
//
// Usage (GUI)
//   ./sim6502-gui
//   File -> Open -> examples/bitmap_circle.asm
//   View -> VIC-II Screen
//   Click Run
//
// Usage (CLI)
//   ./sim6502 -p 6502 examples/bitmap_circle.asm

* = $0200

start:
    //------------------------------------------------------------------
    // 1. Select VIC bank 0 ($0000-$3FFF) via CIA2 Port A.
    //    Hardware inverts bits 1:0: writing %11 selects bank 0.
    //------------------------------------------------------------------
    lda #$03
    sta $DD02           // CIA2 Port A DDR: bits 0-1 as outputs
    sta $DD00           // CIA2 Port A: %11 -> bank 0

    //------------------------------------------------------------------
    // 2. Fill all 1000 screen-RAM cells ($0400-$07E7) with $10.
    //    $10 = fg colour 1 (white) on bg colour 0 (black).
    //    In standard bitmap mode: set pixels show fg, clear pixels bg.
    //------------------------------------------------------------------
    lda #$10
    ldx #$00
fill_scr:
    sta $0400,X
    sta $0500,X
    sta $0600,X
    inx
    bne fill_scr        // 3x256 = 768 bytes ($0400-$06FF)

    ldx #$00
fill_scr_tail:
    sta $0700,X
    inx
    cpx #$E8
    bne fill_scr_tail   // 232 more bytes ($0700-$07E7)

    //------------------------------------------------------------------
    // 3. Configure VIC-II for Standard Bitmap Mode.
    //------------------------------------------------------------------
    lda #$00
    sta $D020           // border colour: 0 (black)
    sta $D021           // background colour: 0 (black)
    lda #$3B            // D011: DEN=1 BMM=1 RSEL=1 yscroll=3
    sta $D011
    lda #$C8            // D016: MCM=0 CSEL=1 xscroll=0
    sta $D016
    lda #$18            // D018: screen=$0400 (VM=1), bitmap=$2000 (bit3=1)
    sta $D018

    //------------------------------------------------------------------
    // 4. Draw circle: centre (160, 100), radius 80.
    //
    //    Midpoint (Bresenham) circle algorithm:
    //      x = r, y = 0, p = 1 - r
    //      while y <= x:
    //        plot 8 symmetric octant points
    //        y++
    //        if p > 0: x--; p += 2*(y-x) + 1   (new y, new x)
    //        else:         p += 2*y     + 1
    //
    //    Initial p = 1 - 80 = -79 = $FFB1 (16-bit two's complement).
    //    All coordinates fit in 8 bits: X 80..240, Y 20..180.
    //------------------------------------------------------------------
    lda #80
    sta $10             // circ_x = radius
    lda #$00
    sta $11             // circ_y = 0
    lda #$B1            // low byte of -79  (256 - 79 = 177 = $B1)
    sta $12             // param_lo
    lda #$FF            // high byte of -79
    sta $13             // param_hi

circle_loop:
    // Continue while circ_y <= circ_x
    lda $11             // circ_y
    cmp $10             // compare to circ_x (carry set if y >= x)
    bcc circ_body       // y < x: continue drawing
    beq circ_body       // y == x: draw final octant point
    jmp circ_done       // y > x: finished

circ_body:
    //-- Point 1: (cx + x, cy + y) --
    lda #160
    clc
    adc $10
    sta $14
    lda #100
    clc
    adc $11
    sta $15
    jsr plot_pixel

    //-- Point 2: (cx + x, cy - y) --
    lda #160
    clc
    adc $10
    sta $14
    lda #100
    sec
    sbc $11
    sta $15
    jsr plot_pixel

    //-- Point 3: (cx - x, cy + y) --
    lda #160
    sec
    sbc $10
    sta $14
    lda #100
    clc
    adc $11
    sta $15
    jsr plot_pixel

    //-- Point 4: (cx - x, cy - y) --
    lda #160
    sec
    sbc $10
    sta $14
    lda #100
    sec
    sbc $11
    sta $15
    jsr plot_pixel

    //-- Point 5: (cx + y, cy + x) --
    lda #160
    clc
    adc $11
    sta $14
    lda #100
    clc
    adc $10
    sta $15
    jsr plot_pixel

    //-- Point 6: (cx + y, cy - x) --
    lda #160
    clc
    adc $11
    sta $14
    lda #100
    sec
    sbc $10
    sta $15
    jsr plot_pixel

    //-- Point 7: (cx - y, cy + x) --
    lda #160
    sec
    sbc $11
    sta $14
    lda #100
    clc
    adc $10
    sta $15
    jsr plot_pixel

    //-- Point 8: (cx - y, cy - x) --
    lda #160
    sec
    sbc $11
    sta $14
    lda #100
    sec
    sbc $10
    sta $15
    jsr plot_pixel

    // y++
    inc $11

    //-- Update decision parameter --
    //   p > 0: x--; p += (2*y + 1) - 2*x   (new y, new x; may be negative)
    //   p <= 0:     p +=  2*y + 1            (always positive)
    lda $13             // param_hi
    bmi circ_pneg       // p < 0 (high byte = $FF)
    lda $12             // param_lo
    beq circ_pneg       // p == 0, same update formula

    // p > 0: decrement x, compute signed delta = (2*y+1) - 2*x
    dec $10             // x--
    lda $11             // new y
    asl                 // 2*y   (y<=80: bit7=0, carry=0)
    clc
    adc #1              // 2*y + 1  (<=161, no overflow)
    sta $19             // dlo = 2y+1
    lda $10             // new x (after decrement)
    asl                 // 2*x   (x<=80, carry=0)
    sta $1A             // temp: 2x
    sec
    lda $19
    sbc $1A             // dlo = 2y+1 - 2x  (signed; negative when x > y)
    sta $19
    lda #$00
    sbc #$00            // sign extend: carry=1->$00, carry=0->$FF
    sta $1A             // dhi = 0 or $FF
    jmp circ_padd

circ_pneg:
    // p <= 0: delta = 2*y + 1  (positive, fits in one byte)
    lda $11             // new y
    asl                 // 2*y  (carry=0)
    clc
    adc #1              // 2*y + 1
    sta $19             // dlo
    lda #$00
    sta $1A             // dhi = 0

circ_padd:
    clc
    lda $12
    adc $19
    sta $12
    lda $13
    adc $1A
    sta $13
    jmp circle_loop

circ_done:
    rts

//======================================================================
// plot_pixel -- set one pixel in the standard bitmap
//
// Inputs   $14 (px) = pixel X, 0-255 (max 240 used here)
//          $15 (py) = pixel Y, 0-199
//
// Bitmap byte address:
//   row = py >> 3                       (range 0-24)
//   addr = row_base[row]                (from table at $0800/$0819)
//        + (px & $F8)                   (column offset = col*8)
//        + (py & 7)                     (scanline within cell)
// Bit mask:
//   bitmask_table[px & 7]               (table at $0832)
//
// Clobbers: A, X, Y, $16, $17, $18
//======================================================================
plot_pixel:
    // row = py >> 3
    lda $15
    lsr
    lsr
    lsr
    tax                 // X = row (0-24)

    // Load row base address from parallel low/high tables
    lda $0800,X         // row_base_lo[row]
    sta $16
    lda $0819,X         // row_base_hi[row]  ($0800 + 25 = $0819)
    sta $17

    // Add column byte offset: px & $F8  (= col * 8)
    lda $14
    and #$F8
    clc
    adc $16
    sta $16
    bcc ppnc1
    inc $17
ppnc1:
    // Add scanline within cell: py & 7
    lda $15
    and #$07
    clc
    adc $16
    sta $16
    bcc ppnc2
    inc $17
ppnc2:
    // Compute bitmask: $80 >> (px & 7)
    lda $14
    and #$07
    tax
    lda $0832,X         // bitmask_table[bit_pos]  ($0800 + 50 = $0832)
    sta $18

    // Read-modify-write: set the bit in the bitmap byte
    ldy #$00
    lda ($16),Y
    ora $18
    sta ($16),Y
    rts

//======================================================================
// Lookup tables
//======================================================================

// row_base_lo / row_base_hi
//   row_base[i] = $2000 + i * 320  for i = 0..24
//   Stored as 25 low bytes @ $0800, then 25 high bytes @ $0819.
//
//   i   address   lo   hi
//   0   $2000     $00  $20
//   1   $2140     $40  $21
//   2   $2280     $80  $22
//   3   $23C0     $C0  $23
//   4   $2500     $00  $25
//   5   $2640     $40  $26
//   6   $2780     $80  $27
//   7   $28C0     $C0  $28
//   8   $2A00     $00  $2A
//   9   $2B40     $40  $2B
//  10   $2C80     $80  $2C
//  11   $2DC0     $C0  $2D
//  12   $2F00     $00  $2F
//  13   $3040     $40  $30
//  14   $3180     $80  $31
//  15   $32C0     $C0  $32
//  16   $3400     $00  $34
//  17   $3540     $40  $35
//  18   $3680     $80  $36
//  19   $37C0     $C0  $37
//  20   $3900     $00  $39
//  21   $3A40     $40  $3A
//  22   $3B80     $80  $3B
//  23   $3CC0     $C0  $3C
//  24   $3E00     $00  $3E

* = $0800
row_base_lo:
.byte $00,$40,$80,$C0,$00,$40,$80,$C0,$00,$40,$80,$C0,$00
.byte $40,$80,$C0,$00,$40,$80,$C0,$00,$40,$80,$C0,$00

row_base_hi:
.byte $20,$21,$22,$23,$25,$26,$27,$28,$2A,$2B,$2C,$2D,$2F
.byte $30,$31,$32,$34,$35,$36,$37,$39,$3A,$3B,$3C,$3E

// bitmask_table[i] = $80 >> i
//   i=0 -> $80 (MSB, leftmost pixel in byte)
//   i=7 -> $01 (LSB, rightmost pixel in byte)
bitmask_table:
.byte $80,$40,$20,$10,$08,$04,$02,$01
