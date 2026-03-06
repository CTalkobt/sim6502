#ifndef VIC2_H
#define VIC2_H

#include <stdint.h>
#include "memory.h"

#define VIC2_FRAME_W  384   /* full PAL frame width  (32px border each side) */
#define VIC2_FRAME_H  272   /* full PAL frame height (36px border top/bottom) */
#define VIC2_ACTIVE_X  32   /* left border width (pixels) */
#define VIC2_ACTIVE_Y  36   /* top border height (pixels) */

/* C64 "Pepto" sRGB palette. Index 0–15. */
extern const uint8_t vic2_palette[16][3];

/* Human-readable colour names, index 0–15. */
extern const char *vic2_color_names[16];

/* Render a full VIC2_FRAME_W × VIC2_FRAME_H RGB frame into buf.
   buf must hold at least VIC2_FRAME_W * VIC2_FRAME_H * 3 bytes.
   Reads VIC-II registers and video data directly from mem->mem[]. */
void vic2_render_rgb(const memory_t *mem, uint8_t *buf);

/* Save a PPM screenshot to filename. Returns 0 on success, -1 on file error. */
int vic2_render_ppm(const memory_t *mem, const char *filename);

/* Print a human-readable VIC-II register summary to stdout. */
void vic2_print_info(const memory_t *mem);

#endif /* VIC2_H */
