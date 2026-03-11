#include "mega65_io.h"
#include "memory.h"
#include "vic2_io.h"

bool MathCoprocessorHandler::io_write(memory_t *mem, uint16_t addr, uint8_t val) {
    mem->mem[addr] = val;
    if (addr >= 0xD770 && addr <= 0xD773) {
        unsigned int a = mem->mem[0xD770] | ((unsigned int)mem->mem[0xD771] << 8);
        unsigned int b = mem->mem[0xD772] | ((unsigned int)mem->mem[0xD773] << 8);
        unsigned long long p = (unsigned long long)a * b;
        mem->mem[0xD778] = (unsigned char)(p);
        mem->mem[0xD779] = (unsigned char)(p >> 8);
        mem->mem[0xD77A] = (unsigned char)(p >> 16);
        mem->mem[0xD77B] = (unsigned char)(p >> 24);
    } else if (addr >= 0xD768 && addr <= 0xD76B) {
        unsigned int a = mem->mem[0xD768] | ((unsigned int)mem->mem[0xD769] << 8);
        unsigned int b = mem->mem[0xD76A] | ((unsigned int)mem->mem[0xD76B] << 8);
        unsigned int q = 0, r = 0;
        if (b) { q = a / b; r = a % b; }
        mem->mem[0xD778] = (unsigned char)(q);
        mem->mem[0xD779] = (unsigned char)(q >> 8);
        mem->mem[0xD77A] = (unsigned char)(r);
        mem->mem[0xD77B] = (unsigned char)(r >> 8);
    }
    return true;
}

bool DMAControllerHandler::io_write(memory_t *mem, uint16_t addr, uint8_t val) {
    mem->mem[addr] = val;
    if (addr == 0xD702) mem->mem[0xD704] = 0;
    if (addr == 0xD705) mem_dma_execute(mem, val, DMA_MODE_ENHANCED);
    else if (addr == 0xD700) mem_dma_execute(mem, val, DMA_MODE_LEGACY);
    return true;
}

void mega65_io_register(memory_t *mem) {
    static MathCoprocessorHandler math_handler;
    static DMAControllerHandler dma_handler;

    vic2_io_register(mem);

    if (mem->io_registry) {
        mem->io_registry->register_handler(0xD768, 0xD76B, &math_handler);
        mem->io_registry->register_handler(0xD770, 0xD773, &math_handler);
        mem->io_registry->register_handler(0xD700, 0xD705, &dma_handler);
        mem->io_registry->rebuild_map(mem);
    } else {
        /* Fallback for cases where registry isn't initialized yet */
        for (uint16_t a = 0xD768; a <= 0xD76B; a++) mem->io_handlers[a] = &math_handler;
        for (uint16_t a = 0xD770; a <= 0xD773; a++) mem->io_handlers[a] = &math_handler;
        mem->io_handlers[0xD700] = &dma_handler;
        mem->io_handlers[0xD702] = &dma_handler;
        mem->io_handlers[0xD705] = &dma_handler;
    }
}
