#include "catch.hpp"
#include "vic2_io.h"
#include "sid_io.h"
#include "vic2.h"
#include "memory.h"
#include "io_registry.h"
#include "sim_api.h"
#include <cstring>
#include <vector>
#include <stdlib.h>

TEST_CASE("Devices - VIC-II", "[devices][vic2]") {
    memory_t mem;
    memset(&mem, 0, sizeof(mem));
    IORegistry registry(nullptr);
    mem.io_registry = &registry;

    VIC2Handler vic;
    registry.register_handler(0xD000, 0xD03F, &vic);
    registry.rebuild_map(&mem);

    SECTION("Register Write and Side Effects") {
        // $D020 = Border color
        mem_write(&mem, 0xD020, 0x01); // White
        uint8_t val = mem_read(&mem, 0xD020);
        CHECK(val == 0x01);
        
        // Check that VIC-II logic can render (just ensure no crash)
        uint8_t* frame = (uint8_t*)malloc(VIC2_FRAME_W * VIC2_FRAME_H * 3);
        REQUIRE(frame != nullptr);
        vic2_render_rgb(&mem, frame);
        // Byte at index 0 should be border color (White)
        // C64 color 1 is white.
        CHECK(frame[0] == 0xFF); 
        CHECK(frame[1] == 0xFF);
        CHECK(frame[2] == 0xFF);
        free(frame);
    }
}

TEST_CASE("Devices - VIC-II TODO", "[devices][vic2][todo][.]") {
    memory_t mem;
    memset(&mem, 0, sizeof(mem));
    IORegistry registry(nullptr);
    mem.io_registry = &registry;
    VIC2Handler vic;
    registry.register_handler(0xD000, 0xD03F, &vic);
    registry.rebuild_map(&mem);

    SECTION("Sprite-Sprite Collision ($D01E)") {
        // TODO: Implement collision detection logic.
        // Currently this register is likely just a memory byte.
        CHECK(mem_read(&mem, 0xD01E) == 0);
    }

    SECTION("Sprite-Background Collision ($D01F)") {
        // TODO: Implement collision detection logic.
        CHECK(mem_read(&mem, 0xD01F) == 0);
    }
}

TEST_CASE("Devices - SID", "[devices][sid]") {
    memory_t mem;
    memset(&mem, 0, sizeof(mem));
    IORegistry registry(nullptr);
    mem.io_registry = &registry;

    SIDHandler sid;
    registry.register_handler(0xD400, 0xD41F, &sid);
    registry.rebuild_map(&mem);

    SECTION("Register State") {
        // $D400 = Voice 1 Freq Low
        mem_write(&mem, 0xD400, 0x34);
        mem_write(&mem, 0xD401, 0x12);
        
        CHECK(mem_read(&mem, 0xD400) == 0x34);
        CHECK(mem_read(&mem, 0xD401) == 0x12);
        
        // $D404 = Voice 1 Control (Waveform)
        mem_write(&mem, 0xD404, 0x11); // Triangle + Gate
        CHECK(mem_read(&mem, 0xD404) == 0x11);
    }
}

TEST_CASE("Devices - MEGA65 io_handler flat map", "[devices][mega65]") {
    // Bug fix: SID and CIA were registered after mega65_io_register() called
    // rebuild_map() internally, so they were absent from the flat io_handlers[]
    // lookup table. Writes to $D400 (SID) and $DC00 (CIA) would bypass handlers.
    sim_session_t *s = sim_create("45gs02"); // implies MACHINE_MEGA65
    REQUIRE(s != nullptr);

    const memory_t *mem = sim_get_memory(s);
    REQUIRE(mem != nullptr);

    SECTION("SID handler present in flat map ($D400-$D41F)") {
        CHECK(mem->io_handlers[0xD400] != nullptr);
        CHECK(mem->io_handlers[0xD41F] != nullptr);
    }

    SECTION("CIA handler present in flat map ($DC00)") {
        CHECK(mem->io_handlers[0xDC00] != nullptr);
    }

    SECTION("Math coprocessor still present after rebuild ($D770)") {
        // Verify mega65_io_register entries were not clobbered by the second rebuild
        CHECK(mem->io_handlers[0xD770] != nullptr);
    }

    sim_destroy(s);
}

TEST_CASE("Devices - SID TODO", "[devices][sid][todo][.]") {
    memory_t mem;
    memset(&mem, 0, sizeof(mem));
    IORegistry registry(nullptr);
    mem.io_registry = &registry;
    SIDHandler sid;
    registry.register_handler(0xD400, 0xD41F, &sid);
    registry.rebuild_map(&mem);

    SECTION("Voice 3 Oscillator Output ($D41B)") {
        // TODO: Implement oscillator readout.
        CHECK(mem_read(&mem, 0xD41B) == 0);
    }

    SECTION("Voice 3 Envelope Output ($D41C)") {
        // TODO: Implement envelope readout.
        CHECK(mem_read(&mem, 0xD41C) == 0);
    }
}
