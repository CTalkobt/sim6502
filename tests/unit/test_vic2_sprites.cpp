#include "catch.hpp"
#include "vic2.h"
#include "vic2_io.h"
#include "memory.h"
#include "io_registry.h"
#include <cstring>

TEST_CASE("VIC-II Sprites - Rendering and Data Access", "[devices][vic2][sprites]") {
    memory_t mem;
    memset(&mem, 0, sizeof(mem));
    IORegistry registry(nullptr);
    mem.io_registry = &registry;
    VIC2Handler vic;
    registry.register_handler(0xD000, 0xD03F, &vic);
    registry.rebuild_map(&mem);
    
    // Set VIC-II Bank 0 via CIA2 Port A ($DD00)
    // Bits 1:0 are inverted VIC bank. %11 -> Bank 0.
    mem_write(&mem, 0xDD00, 0x03);
    
    // Default screen base is $0000 (bits 7:4 of $D018 are 0)
    // Sprite pointers at $03F8-$03FF
    
    // Let's set up sprite 0
    mem_write(&mem, 0x03F8, 0x40); // Sprite 0 data at $40 * 64 = $1000
    
    // Fill sprite 0 with some data (hires)
    // A simple 1x1 pixel at top-left
    mem_write(&mem, 0x1000, 0x80); 
    
    // Set sprite 0 color to 1 (White)
    mem_write(&mem, 0xD027, 0x01);
    
    uint8_t buf[24 * 21 * 4];
    memset(buf, 0, sizeof(buf));
    
    SECTION("Hires Sprite Rendering") {
        mem_write(&mem, 0xD01C, 0x00); // All sprites hires
        vic2_render_sprite(&mem, 0, buf);
        
        // Pixel (0,0) should be white (255, 255, 255, 255)
        CHECK(buf[0] == 0xFF);
        CHECK(buf[1] == 0xFF);
        CHECK(buf[2] == 0xFF);
        CHECK(buf[3] == 0xFF);
        
        // Pixel (1,0) should be transparent (0, 0, 0, 0)
        CHECK(buf[4] == 0x00);
        CHECK(buf[7] == 0x00);
    }
    
    SECTION("Multicolour Sprite Rendering") {
        mem_write(&mem, 0xD01C, 0x01); // Sprite 0 multicolour
        mem_write(&mem, 0xD025, 0x02); // MC0 = Red
        mem_write(&mem, 0xD026, 0x05); // MC1 = Green
        
        // %01 00 00 00 -> first pixel pair is MC0
        mem_write(&mem, 0x1000, 0x40); 
        
        vic2_render_sprite(&mem, 0, buf);
        
        // First pixel of pair (0,0) should be MC0 (Red: 0x88, 0x00, 0x00)
        CHECK(buf[0] == 0x88);
        CHECK(buf[1] == 0x00);
        CHECK(buf[2] == 0x00);
        CHECK(buf[3] == 0xFF);
        
        // Second pixel of pair (1,0) should also be MC0 (Red)
        CHECK(buf[4] == 0x88);
        CHECK(buf[5] == 0x00);
        CHECK(buf[6] == 0x00);
        CHECK(buf[7] == 0xFF);

        // %11 00 00 00 -> first pixel pair is MC1 (Green: 0x00, 0xCC, 0x55)
        mem_write(&mem, 0x1000, 0xC0);
        vic2_render_sprite(&mem, 0, buf);
        CHECK(buf[0] == 0x00);
        CHECK(buf[1] == 0xCC);
        CHECK(buf[2] == 0x55);
        
        // %10 00 00 00 -> first pixel pair is Sprite Color (White)
        mem_write(&mem, 0x1000, 0x80);
        vic2_render_sprite(&mem, 0, buf);
        CHECK(buf[0] == 0xFF);
        CHECK(buf[1] == 0xFF);
        CHECK(buf[2] == 0xFF);
    }
}
