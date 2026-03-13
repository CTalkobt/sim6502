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
    if (addr == 0xD012) {
        /* Simulate raster line based on clocks. 
           PAL C64: 312 lines per frame, 63 cycles per line.
           Approx 19656 cycles per frame. */
        uint64_t frame_cycles = total_clocks % 19656;
        int line = (int)(frame_cycles / 63);
        *val = (uint8_t)(line & 0xFF);
        return true;
    }
    if (addr == 0xD011) {
        /* Bit 7 is the 9th bit of the raster line */
        uint64_t frame_cycles = total_clocks % 19656;
        int line = (int)(frame_cycles / 63);
        *val = (mem->mem[addr] & 0x7F) | ((line & 0x100) ? 0x80 : 0);
        return true;
    }
    /* For now, just read from memory */
    *val = mem->mem[addr];
    return true;
}

void vic2_io_register(memory_t *mem) {
    static VIC2Handler vic2_handler;
    mem->io_registry->register_handler(0xD000, 0xD02E, &vic2_handler);
}
