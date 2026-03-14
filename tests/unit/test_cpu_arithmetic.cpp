#include "catch.hpp"
#include "cpu_6502.h"
#include <cstring>

TEST_CASE("CPU Arithmetic - Binary Mode", "[cpu][arithmetic]") {
    CPU6502 cpu;
    cpu.reset();
    cpu.set_flag(FLAG_D, 0); // Binary mode

    SECTION("ADC Basic") {
        cpu.a = 0x10;
        cpu.set_flag(FLAG_C, 0);
        cpu.do_adc(0x20);
        CHECK(cpu.a == 0x30);
        CHECK(cpu.get_flag(FLAG_C) == 0);
        CHECK(cpu.get_flag(FLAG_Z) == 0);
        CHECK(cpu.get_flag(FLAG_N) == 0);
    }

    SECTION("ADC with Carry") {
        cpu.a = 0xFF;
        cpu.set_flag(FLAG_C, 0);
        cpu.do_adc(0x01);
        CHECK(cpu.a == 0x00);
        CHECK(cpu.get_flag(FLAG_C) == 1);
        CHECK(cpu.get_flag(FLAG_Z) == 1);
        CHECK(cpu.get_flag(FLAG_N) == 0);
    }

    SECTION("ADC Overflow") {
        cpu.a = 0x7F; // 127
        cpu.set_flag(FLAG_C, 0);
        cpu.do_adc(0x01); // +1 = 128 (-128)
        CHECK(cpu.a == 0x80);
        CHECK(cpu.get_flag(FLAG_V) == 1);
        CHECK(cpu.get_flag(FLAG_N) == 1);
    }

    SECTION("SBC Basic") {
        cpu.a = 0x30;
        cpu.set_flag(FLAG_C, 1); // No borrow
        cpu.do_sbc(0x10);
        CHECK(cpu.a == 0x20);
        CHECK(cpu.get_flag(FLAG_C) == 1); // No borrow out
    }

    SECTION("SBC with Borrow") {
        cpu.a = 0x10;
        cpu.set_flag(FLAG_C, 0); // Borrow in
        cpu.do_sbc(0x01);
        CHECK(cpu.a == 0x0E); // 16 - 1 - 1 = 14
        CHECK(cpu.get_flag(FLAG_C) == 1);
    }
}

TEST_CASE("CPU Arithmetic - Decimal Mode", "[cpu][arithmetic][decimal]") {
    CPU6502 cpu;
    cpu.reset();
    cpu.set_flag(FLAG_D, 1); // Decimal mode

    SECTION("ADC Decimal Basic") {
        cpu.a = 0x12;
        cpu.set_flag(FLAG_C, 0);
        cpu.do_adc(0x34);
        CHECK(cpu.a == 0x46);
        CHECK(cpu.get_flag(FLAG_C) == 0);
    }

    SECTION("ADC Decimal Carry") {
        cpu.a = 0x58;
        cpu.set_flag(FLAG_C, 0);
        cpu.do_adc(0x46); // 58 + 46 = 104 -> 0x04 with Carry
        CHECK(cpu.a == 0x04);
        CHECK(cpu.get_flag(FLAG_C) == 1);
    }

    SECTION("SBC Decimal Basic") {
        cpu.a = 0x46;
        cpu.set_flag(FLAG_C, 1);
        cpu.do_sbc(0x12);
        CHECK(cpu.a == 0x34);
        CHECK(cpu.get_flag(FLAG_C) == 1);
    }

    SECTION("SBC Decimal Borrow") {
        cpu.a = 0x12;
        cpu.set_flag(FLAG_C, 1);
        cpu.do_sbc(0x34); // 12 - 34 = -22 -> 78 with Borrow (C=0)
        // Note: 6502 decimal SBC is tricky. 
        // 12 - 34: 
        // lo: 2 - 4 = -2 -> +10 = 8, hi--
        // hi: 1 - 3 - 1 = -3 -> +10 = 7
        // Result: 0x78
        CHECK(cpu.a == 0x78);
        CHECK(cpu.get_flag(FLAG_C) == 0);
    }
}
