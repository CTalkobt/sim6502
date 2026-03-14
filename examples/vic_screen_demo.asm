
// vic_screen_demo.asm
//
// Demonstrates the VIC-II Screen pane in sim6502-gui.
//
// Fills the 40x25 display with solid-color blocks cycling through all 16
// C64 palette colors, using Standard Character Mode.
//
// Memory layout used
//   $0200        Program code
//   $0400        Screen RAM  (D018 VM=1  ->  bank_base + 1x1024)
//   $1000        Char data   (D018 CB=2  ->  bank_base + 2x2048)
//   $D800        Colour RAM
//   $DD00/$DD02  CIA2 Port A -- selects VIC bank 0 ($0000-$3FFF)
//
// Usage (GUI)
//   ./sim6502-gui
//   File -> Open -> examples/vic_screen_demo.asm
//   File -> Load Symbols -> Presets -> C64
//   View -> VIC-II Screen
//   Click Run (or Step) -- screen pane updates after each step.
//
// Usage (CLI)
//   ./sim6502 -p 6502 -S symbols/c64.sym examples/vic_screen_demo.asm

* = $0200

start:
    //------------------------------------------------------------------
    // 1. Select VIC bank 0 ($0000-$3FFF) via CIA2.
    //    Port A bits 0-1 = %11 means bank 0 (inverted by VIC hardware).
    //------------------------------------------------------------------
    lda #$03
    sta $DD02       // cia2_ddr_a  bits 0-1 outputs
    sta $DD00       // cia2_port_a output %11 = bank 0

    //------------------------------------------------------------------
    // 2. Fill screen RAM $0400-$07E7 with char code $01 (solid block).
    //    1000 cells = 3 full pages (256 each) + 232 remainder ($E8).
    //------------------------------------------------------------------
    lda #$01
    ldx #$00
fill_screen:
    sta $0400,X
    sta $0500,X
    sta $0600,X
    inx
    bne fill_screen

    ldx #$00
fill_screen_tail:
    sta $0700,X
    inx
    cpx #$E8
    bne fill_screen_tail

    //------------------------------------------------------------------
    // 3. Fill colour RAM $D800-$DBE7: color(n) = n AND $0F.
    //    Produces a repeating stripe of all 16 palette colours per row.
    //------------------------------------------------------------------
    ldx #$00
fill_colors:
    txa
    and #$0F
    sta $D800,X
    sta $D900,X
    sta $DA00,X
    inx
    bne fill_colors

    ldx #$00
fill_colors_tail:
    txa
    and #$0F
    sta $DB00,X
    inx
    cpx #$E8
    bne fill_colors_tail

    //------------------------------------------------------------------
    // 4. Set VIC-II display registers (no colons in inline comments --
    //    the assembler's label scanner uses strchr(':') on each line).
    //------------------------------------------------------------------
    lda #$00
    sta $D020       // vic_border_color  = 0 black
    sta $D021       // vic_background    = 0 black

    lda #$1B        // vic_control1  DEN on, 25 rows, BMM off, ECM off, yscroll 3
    sta $D011

    lda #$C8        // vic_control2  MCM off, 40 cols, xscroll 0
    sta $D016

    lda #$14        // vic_memory_setup  screen $0400 (VM=1), chars $1000 (CB=2)
    sta $D018

done:
    rts

// ------------------------------------------------------------------
// Character data at $1000 (char generator base for D018=$14, bank 0)
//
// Each character is 8 bytes (one per pixel row, MSB = leftmost pixel).
//
// char $00  space -- all zero
// char $01  solid block -- used to fill the screen above
// char $02  top-half block
// char $03  bottom-half block
// char $04  left-half block  (left 4 pixels per row)
// char $05  right-half block (right 4 pixels per row)
// char $06  checkerboard A
// char $07  checkerboard B
// ------------------------------------------------------------------
* = $1000
.byte $00,$00,$00,$00,$00,$00,$00,$00  // char $00  space
.byte $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF  // char $01  solid block
.byte $FF,$FF,$FF,$FF,$00,$00,$00,$00  // char $02  top-half
.byte $00,$00,$00,$00,$FF,$FF,$FF,$FF  // char $03  bottom-half
.byte $F0,$F0,$F0,$F0,$F0,$F0,$F0,$F0  // char $04  left-half
.byte $0F,$0F,$0F,$0F,$0F,$0F,$0F,$0F  // char $05  right-half
.byte $AA,$55,$AA,$55,$AA,$55,$AA,$55  // char $06  checkerboard A
.byte $55,$AA,$55,$AA,$55,$AA,$55,$AA  // char $07  checkerboard B
