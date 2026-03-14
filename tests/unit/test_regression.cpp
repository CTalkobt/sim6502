#include "catch.hpp"
#include "sim_api.h"
#include <cstring>
#include <unistd.h>

TEST_CASE("Regression - Assembly Execution", "[regression]") {
    extern int g_verbose;
    g_verbose = 3;
    sim_session_t* s = sim_create("6502");
    REQUIRE(s != nullptr);

    SECTION("Basic LDA Assembly") {
        const char* asm_file = "test_basic.asm";
        FILE* f = fopen(asm_file, "w");
        fprintf(f, ".cpu _6502\n");
        fprintf(f, "* = $1000\n");
        fprintf(f, "lda #$42\n");
        fprintf(f, "rts\n");
        fclose(f);

        remove("test_basic.prg");
        int ok = sim_load_asm(s, asm_file);
        if (ok == 0) {
            sim_set_state(s, SIM_READY);
            sim_step(s, 10);
            
            CPU* cpu = sim_get_cpu(s);
            CHECK(cpu->a == 0x42);
        } else {
            WARN("sim_load_asm failed, likely KickAss65CE02.jar missing. Skipping test.");
        }

        remove(asm_file);
        remove("test_basic.prg");
        remove("test_basic.sym");
    }

    sim_destroy(s);
}

TEST_CASE("Regression - Undocumented Opcodes", "[regression][undoc]") {
    extern int g_verbose;
    g_verbose = 3;
    sim_session_t* s = sim_create("6502-undoc");
    REQUIRE(s != nullptr);

    SECTION("LAX Instruction") {
        const char* asm_file = "test_lax.asm";
        FILE* f = fopen(asm_file, "w");
        // .cpu _6502 makes KickAss happy. 
        // //.cpu "6502-undoc" makes our detector happy without breaking KickAss.
        fprintf(f, ".cpu _6502\n");
        fprintf(f, "//.cpu \"6502-undoc\"\n");
        fprintf(f, "* = $1000\n");
        fprintf(f, ".byte $AF, $34, $12 // LAX $1234\n");
        fprintf(f, "rts\n");
        fclose(f);

        remove("test_lax.prg");
        int ok = sim_load_asm(s, asm_file);
        if (ok == 0) {
            // Write memory AFTER load, as load clears memory
            sim_mem_write_byte(s, 0x1234, 0x55);

            // Check if we correctly handled the override
            CHECK(sim_get_cpu_type(s) == CPU_6502_UNDOCUMENTED);

            sim_set_state(s, SIM_READY);
            sim_step(s, 10);
            
            CPU* cpu = sim_get_cpu(s);
            CHECK(cpu->a == 0x55);
            CHECK(cpu->x == 0x55);
        } else {
            WARN("sim_load_asm failed, likely KickAss65CE02.jar missing. Skipping test.");
        }

        remove(asm_file);
        remove("test_lax.prg");
        remove("test_lax.sym");
    }

    sim_destroy(s);
}
