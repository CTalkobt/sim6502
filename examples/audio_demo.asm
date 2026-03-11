.target c64
; audio_demo.asm
;
; A simple program to verify SID audio output.
; This sets up Voice 1 to play a continuous A-4 (approx 440Hz) tone.
;
; Usage (GUI):
;   1. Load into sim6502-gui
;   2. Press Run (F5)
;   3. You should hear a steady tone.
;   4. Open View -> I/O Devices to see the registered SID chips.
;
; Memory layout:
;   $0200  Program code
;   $D400  SID registers

.org $0200

start:
    ; 1. Set Master Volume to Maximum
    LDA #$0F
    STA $D418

    ; 2. Set Envelope (Voice 1)
    ; Attack = 0, Decay = 0
    LDA #$00
    STA $D405
    ; Sustain = 15, Release = 0 (Full sustain)
    LDA #$F0
    STA $D406

    ; 3. Set Frequency for A-4
    ; Approx 440Hz on PAL C64.
    ; In Phase 3 implementation, any non-zero frequency triggers 
    ; the verification pulse oscillator.
    LDA #$00
    STA $D400 ; Freq Lo
    LDA #$20
    STA $D401 ; Freq Hi (Approx 1.2 kHz for better audibility)

    ; 4. Trigger Waveform + Gate
    ; Bit 4 = Triangle, Bit 0 = Gate
    LDA #$11
    STA $D404

    ; 5. Loop forever to keep the tone playing
loop:
    JMP loop
