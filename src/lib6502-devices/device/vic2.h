#ifndef VIC2_H
#define VIC2_H

#include <stdint.h>
#include "memory.h"

#define VIC2_FRAME_W   384  /* full PAL frame width  (32px border each side) */
#define VIC2_FRAME_H   272  /* full PAL frame height (36px border top/bottom) */
#define VIC2_ACTIVE_X   32  /* left border width (pixels) */
#define VIC2_ACTIVE_Y   36  /* top border height (pixels) */
#define VIC2_ACTIVE_W  320  /* active display width  (40 chars × 8px) */
#define VIC2_ACTIVE_H  200  /* active display height (25 rows  × 8px) */

/* C64 "Pepto" sRGB palette. Index 0–15. */
extern const uint8_t vic2_palette[16][3];

/* Human-readable colour names, index 0–15. */
extern const char *vic2_color_names[16];

/* Render a full VIC2_FRAME_W × VIC2_FRAME_H RGB frame into buf
   (border + active area + sprites).
   buf must hold at least VIC2_FRAME_W * VIC2_FRAME_H * 3 bytes. */
void vic2_render_rgb(const memory_t *mem, uint8_t *buf);

/* Render only the VIC2_ACTIVE_W × VIC2_ACTIVE_H (320×200) active display
   area into buf — no border, but sprites are included and clipped to the
   active area.  Adapts automatically to the current VIC-II mode (char /
   bitmap / ECM / MCM) read from registers.
   buf must hold at least VIC2_ACTIVE_W * VIC2_ACTIVE_H * 3 bytes. */
void vic2_render_rgb_active(const memory_t *mem, uint8_t *buf);

/* Save a full-frame PPM screenshot to filename. Returns 0 on success. */
int vic2_render_ppm(const memory_t *mem, const char *filename);

/* Save a 320×200 active-area PPM (no border; sprites included and clipped)
   to filename.  Returns 0 on success, -1 on file error. */
int vic2_render_ppm_active(const memory_t *mem, const char *filename);

/* Print a human-readable VIC-II state summary (mode, key addresses,
   border/background colours) to stdout. */
void vic2_print_info(const memory_t *mem);

/* Print a full VIC-II register dump: all control registers with decoded
   fields, raster line, interrupt status/enable, video bank, computed screen/
   charset/bitmap addresses, and all colour registers with names. */
void vic2_print_regs(const memory_t *mem);

/* Print all 8 sprite states (position, colour, flags, data address) to stdout. */
void vic2_print_sprites(const memory_t *mem);

/* Render a single sprite (0-7) to a 24x21 RGBA buffer.
   buf must be at least 24 * 21 * 4 bytes. 
   Includes transparency (alpha=0 for bg). */
void vic2_render_sprite(const memory_t *mem, int index, uint8_t *buf);
void vic2_render_char(const memory_t *mem, uint16_t char_base, int char_index, int mcm, uint8_t c0, uint8_t c1, uint8_t c2, uint8_t c3, uint8_t *buf);

/* JSON output variants — print a single-line JSON object to stdout.
   Intended for use with the -J (JSON mode) interactive CLI flag. */
void vic2_json_info(const memory_t *mem);
void vic2_json_regs(const memory_t *mem);
void vic2_json_sprites(const memory_t *mem);

/* ---------------------------------------------------------------------------
 * TODO: VIC-IV CHARPTR support (Mega65)
 *
 * The Mega65 VIC-IV exposes a 20-bit charset base pointer via three registers
 * that bypass the VIC-II D018/CIA2 bank scheme entirely:
 *
 *   $D068  CHARPTR lo  — bits  [7:0]  of the 20-bit charset base address
 *   $D069  CHARPTR mid — bits [15:8]
 *   $D06A  CHARPTR hi  — bits [19:16] (low nibble only)
 *
 * When these are implemented:
 *  1. Register map: add $D068–$D06A to the VIC-IV I/O handler so reads/writes
 *     are stored and surfaced through the memory_t I/O layer.
 *  2. sim_init_vic2_defaults (sim_api.cpp): for MACHINE_MEGA65, write CHARPTR
 *     to point at the default chargen location (e.g. $20000) and remove the
 *     temporary D018/DD00 bank-1 fallback and its TEMPORARY comments.
 *  3. GetCharSetAddr (pane_vic_char_editor.cpp): for Mega65, read CHARPTR
 *     ($D068–$D06A) to compute the charset address instead of the D018/DD00
 *     path.  The returned value will need to be widened to uint32_t to handle
 *     addresses above $FFFF.
 *  4. Remove all "TEMPORARY" annotations added during the interim workaround.
 * --------------------------------------------------------------------------- */

#endif /* VIC2_H */
