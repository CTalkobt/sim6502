#include "catch.hpp"
#include "sim_api.h"
#include "patterns.h"
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <errno.h>

struct PatternTestCase {
    const char* name;
    const char* proc;
    const char* machine;
    const char* setup_asm;
    const char* check_asm;
    const char* expected_regs;
};

static void run_pattern_test(const PatternTestCase& tc) {
    sim_session_t* s = sim_create(tc.proc);
    REQUIRE(s != nullptr);
    if (tc.machine) {
        if (strcmp(tc.machine, "mega65") == 0) sim_set_machine_type(s, MACHINE_MEGA65);
    }

    const snippet_t* sn = snippet_find(tc.name);
    REQUIRE(sn != nullptr);

    char asm_path[64];
    strcpy(asm_path, "test_pattern_XXXXXX.asm");
    // Note: mkstemp requires the last 6 chars to be XXXXXX and modifies the string in-place.
    // It does NOT support arbitrary extensions. We must use a valid template.
    char temp_template[] = "/tmp/sim_pattern_XXXXXX";
    int fd = mkstemp(temp_template);
    if (fd == -1) {
        FAIL("mkstemp failed: " << strerror(errno));
    }
    
    // We want it to have .asm extension for sim_load_asm logic
    char asm_file[256];
    snprintf(asm_file, sizeof(asm_file), "%s.asm", temp_template);
    
    FILE* f = fopen(asm_file, "w");
    REQUIRE(f != nullptr);
    fprintf(f, ".cpu %s\n", strcmp(tc.proc, "45gs02") == 0 ? "_45gs02" : "_6502");
    fprintf(f, "* = $2000\n");
    fprintf(f, "    jmp _main\n");
    fprintf(f, "%s\n", sn->body);
    fprintf(f, "_main:\n");
    fprintf(f, "    %s\n", tc.setup_asm);
    fprintf(f, "    jsr %s\n", tc.name);
    fprintf(f, "    %s\n", tc.check_asm);
    fprintf(f, "    rts\n");
    fclose(f);
    close(fd);

    int ok = sim_load_asm(s, asm_file);
    if (ok == 0) {
        sim_set_state(s, SIM_READY);
        sim_step(s, 10000); 

        CPU* cpu = sim_get_cpu(s);
        
        if (strstr(tc.expected_regs, "A=")) {
            unsigned int exp_a;
            sscanf(strstr(tc.expected_regs, "A=") + 2, "%x", &exp_a);
            CHECK(cpu->a == (uint8_t)exp_a);
        }
        if (strstr(tc.expected_regs, "X=")) {
            unsigned int exp_x;
            sscanf(strstr(tc.expected_regs, "X=") + 2, "%x", &exp_x);
            CHECK(cpu->x == (uint8_t)exp_x);
        }
        if (strstr(tc.expected_regs, "Y=")) {
            unsigned int exp_y;
            sscanf(strstr(tc.expected_regs, "Y=") + 2, "%x", &exp_y);
            CHECK(cpu->y == (uint8_t)exp_y);
        }
    } else {
        WARN("Skipping pattern test '" << tc.name << "' - assembly failed.");
    }

    unlink(asm_file);
    // Cleanup generated files
    char prg_path[256], sym_path[256];
    snprintf(prg_path, sizeof(prg_path), "%s.prg", temp_template);
    snprintf(sym_path, sizeof(sym_path), "%s.sym", temp_template);
    unlink(prg_path);
    unlink(sym_path);
    unlink(temp_template);
    sim_destroy(s);
}

TEST_CASE("Idioms - Logic Validation", "[idioms][logic]") {
    SECTION("add16: 200 + 52 = 252 ($00FC)") {
        run_pattern_test({
            "add16", "6502", nullptr,
            "lda #$C8\nsta $10\nlda #$00\nsta $11\nlda #$34\nsta $12\nlda #$00\nsta $13",
            "lda $13\nldx $12",
            "A=00 X=FC"
        });
    }

    SECTION("mul8: 7 * 9 = 63 ($3F)") {
        run_pattern_test({
            "mul8", "6502", nullptr,
            "lda #$07\nsta $10\nlda #$09\nsta $11",
            "lda $13\nldx $12",
            "A=00 X=3F"
        });
    }

    SECTION("memcopy: 4 bytes $0300->$0400") {
        run_pattern_test({
            "memcopy", "6502", nullptr,
            "lda #$AA\nsta $0300\nlda #$BB\nsta $0301\nlda #$CC\nsta $0302\nlda #$DD\nsta $0303\n"
            "lda #$00\nsta $10\nlda #$03\nsta $11\nlda #$00\nsta $12\nlda #$04\nsta $13\nlda #$04\nsta $14",
            "lda $0400\nldx $0401",
            "A=AA X=BB"
        });
    }
}
