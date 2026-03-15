#include "catch.hpp"
#include "cpu_6502.h"
#include "memory.h"
#include "interrupts.h"
#include <cstring>

TEST_CASE("System Integration - Large Memory Maps", "[integration][memory]") {
    memory_t mem;
    memset(&mem, 0, sizeof(mem));
    
    // Test writing to multiple 28-bit banks
    // Bank 0 (0x0xxxxxxx), Bank 1 (0x1xxxxxxx), Bank 15 (0xFxxxxxxx)
    
    SECTION("Multi-bank access") {
        far_mem_write(&mem, 0x0001000, 0xA1);
        far_mem_write(&mem, 0x1001000, 0xB2);
        far_mem_write(&mem, 0x2001000, 0xC3);
        far_mem_write(&mem, 0xF001000, 0xD4);
        
        CHECK(far_mem_read(&mem, 0x0001000) == 0xA1);
        CHECK(far_mem_read(&mem, 0x1001000) == 0xB2);
        CHECK(far_mem_read(&mem, 0x2001000) == 0xC3);
        CHECK(far_mem_read(&mem, 0xF001000) == 0xD4);
    }
    
    SECTION("Page boundary crossing") {
        // Sparse pages are 4KB (FAR_PAGE_SIZE)
        uint32_t addr = 0x1000FFF; // End of a page
        far_mem_write(&mem, addr, 0x55);
        far_mem_write(&mem, addr + 1, 0xAA);
        
        CHECK(far_mem_read(&mem, addr) == 0x55);
        CHECK(far_mem_read(&mem, addr + 1) == 0xAA);
    }

    mem_free_far_pages(&mem);
}

TEST_CASE("System Integration - High-Frequency Interrupts", "[integration][interrupts]") {
    // Heap-allocate to avoid ~1 MB stack frame from far_pages/io_handlers arrays
    memory_t* mem = new memory_t();
    memset(mem, 0, sizeof(*mem));
    CPU* cpu = CPUFactory::create(CPU_6502);
    cpu->mem = mem;
    cpu->reset();

    // IRQ vector $FFFE/$FFFF -> $3000
    mem->mem[0xFFFE] = 0x00;
    mem->mem[0xFFFF] = 0x30;

    // IRQ handler at $3000: LDA #$42, STA $0200, RTI
    mem->mem[0x3000] = 0xA9; // LDA #$42
    mem->mem[0x3001] = 0x42;
    mem->mem[0x3002] = 0x8D; // STA $0200
    mem->mem[0x3003] = 0x00;
    mem->mem[0x3004] = 0x02;
    mem->mem[0x3005] = 0x40; // RTI

    // Busy-spin at $1000: JMP $1000
    mem->mem[0x1000] = 0x4C; // JMP abs
    mem->mem[0x1001] = 0x00; // lo
    mem->mem[0x1002] = 0x10; // hi  ->  $1000

    cpu->pc = 0x1000;
    cpu->set_flag(FLAG_I, 0); // reset() sets FLAG_I; clear it to allow IRQs

    SECTION("Rapid IRQ triggers") {
        interrupt_controller_t* ic = static_cast<interrupt_controller_t*>(cpu->get_interrupt_controller());

        for (int i = 0; i < 5; i++) {
            interrupt_request_irq(ic);
            if (interrupt_check(ic, cpu)) {
                interrupt_handle(ic, cpu, mem);
            }
            cpu->step(); // LDA #$42
            cpu->step(); // STA $0200
            interrupt_release_irq(ic);
            cpu->step(); // RTI  ->  PC restored to $1000, I flag restored (clear)
            cpu->step(); // JMP $1000
        }

        // Handler must have stored the marker and RTI'd successfully
        CHECK(mem->mem[0x0200] == 0x42);
        CHECK(ic->handled_count == 5);
    }

    delete cpu;
    delete mem;
}
