#include "vic2_io.h"
#include "memory.h"

bool VIC2Handler::io_write(memory_t *mem, uint16_t addr, uint8_t val) {
    if (addr == 0xD019) {
        /* Acknowledge interrupts: bits written as 1 are cleared */
        mem->mem[addr] &= ~val;
        return true;
    }
    /* Standard write for others */
    mem->mem[addr] = val;
    return true;
}

bool VIC2Handler::io_read(memory_t *mem, uint16_t addr, uint8_t *val) {
    /* For now, just read from memory */
    *val = mem->mem[addr];
    return true;
}

void vic2_io_register(memory_t *mem) {
    static VIC2Handler vic2_handler;
    if (mem->io_registry) {
        mem->io_registry->register_handler(0xD000, 0xD02E, &vic2_handler);
    } else {
        for (uint16_t a = 0xD000; a <= 0xD02E; a++) {
            mem->io_handlers[a] = &vic2_handler;
        }
    }
}
