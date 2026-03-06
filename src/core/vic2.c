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
