#include "vic2.h"
#include <stdio.h>
#include <string.h>

/* C64 colour palette — "Pepto" approximation (sRGB) */
const uint8_t vic2_palette[16][3] = {
    {0x00,0x00,0x00}, /* 0  Black      */
    {0xFF,0xFF,0xFF}, /* 1  White      */
    {0x88,0x00,0x00}, /* 2  Red        */
    {0xAA,0xFF,0xEE}, /* 3  Cyan       */
    {0xCC,0x44,0xCC}, /* 4  Purple     */
    {0x00,0xCC,0x55}, /* 5  Green      */
    {0x00,0x00,0xAA}, /* 6  Blue       */
    {0xEE,0xEE,0x77}, /* 7  Yellow     */
    {0xDD,0x88,0x55}, /* 8  Orange     */
    {0x66,0x44,0x00}, /* 9  Brown      */
    {0xFF,0x77,0x77}, /* A  Lt Red     */
    {0x33,0x33,0x33}, /* B  Dk Grey    */
    {0x77,0x77,0x77}, /* C  Grey       */
    {0xAA,0xFF,0x66}, /* D  Lt Green   */
    {0x00,0x88,0xFF}, /* E  Lt Blue    */
    {0xBB,0xBB,0xBB}, /* F  Lt Grey    */
};

const char *vic2_color_names[16] = {
    "Black", "White", "Red", "Cyan", "Purple", "Green", "Blue", "Yellow",
    "Orange", "Brown", "Lt Red", "Dk Grey", "Grey", "Lt Green", "Lt Blue", "Lt Grey"
};

static inline void vic_put(uint8_t *px, int x, int y, int ci)
{
    if (x < 0 || x >= VIC2_FRAME_W || y < 0 || y >= VIC2_FRAME_H) return;
    int off = (y * VIC2_FRAME_W + x) * 3;
    ci &= 0xF;
    px[off+0] = vic2_palette[ci][0];
    px[off+1] = vic2_palette[ci][1];
    px[off+2] = vic2_palette[ci][2];
}

void vic2_render_rgb(const memory_t *mem, uint8_t *buf)
{
    /* Read VIC-II control registers */
    uint8_t ctrl1    = mem->mem[0xD011];
    uint8_t ctrl2    = mem->mem[0xD016];
    uint8_t memsetup = mem->mem[0xD018];
    uint8_t border   = mem->mem[0xD020] & 0xF;
    uint8_t bg0      = mem->mem[0xD021] & 0xF;
    uint8_t bg1      = mem->mem[0xD022] & 0xF;
    uint8_t bg2      = mem->mem[0xD023] & 0xF;
    uint8_t bg3      = mem->mem[0xD024] & 0xF;

    int ecm = (ctrl1 >> 6) & 1;   /* Extended Colour Mode */
    int bmm = (ctrl1 >> 5) & 1;   /* Bitmap Mode          */
    int den = (ctrl1 >> 4) & 1;   /* Display Enable       */
    int mcm = (ctrl2 >> 4) & 1;   /* Multicolour Mode     */

    /* VIC bank: CIA2 Port A $DD00 bits 1:0 (inverted) */
    uint8_t  cia2a    = mem->mem[0xDD00];
    uint32_t vic_bank = (uint32_t)((~cia2a) & 3) * 0x4000u;

    uint32_t screen_base = vic_bank + (uint32_t)((memsetup >> 4) & 0xF) * 1024u;
    uint32_t char_base   = vic_bank + (uint32_t)((memsetup >> 1) & 0x7) * 2048u;
    uint32_t bm_base     = vic_bank + (uint32_t)(((memsetup >> 3) & 1) * 0x2000u);
    uint16_t color_ram   = 0xD800;

    /* Fill frame with border colour */
    for (int i = 0; i < VIC2_FRAME_W * VIC2_FRAME_H; i++) {
        buf[i*3+0] = vic2_palette[border][0];
        buf[i*3+1] = vic2_palette[border][1];
        buf[i*3+2] = vic2_palette[border][2];
    }

    if (!den) return;

    if (!bmm) {
        /* Character modes */
        for (int row = 0; row < 25; row++) {
            for (int col = 0; col < 40; col++) {
                uint16_t cell = (uint16_t)(row * 40 + col);
                uint8_t  sc   = mem->mem[(screen_base + cell) & 0xFFFF];
                uint8_t  cr   = mem->mem[(color_ram   + cell) & 0xFFFF] & 0xF;
                int      px0  = VIC2_ACTIVE_X + col * 8;
                int      py0  = VIC2_ACTIVE_Y + row * 8;

                if (ecm) {
                    /* Extended Colour: char bits 7:6 pick one of 4 backgrounds */
                    uint8_t bgtab[4] = { bg0, bg1, bg2, bg3 };
                    uint32_t cptr = (char_base + (uint32_t)(sc & 0x3F) * 8u) & 0xFFFF;
                    for (int cy = 0; cy < 8; cy++) {
                        uint8_t bits = mem->mem[(cptr + (uint32_t)cy) & 0xFFFF];
                        for (int cx = 0; cx < 8; cx++)
                            vic_put(buf, px0+cx, py0+cy,
                                    (bits & (0x80>>cx)) ? cr : bgtab[sc >> 6]);
                    }
                } else if (mcm && (cr & 0x8)) {
                    /* Multicolour: 2bpp, 4×8 doubled pixels */
                    uint8_t cols[4] = { bg0, bg1, bg2, (uint8_t)(cr & 0x7) };
                    uint32_t cptr = (char_base + (uint32_t)sc * 8u) & 0xFFFF;
                    for (int cy = 0; cy < 8; cy++) {
                        uint8_t bits = mem->mem[(cptr + (uint32_t)cy) & 0xFFFF];
                        for (int cx = 0; cx < 4; cx++) {
                            int sel = (bits >> (6 - cx*2)) & 0x3;
                            vic_put(buf, px0+cx*2,   py0+cy, cols[sel]);
                            vic_put(buf, px0+cx*2+1, py0+cy, cols[sel]);
                        }
                    }
                } else {
                    /* Standard char (hires cell when MCM but cr bit 3 = 0) */
                    uint32_t cptr = (char_base + (uint32_t)sc * 8u) & 0xFFFF;
                    for (int cy = 0; cy < 8; cy++) {
                        uint8_t bits = mem->mem[(cptr + (uint32_t)cy) & 0xFFFF];
                        for (int cx = 0; cx < 8; cx++)
                            vic_put(buf, px0+cx, py0+cy,
                                    (bits & (0x80>>cx)) ? cr : bg0);
                    }
                }
            }
        }
    } else {
        /* Bitmap modes */
        for (int row = 0; row < 25; row++) {
            for (int col = 0; col < 40; col++) {
                uint16_t cell = (uint16_t)(row * 40 + col);
                uint8_t  sc   = mem->mem[(screen_base + cell) & 0xFFFF];
                uint8_t  cr   = mem->mem[(color_ram   + cell) & 0xFFFF] & 0xF;
                uint8_t  fg   = (sc >> 4) & 0xF;
                uint8_t  bg   = sc & 0xF;
                int      px0  = VIC2_ACTIVE_X + col * 8;
                int      py0  = VIC2_ACTIVE_Y + row * 8;
                uint32_t bptr = (bm_base + (uint32_t)cell * 8u) & 0xFFFF;

                if (!mcm) {
                    /* Standard bitmap: 1bpp per 8×8 block */
                    for (int cy = 0; cy < 8; cy++) {
                        uint8_t bits = mem->mem[(bptr + (uint32_t)cy) & 0xFFFF];
                        for (int cx = 0; cx < 8; cx++)
                            vic_put(buf, px0+cx, py0+cy,
                                    (bits & (0x80>>cx)) ? fg : bg);
                    }
                } else {
                    /* Multicolour bitmap: 2bpp, 4×8 doubled pixels */
                    uint8_t cols[4] = { bg0, fg, bg, cr };
                    for (int cy = 0; cy < 8; cy++) {
                        uint8_t bits = mem->mem[(bptr + (uint32_t)cy) & 0xFFFF];
                        for (int cx = 0; cx < 4; cx++) {
                            int sel = (bits >> (6 - cx*2)) & 0x3;
                            vic_put(buf, px0+cx*2,   py0+cy, cols[sel]);
                            vic_put(buf, px0+cx*2+1, py0+cy, cols[sel]);
                        }
                    }
                }
            }
        }
    }

    /* ---- Sprite rendering ---- */
    /* Sprites are drawn 7→0 so sprite 0 ends up on top.
     * Coordinate mapping to frame buffer:
     *   frame_x = sprite_x - 16   (raster X 48 = col 0 = frame_x 32)
     *   frame_y = sprite_y - 15   (raster Y 51 = row 0 = frame_y 36)  */
    {
        uint8_t d015 = mem->mem[0xD015];   /* sprite enable    */
        uint8_t d010 = mem->mem[0xD010];   /* X MSBs           */
        uint8_t d017 = mem->mem[0xD017];   /* Y expand         */
        uint8_t d01c = mem->mem[0xD01C];   /* multicolor       */
        uint8_t d01d = mem->mem[0xD01D];   /* X expand         */
        uint8_t mc0  = mem->mem[0xD025] & 0xF;
        uint8_t mc1  = mem->mem[0xD026] & 0xF;

        for (int sn = 7; sn >= 0; sn--) {
            if (!(d015 & (1 << sn))) continue;

            int      sx  = mem->mem[0xD000 + sn*2];
            if (d010 & (1 << sn)) sx |= 0x100;
            int      sy  = mem->mem[0xD001 + sn*2];
            uint8_t  col = mem->mem[0xD027 + sn] & 0xF;
            int      xe  = (d01d & (1 << sn)) ? 2 : 1;
            int      ye  = (d017 & (1 << sn)) ? 2 : 1;
            int      mcf = (d01c & (1 << sn)) ? 1 : 0;

            uint16_t ptr_addr  = (uint16_t)((screen_base + 0x3F8u + (uint32_t)sn) & 0xFFFF);
            uint8_t  ptr       = mem->mem[ptr_addr];
            uint32_t data_base = (vic_bank + (uint32_t)ptr * 64u) & 0xFFFF;

            int fx0 = sx - 16;
            int fy0 = sy - 15;

            for (int row = 0; row < 21; row++) {
                uint16_t ra = (uint16_t)((data_base + (uint32_t)row * 3u) & 0xFFFF);
                uint32_t bits = ((uint32_t)mem->mem[ra]              << 16) |
                                ((uint32_t)mem->mem[(ra+1u) & 0xFFFF] <<  8) |
                                 (uint32_t)mem->mem[(ra+2u) & 0xFFFF];

                for (int yr = 0; yr < ye; yr++) {
                    int fy = fy0 + row * ye + yr;
                    if (fy < 0 || fy >= VIC2_FRAME_H) continue;

                    if (!mcf) {
                        /* Standard 1bpp: 24 pixels */
                        for (int px = 0; px < 24; px++) {
                            if (!(bits & (0x800000u >> (uint32_t)px))) continue;
                            for (int xr = 0; xr < xe; xr++)
                                vic_put(buf, fx0 + px * xe + xr, fy, col);
                        }
                    } else {
                        /* Multicolor 2bpp: 12 pixel-pairs */
                        for (int px = 0; px < 12; px++) {
                            int sel = (int)((bits >> (uint32_t)(22 - px*2)) & 0x3u);
                            if (sel == 0) continue;
                            uint8_t c = (sel == 1) ? mc0 : (sel == 2) ? col : mc1;
                            for (int xr = 0; xr < xe * 2; xr++)
                                vic_put(buf, fx0 + px * xe * 2 + xr, fy, c);
                        }
                    }
                }
            }
        }
    }
}

int vic2_render_ppm(const memory_t *mem, const char *filename)
{
    static uint8_t pixels[VIC2_FRAME_W * VIC2_FRAME_H * 3];
    vic2_render_rgb(mem, pixels);
    FILE *f = fopen(filename, "wb");
    if (!f) return -1;
    fprintf(f, "P6\n%d %d\n255\n", VIC2_FRAME_W, VIC2_FRAME_H);
    fwrite(pixels, 1, sizeof(pixels), f);
    fclose(f);
    return 0;
}

/* Active-area helper: same pixel logic as vic2_render_rgb but writes into a
 * VIC2_ACTIVE_W×VIC2_ACTIVE_H buffer with (0,0) = top-left of the display. */
static inline void vic_put_a(uint8_t *px, int x, int y, int ci)
{
    if ((unsigned)x >= VIC2_ACTIVE_W || (unsigned)y >= VIC2_ACTIVE_H) return;
    int off = (y * VIC2_ACTIVE_W + x) * 3;
    ci &= 0xF;
    px[off+0] = vic2_palette[ci][0];
    px[off+1] = vic2_palette[ci][1];
    px[off+2] = vic2_palette[ci][2];
}

void vic2_render_rgb_active(const memory_t *mem, uint8_t *buf)
{
    uint8_t ctrl1    = mem->mem[0xD011];
    uint8_t ctrl2    = mem->mem[0xD016];
    uint8_t memsetup = mem->mem[0xD018];
    uint8_t bg0      = mem->mem[0xD021] & 0xF;
    uint8_t bg1      = mem->mem[0xD022] & 0xF;
    uint8_t bg2      = mem->mem[0xD023] & 0xF;
    uint8_t bg3      = mem->mem[0xD024] & 0xF;

    int ecm = (ctrl1 >> 6) & 1;
    int bmm = (ctrl1 >> 5) & 1;
    int den = (ctrl1 >> 4) & 1;
    int mcm = (ctrl2 >> 4) & 1;

    uint8_t  cia2a    = mem->mem[0xDD00];
    uint32_t vic_bank = (uint32_t)((~cia2a) & 3) * 0x4000u;

    uint32_t screen_base = vic_bank + (uint32_t)((memsetup >> 4) & 0xF) * 1024u;
    uint32_t char_base   = vic_bank + (uint32_t)((memsetup >> 1) & 0x7) * 2048u;
    uint32_t bm_base     = vic_bank + (uint32_t)(((memsetup >> 3) & 1) * 0x2000u);
    uint16_t color_ram   = 0xD800;

    /* Fill with background colour */
    int bg_fill = den ? (int)bg0 : 0;
    for (int i = 0; i < VIC2_ACTIVE_W * VIC2_ACTIVE_H; i++) {
        buf[i*3+0] = vic2_palette[bg_fill][0];
        buf[i*3+1] = vic2_palette[bg_fill][1];
        buf[i*3+2] = vic2_palette[bg_fill][2];
    }

    if (!den) return;

    if (!bmm) {
        /* Character modes */
        for (int row = 0; row < 25; row++) {
            for (int col = 0; col < 40; col++) {
                uint16_t cell = (uint16_t)(row * 40 + col);
                uint8_t  sc   = mem->mem[(screen_base + cell) & 0xFFFF];
                uint8_t  cr   = mem->mem[(color_ram   + cell) & 0xFFFF] & 0xF;
                int      px0  = col * 8;
                int      py0  = row * 8;

                if (ecm) {
                    uint8_t bgtab[4] = { bg0, bg1, bg2, bg3 };
                    uint32_t cptr = (char_base + (uint32_t)(sc & 0x3F) * 8u) & 0xFFFF;
                    for (int cy = 0; cy < 8; cy++) {
                        uint8_t bits = mem->mem[(cptr + (uint32_t)cy) & 0xFFFF];
                        for (int cx = 0; cx < 8; cx++)
                            vic_put_a(buf, px0+cx, py0+cy,
                                      (bits & (0x80>>cx)) ? cr : bgtab[sc >> 6]);
                    }
                } else if (mcm && (cr & 0x8)) {
                    uint8_t cols[4] = { bg0, bg1, bg2, (uint8_t)(cr & 0x7) };
                    uint32_t cptr = (char_base + (uint32_t)sc * 8u) & 0xFFFF;
                    for (int cy = 0; cy < 8; cy++) {
                        uint8_t bits = mem->mem[(cptr + (uint32_t)cy) & 0xFFFF];
                        for (int cx = 0; cx < 4; cx++) {
                            int sel = (bits >> (6 - cx*2)) & 0x3;
                            vic_put_a(buf, px0+cx*2,   py0+cy, cols[sel]);
                            vic_put_a(buf, px0+cx*2+1, py0+cy, cols[sel]);
                        }
                    }
                } else {
                    uint32_t cptr = (char_base + (uint32_t)sc * 8u) & 0xFFFF;
                    for (int cy = 0; cy < 8; cy++) {
                        uint8_t bits = mem->mem[(cptr + (uint32_t)cy) & 0xFFFF];
                        for (int cx = 0; cx < 8; cx++)
                            vic_put_a(buf, px0+cx, py0+cy,
                                      (bits & (0x80>>cx)) ? cr : bg0);
                    }
                }
            }
        }
    } else {
        /* Bitmap modes */
        for (int row = 0; row < 25; row++) {
            for (int col = 0; col < 40; col++) {
                uint16_t cell = (uint16_t)(row * 40 + col);
                uint8_t  sc   = mem->mem[(screen_base + cell) & 0xFFFF];
                uint8_t  cr   = mem->mem[(color_ram   + cell) & 0xFFFF] & 0xF;
                uint8_t  fg   = (sc >> 4) & 0xF;
                uint8_t  bg   = sc & 0xF;
                int      px0  = col * 8;
                int      py0  = row * 8;
                uint32_t bptr = (bm_base + (uint32_t)cell * 8u) & 0xFFFF;

                if (!mcm) {
                    for (int cy = 0; cy < 8; cy++) {
                        uint8_t bits = mem->mem[(bptr + (uint32_t)cy) & 0xFFFF];
                        for (int cx = 0; cx < 8; cx++)
                            vic_put_a(buf, px0+cx, py0+cy,
                                      (bits & (0x80>>cx)) ? fg : bg);
                    }
                } else {
                    uint8_t cols[4] = { bg0, fg, bg, cr };
                    for (int cy = 0; cy < 8; cy++) {
                        uint8_t bits = mem->mem[(bptr + (uint32_t)cy) & 0xFFFF];
                        for (int cx = 0; cx < 4; cx++) {
                            int sel = (bits >> (6 - cx*2)) & 0x3;
                            vic_put_a(buf, px0+cx*2,   py0+cy, cols[sel]);
                            vic_put_a(buf, px0+cx*2+1, py0+cy, cols[sel]);
                        }
                    }
                }
            }
        }
    }

    /* ---- Sprite rendering (clipped to active area) ----
     * Same logic as vic2_render_rgb() but coordinates are shifted:
     *   active_x = frame_x - VIC2_ACTIVE_X  =  (sx - 16) - 32  =  sx - 48
     *   active_y = frame_y - VIC2_ACTIVE_Y  =  (sy - 15) - 36  =  sy - 51
     * vic_put_a() silently clips anything outside 0..319, 0..199.         */
    {
        uint8_t d015 = mem->mem[0xD015];
        uint8_t d010 = mem->mem[0xD010];
        uint8_t d017 = mem->mem[0xD017];
        uint8_t d01c = mem->mem[0xD01C];
        uint8_t d01d = mem->mem[0xD01D];
        uint8_t mc0  = mem->mem[0xD025] & 0xF;
        uint8_t mc1  = mem->mem[0xD026] & 0xF;

        for (int sn = 7; sn >= 0; sn--) {
            if (!(d015 & (1 << sn))) continue;

            int     sx  = mem->mem[0xD000 + sn*2];
            if (d010 & (1 << sn)) sx |= 0x100;
            int     sy  = mem->mem[0xD001 + sn*2];
            uint8_t col = mem->mem[0xD027 + sn] & 0xF;
            int     xe  = (d01d & (1 << sn)) ? 2 : 1;
            int     ye  = (d017 & (1 << sn)) ? 2 : 1;
            int     mcf = (d01c & (1 << sn)) ? 1 : 0;

            uint16_t ptr_addr  = (uint16_t)((screen_base + 0x3F8u + (uint32_t)sn) & 0xFFFF);
            uint8_t  ptr       = mem->mem[ptr_addr];
            uint32_t data_base = (vic_bank + (uint32_t)ptr * 64u) & 0xFFFF;

            int ax0 = sx - 48;   /* = (sx-16) - VIC2_ACTIVE_X */
            int ay0 = sy - 51;   /* = (sy-15) - VIC2_ACTIVE_Y */

            for (int row = 0; row < 21; row++) {
                uint16_t ra = (uint16_t)((data_base + (uint32_t)row * 3u) & 0xFFFF);
                uint32_t bits = ((uint32_t)mem->mem[ra]               << 16) |
                                ((uint32_t)mem->mem[(ra+1u) & 0xFFFF] <<  8) |
                                 (uint32_t)mem->mem[(ra+2u) & 0xFFFF];

                for (int yr = 0; yr < ye; yr++) {
                    int ay = ay0 + row * ye + yr;

                    if (!mcf) {
                        for (int px = 0; px < 24; px++) {
                            if (!(bits & (0x800000u >> (uint32_t)px))) continue;
                            for (int xr = 0; xr < xe; xr++)
                                vic_put_a(buf, ax0 + px * xe + xr, ay, col);
                        }
                    } else {
                        for (int px = 0; px < 12; px++) {
                            int sel = (int)((bits >> (uint32_t)(22 - px*2)) & 0x3u);
                            if (sel == 0) continue;
                            uint8_t c = (sel == 1) ? mc0 : (sel == 2) ? col : mc1;
                            for (int xr = 0; xr < xe * 2; xr++)
                                vic_put_a(buf, ax0 + px * xe * 2 + xr, ay, c);
                        }
                    }
                }
            }
        }
    }
}

int vic2_render_ppm_active(const memory_t *mem, const char *filename)
{
    static uint8_t pixels[VIC2_ACTIVE_W * VIC2_ACTIVE_H * 3];
    vic2_render_rgb_active(mem, pixels);
    FILE *f = fopen(filename, "wb");
    if (!f) return -1;
    fprintf(f, "P6\n%d %d\n255\n", VIC2_ACTIVE_W, VIC2_ACTIVE_H);
    fwrite(pixels, 1, sizeof(pixels), f);
    fclose(f);
    return 0;
}

void vic2_print_info(const memory_t *mem)
{
    uint8_t ctrl1    = mem->mem[0xD011];
    uint8_t ctrl2    = mem->mem[0xD016];
    uint8_t memsetup = mem->mem[0xD018];
    uint8_t border   = mem->mem[0xD020] & 0xF;
    uint8_t bg0      = mem->mem[0xD021] & 0xF;
    uint8_t bg1      = mem->mem[0xD022] & 0xF;
    uint8_t bg2      = mem->mem[0xD023] & 0xF;
    uint8_t bg3      = mem->mem[0xD024] & 0xF;
    uint8_t cia2a    = mem->mem[0xDD00];

    int ecm = (ctrl1 >> 6) & 1;
    int bmm = (ctrl1 >> 5) & 1;
    int den = (ctrl1 >> 4) & 1;
    int mcm = (ctrl2 >> 4) & 1;
    int bank = (~cia2a) & 3;

    const char *mode;
    if      (!den)              mode = "Display Off";
    else if (!bmm&&!ecm&&!mcm) mode = "Standard Char";
    else if (!bmm&&!ecm&& mcm) mode = "Multicolour Char";
    else if (!bmm&& ecm&&!mcm) mode = "Extended Colour";
    else if ( bmm&&!ecm&&!mcm) mode = "Standard Bitmap";
    else if ( bmm&&!ecm&& mcm) mode = "Multicolour Bitmap";
    else                        mode = "Invalid";

    uint32_t vic_bank    = (uint32_t)bank * 0x4000u;
    uint32_t screen_addr = vic_bank + (uint32_t)((memsetup >> 4) & 0xF) * 1024u;
    uint32_t cg_addr     = vic_bank + (uint32_t)((memsetup >> 1) & 0x7) * 2048u;
    uint32_t bm_addr     = vic_bank + (uint32_t)(((memsetup >> 3) & 1) * 0x2000u);

    printf("VIC-II State:\n");
    printf("  Mode     : %s\n", mode);
    printf("  D011     : $%02X  (ECM=%d BMM=%d DEN=%d RSEL=%d yscroll=%d)\n",
           ctrl1, ecm, bmm, den, (ctrl1>>3)&1, ctrl1&7);
    printf("  D016     : $%02X  (MCM=%d CSEL=%d xscroll=%d)\n",
           ctrl2, mcm, (ctrl2>>3)&1, ctrl2&7);
    printf("  D018     : $%02X\n", memsetup);
    printf("  Bank     : %d ($%04X-$%04X)  CIA2PA=$%02X\n",
           bank, (unsigned)vic_bank, (unsigned)(vic_bank + 0x3FFF), cia2a);
    printf("  Screen   : $%04X\n", (unsigned)screen_addr);
    if (bmm)
        printf("  Bitmap   : $%04X\n", (unsigned)bm_addr);
    else
        printf("  CharGen  : $%04X\n", (unsigned)cg_addr);
    printf("  Border   : %d (%s)\n", border, vic2_color_names[border]);
    printf("  BG0      : %d (%s)\n", bg0,    vic2_color_names[bg0]);
    if (ecm) {
        printf("  BG1      : %d (%s)\n", bg1, vic2_color_names[bg1]);
        printf("  BG2      : %d (%s)\n", bg2, vic2_color_names[bg2]);
        printf("  BG3      : %d (%s)\n", bg3, vic2_color_names[bg3]);
    }
    printf("  Frame    : %dx%d px (active 320x200 at +%d,+%d)\n",
           VIC2_FRAME_W, VIC2_FRAME_H, VIC2_ACTIVE_X, VIC2_ACTIVE_Y);
}

void vic2_print_regs(const memory_t *mem)
{
    uint8_t ctrl1    = mem->mem[0xD011];
    uint8_t ctrl2    = mem->mem[0xD016];
    uint8_t memsetup = mem->mem[0xD018];
    uint8_t raster   = mem->mem[0xD012];
    uint8_t border   = mem->mem[0xD020] & 0xF;
    uint8_t bg0      = mem->mem[0xD021] & 0xF;
    uint8_t bg1      = mem->mem[0xD022] & 0xF;
    uint8_t bg2      = mem->mem[0xD023] & 0xF;
    uint8_t bg3      = mem->mem[0xD024] & 0xF;
    uint8_t d019     = mem->mem[0xD019];
    uint8_t d01a     = mem->mem[0xD01A];
    uint8_t cia2a    = mem->mem[0xDD00];

    int ecm     = (ctrl1 >> 6) & 1;
    int bmm     = (ctrl1 >> 5) & 1;
    int den     = (ctrl1 >> 4) & 1;
    int rsel    = (ctrl1 >> 3) & 1;
    int rst8    = (ctrl1 >> 7) & 1;
    int yscroll = ctrl1 & 7;
    int mcm     = (ctrl2 >> 4) & 1;
    int csel    = (ctrl2 >> 3) & 1;
    int xscroll = ctrl2 & 7;
    int bank    = (~cia2a) & 3;

    uint32_t vic_bank    = (uint32_t)bank * 0x4000u;
    uint32_t screen_addr = vic_bank + (uint32_t)((memsetup >> 4) & 0xF) * 1024u;
    uint32_t cg_addr     = vic_bank + (uint32_t)((memsetup >> 1) & 0x7) * 2048u;
    uint32_t bm_addr     = vic_bank + (uint32_t)(((memsetup >> 3) & 1) * 0x2000u);
    uint16_t raster_line = (uint16_t)(raster | (rst8 << 8));

    const char *mode;
    if      (!den)               mode = "Display Off";
    else if (!bmm&&!ecm&&!mcm)  mode = "Standard Char";
    else if (!bmm&&!ecm&& mcm)  mode = "Multicolour Char";
    else if (!bmm&& ecm&&!mcm)  mode = "Extended Colour";
    else if ( bmm&&!ecm&&!mcm)  mode = "Standard Bitmap";
    else if ( bmm&&!ecm&& mcm)  mode = "Multicolour Bitmap";
    else                         mode = "Invalid";

    printf("VIC-II Registers:\n");
    printf("  Mode     : %s\n", mode);
    printf("  D011=$%02X : ECM=%d BMM=%d DEN=%d RSEL=%d RST8=%d yscroll=%d\n",
           ctrl1, ecm, bmm, den, rsel, rst8, yscroll);
    printf("  D016=$%02X : MCM=%d CSEL=%d xscroll=%d\n",
           ctrl2, mcm, csel, xscroll);
    printf("  D018=$%02X : screen=bits[7:4]  char/bm=bits[3:1]\n", memsetup);
    printf("  D012=$%02X : Raster line = %d ($%03X)\n",
           raster, raster_line, raster_line);
    printf("  D019=$%02X : IRQ=%d  RST=%d MBC=%d MMC=%d LP=%d\n",
           d019, (d019>>7)&1, d019&1, (d019>>1)&1, (d019>>2)&1, (d019>>3)&1);
    printf("  D01A=$%02X : ERST=%d EMBC=%d EMMC=%d ELP=%d\n",
           d01a, d01a&1, (d01a>>1)&1, (d01a>>2)&1, (d01a>>3)&1);
    printf("  Bank     : %d  ($%04X-$%04X)  CIA2PA=$%02X\n",
           bank, (unsigned)vic_bank, (unsigned)(vic_bank + 0x3FFF), cia2a);
    printf("  Screen   : $%04X\n", (unsigned)screen_addr);
    if (bmm)
        printf("  Bitmap   : $%04X\n", (unsigned)bm_addr);
    else
        printf("  CharGen  : $%04X\n", (unsigned)cg_addr);
    printf("  ColourRAM: $D800\n");
    printf("  D020 Border: %d (%s)\n",   border, vic2_color_names[border]);
    printf("  D021   BG0: %d (%s)\n",    bg0,    vic2_color_names[bg0]);
    printf("  D022   BG1: %d (%s)\n",    bg1,    vic2_color_names[bg1]);
    printf("  D023   BG2: %d (%s)\n",    bg2,    vic2_color_names[bg2]);
    printf("  D024   BG3: %d (%s)\n",    bg3,    vic2_color_names[bg3]);
}

void vic2_print_sprites(const memory_t *mem)
{
    uint8_t d015  = mem->mem[0xD015];
    uint8_t d01c  = mem->mem[0xD01C];
    uint8_t d01d  = mem->mem[0xD01D];
    uint8_t d017  = mem->mem[0xD017];
    uint8_t d01b  = mem->mem[0xD01B];
    uint8_t d010  = mem->mem[0xD010];
    uint8_t d018  = mem->mem[0xD018];
    uint8_t d025  = mem->mem[0xD025] & 0xF;
    uint8_t d026  = mem->mem[0xD026] & 0xF;
    uint8_t cia2a = mem->mem[0xDD00];

    uint32_t vic_bank    = (uint32_t)((~cia2a) & 3) * 0x4000u;
    uint32_t screen_base = vic_bank + (uint32_t)((d018 >> 4) & 0xF) * 1024u;

    printf("VIC-II Sprites  D015=$%02X  MC0=%d(%s)  MC1=%d(%s):\n",
           d015, d025, vic2_color_names[d025], d026, vic2_color_names[d026]);
    printf("  #  En   X    Y   Col            MCM XE  YE  BG  DataAddr\n");

    for (int sn = 0; sn < 8; sn++) {
        int      enabled = (d015 >> sn) & 1;
        uint16_t sx      = (uint16_t)(mem->mem[0xD000 + sn*2] | (((d010 >> sn) & 1) << 8));
        uint8_t  sy      = mem->mem[0xD001 + sn*2];
        uint8_t  color   = mem->mem[0xD027 + sn] & 0xF;
        uint8_t  ptr     = mem->mem[(screen_base + 0x3F8 + sn) & 0xFFFF];
        uint32_t saddr   = (vic_bank + (uint32_t)ptr * 64u) & 0xFFFF;

        printf("  %d  %-3s  %-4d %-3d  %X(%-10s)  %-3s %-3s %-3s %-3s $%04X\n",
               sn, enabled ? "Yes" : "No", sx, sy,
               color, vic2_color_names[color],
               (d01c >> sn) & 1 ? "Y" : "-",
               (d01d >> sn) & 1 ? "Y" : "-",
               (d017 >> sn) & 1 ? "Y" : "-",
               (d01b >> sn) & 1 ? "Y" : "-",
               (unsigned)saddr);
    }
}

/* -------------------------------------------------------------------------
 * JSON output functions
 * ------------------------------------------------------------------------- */

void vic2_json_info(const memory_t *mem)
{
    uint8_t ctrl1    = mem->mem[0xD011];
    uint8_t ctrl2    = mem->mem[0xD016];
    uint8_t memsetup = mem->mem[0xD018];
    uint8_t border   = mem->mem[0xD020] & 0xF;
    uint8_t bg0      = mem->mem[0xD021] & 0xF;
    uint8_t bg1      = mem->mem[0xD022] & 0xF;
    uint8_t bg2      = mem->mem[0xD023] & 0xF;
    uint8_t bg3      = mem->mem[0xD024] & 0xF;
    uint8_t cia2a    = mem->mem[0xDD00];

    int ecm  = (ctrl1 >> 6) & 1;
    int bmm  = (ctrl1 >> 5) & 1;
    int den  = (ctrl1 >> 4) & 1;
    int rsel = (ctrl1 >> 3) & 1;
    int mcm  = (ctrl2 >> 4) & 1;
    int csel = (ctrl2 >> 3) & 1;
    int bank = (~cia2a) & 3;

    const char *mode;
    if      (!den)              mode = "Display Off";
    else if (!bmm&&!ecm&&!mcm) mode = "Standard Char";
    else if (!bmm&&!ecm&& mcm) mode = "Multicolour Char";
    else if (!bmm&& ecm&&!mcm) mode = "Extended Colour";
    else if ( bmm&&!ecm&&!mcm) mode = "Standard Bitmap";
    else if ( bmm&&!ecm&& mcm) mode = "Multicolour Bitmap";
    else                        mode = "Invalid";

    uint32_t vic_bank    = (uint32_t)bank * 0x4000u;
    uint32_t screen_addr = vic_bank + (uint32_t)((memsetup >> 4) & 0xF) * 1024u;
    uint32_t cg_addr     = vic_bank + (uint32_t)((memsetup >> 1) & 0x7) * 2048u;
    uint32_t bm_addr     = vic_bank + (uint32_t)(((memsetup >> 3) & 1) * 0x2000u);

    printf("{\"mode\":\"%s\","
           "\"d011\":%d,\"d016\":%d,\"d018\":%d,"
           "\"ecm\":%d,\"bmm\":%d,\"den\":%d,\"mcm\":%d,"
           "\"rsel\":%d,\"csel\":%d,"
           "\"yscroll\":%d,\"xscroll\":%d,"
           "\"bank\":%d,\"vic_bank_start\":%u,"
           "\"screen_addr\":%u,\"chargen_addr\":%u,\"bitmap_addr\":%u,"
           "\"border_color\":%d,\"border_name\":\"%s\","
           "\"bg0\":%d,\"bg0_name\":\"%s\","
           "\"bg1\":%d,\"bg1_name\":\"%s\","
           "\"bg2\":%d,\"bg2_name\":\"%s\","
           "\"bg3\":%d,\"bg3_name\":\"%s\","
           "\"frame_w\":%d,\"frame_h\":%d}",
           mode,
           ctrl1, ctrl2, memsetup,
           ecm, bmm, den, mcm,
           rsel, csel,
           ctrl1 & 7, ctrl2 & 7,
           bank, (unsigned)vic_bank,
           (unsigned)screen_addr, (unsigned)cg_addr, (unsigned)bm_addr,
           border, vic2_color_names[border],
           bg0, vic2_color_names[bg0],
           bg1, vic2_color_names[bg1],
           bg2, vic2_color_names[bg2],
           bg3, vic2_color_names[bg3],
           VIC2_FRAME_W, VIC2_FRAME_H);
}

void vic2_json_regs(const memory_t *mem)
{
    uint8_t ctrl1    = mem->mem[0xD011];
    uint8_t ctrl2    = mem->mem[0xD016];
    uint8_t memsetup = mem->mem[0xD018];
    uint8_t raster   = mem->mem[0xD012];
    uint8_t border   = mem->mem[0xD020] & 0xF;
    uint8_t bg0      = mem->mem[0xD021] & 0xF;
    uint8_t bg1      = mem->mem[0xD022] & 0xF;
    uint8_t bg2      = mem->mem[0xD023] & 0xF;
    uint8_t bg3      = mem->mem[0xD024] & 0xF;
    uint8_t d019     = mem->mem[0xD019];
    uint8_t d01a     = mem->mem[0xD01A];
    uint8_t cia2a    = mem->mem[0xDD00];

    int ecm     = (ctrl1 >> 6) & 1;
    int bmm     = (ctrl1 >> 5) & 1;
    int den     = (ctrl1 >> 4) & 1;
    int rsel    = (ctrl1 >> 3) & 1;
    int rst8    = (ctrl1 >> 7) & 1;
    int mcm     = (ctrl2 >> 4) & 1;
    int csel    = (ctrl2 >> 3) & 1;
    int bank    = (~cia2a) & 3;

    uint32_t vic_bank    = (uint32_t)bank * 0x4000u;
    uint32_t screen_addr = vic_bank + (uint32_t)((memsetup >> 4) & 0xF) * 1024u;
    uint32_t cg_addr     = vic_bank + (uint32_t)((memsetup >> 1) & 0x7) * 2048u;
    uint32_t bm_addr     = vic_bank + (uint32_t)(((memsetup >> 3) & 1) * 0x2000u);
    uint16_t raster_line = (uint16_t)(raster | (rst8 << 8));

    const char *mode;
    if      (!den)               mode = "Display Off";
    else if (!bmm&&!ecm&&!mcm)  mode = "Standard Char";
    else if (!bmm&&!ecm&& mcm)  mode = "Multicolour Char";
    else if (!bmm&& ecm&&!mcm)  mode = "Extended Colour";
    else if ( bmm&&!ecm&&!mcm)  mode = "Standard Bitmap";
    else if ( bmm&&!ecm&& mcm)  mode = "Multicolour Bitmap";
    else                         mode = "Invalid";

    printf("{\"mode\":\"%s\","
           "\"d011\":%d,\"d016\":%d,\"d018\":%d,\"d012\":%d,"
           "\"ecm\":%d,\"bmm\":%d,\"den\":%d,\"mcm\":%d,"
           "\"rsel\":%d,\"csel\":%d,\"rst8\":%d,"
           "\"yscroll\":%d,\"xscroll\":%d,"
           "\"raster_line\":%d,"
           "\"irq_flag\":%d,\"irq_rst\":%d,\"irq_mbc\":%d,\"irq_mmc\":%d,\"irq_lp\":%d,"
           "\"irq_en_rst\":%d,\"irq_en_mbc\":%d,\"irq_en_mmc\":%d,\"irq_en_lp\":%d,"
           "\"bank\":%d,\"vic_bank_start\":%u,"
           "\"screen_addr\":%u,\"chargen_addr\":%u,\"bitmap_addr\":%u,"
           "\"border_color\":%d,\"border_name\":\"%s\","
           "\"bg0\":%d,\"bg0_name\":\"%s\","
           "\"bg1\":%d,\"bg1_name\":\"%s\","
           "\"bg2\":%d,\"bg2_name\":\"%s\","
           "\"bg3\":%d,\"bg3_name\":\"%s\"}",
           mode,
           ctrl1, ctrl2, memsetup, raster,
           ecm, bmm, den, mcm,
           rsel, csel, rst8,
           ctrl1 & 7, ctrl2 & 7,
           raster_line,
           (d019>>7)&1, d019&1, (d019>>1)&1, (d019>>2)&1, (d019>>3)&1,
           d01a&1, (d01a>>1)&1, (d01a>>2)&1, (d01a>>3)&1,
           bank, (unsigned)vic_bank,
           (unsigned)screen_addr, (unsigned)cg_addr, (unsigned)bm_addr,
           border, vic2_color_names[border],
           bg0, vic2_color_names[bg0],
           bg1, vic2_color_names[bg1],
           bg2, vic2_color_names[bg2],
           bg3, vic2_color_names[bg3]);
}

void vic2_json_sprites(const memory_t *mem)
{
    uint8_t d015  = mem->mem[0xD015];
    uint8_t d01c  = mem->mem[0xD01C];
    uint8_t d01d  = mem->mem[0xD01D];
    uint8_t d017  = mem->mem[0xD017];
    uint8_t d01b  = mem->mem[0xD01B];
    uint8_t d010  = mem->mem[0xD010];
    uint8_t d018  = mem->mem[0xD018];
    uint8_t d025  = mem->mem[0xD025] & 0xF;
    uint8_t d026  = mem->mem[0xD026] & 0xF;
    uint8_t cia2a = mem->mem[0xDD00];

    uint32_t vic_bank    = (uint32_t)((~cia2a) & 3) * 0x4000u;
    uint32_t screen_base = vic_bank + (uint32_t)((d018 >> 4) & 0xF) * 1024u;

    printf("{\"d015\":%d,\"mc0\":%d,\"mc0_name\":\"%s\",\"mc1\":%d,\"mc1_name\":\"%s\",\"sprites\":[",
           d015, d025, vic2_color_names[d025], d026, vic2_color_names[d026]);

    for (int sn = 0; sn < 8; sn++) {
        int      enabled = (d015 >> sn) & 1;
        uint16_t sx      = (uint16_t)(mem->mem[0xD000 + sn*2] | (((d010 >> sn) & 1) << 8));
        uint8_t  sy      = mem->mem[0xD001 + sn*2];
        uint8_t  color   = mem->mem[0xD027 + sn] & 0xF;
        uint8_t  ptr     = mem->mem[(screen_base + 0x3F8 + sn) & 0xFFFF];
        uint32_t saddr   = (vic_bank + (uint32_t)ptr * 64u) & 0xFFFF;

        if (sn > 0) printf(",");
        printf("{\"index\":%d,\"enabled\":%d,\"x\":%d,\"y\":%d,"
               "\"color\":%d,\"color_name\":\"%s\","
               "\"multicolor\":%d,\"expand_x\":%d,\"expand_y\":%d,"
               "\"behind_bg\":%d,\"data_addr\":%u}",
               sn, enabled, sx, sy,
               color, vic2_color_names[color],
               (d01c >> sn) & 1, (d01d >> sn) & 1, (d017 >> sn) & 1,
               (d01b >> sn) & 1, (unsigned)saddr);
    }
    printf("]}");
}
