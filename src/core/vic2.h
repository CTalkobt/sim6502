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

/* JSON output variants — print a single-line JSON object to stdout.
   Intended for use with the -J (JSON mode) interactive CLI flag. */
void vic2_json_info(const memory_t *mem);
void vic2_json_regs(const memory_t *mem);
void vic2_json_sprites(const memory_t *mem);

#endif /* VIC2_H */
