#include "mega65_io.h"
#include "memory.h"
#include "vic2_io.h"

/* DMA list format versions */
enum { DMA_MODE_LEGACY = 0, DMA_MODE_ENHANCED = 1 };

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

void DMAControllerHandler::execute_dma(memory_t *mem, uint8_t val, int mode) {
    unsigned int job_addr = val | (mem->mem[0xD701] << 8) | (mem->mem[0xD702] << 16) | ((mem->mem[0xD704] & 0x0F) << 24);
    unsigned int ptr = job_addr;
    int format = (mode == DMA_MODE_LEGACY) ? 11 : 12;
    unsigned int src_mb = 0, dst_mb = 0;

    unsigned char token = mem_read_phys(mem, ptr);
    if (mode == DMA_MODE_ENHANCED && (token == 0x0A || token == 0x0B)) {
        ptr++; format = (token == 0x0A) ? 11 : 12;
        token = mem_read_phys(mem, ptr++);
        while (token != 0x00 && ptr - job_addr < 256) {
            if (token == 0x80) src_mb = mem_read_phys(mem, ptr++);
            else if (token == 0x81) dst_mb = mem_read_phys(mem, ptr++);
            else if (token & 0x80) ptr++;
            token = mem_read_phys(mem, ptr++);
        }
    }

    unsigned char cmd = mem_read_phys(mem, ptr);
    unsigned int count = mem_read_phys(mem, ptr + 1) | (mem_read_phys(mem, ptr + 2) << 8);
    if (count == 0) count = 65536;

    unsigned int src = mem_read_phys(mem, ptr + 3) | (mem_read_phys(mem, ptr + 4) << 8) |
                      ((mem_read_phys(mem, ptr + 5) & 0x0F) << 16) | (src_mb << 20);
    unsigned int dst = mem_read_phys(mem, ptr + 6) | (mem_read_phys(mem, ptr + 7) << 8) |
                      ((mem_read_phys(mem, ptr + 8) & 0x0F) << 16) | (dst_mb << 20);

    if ((cmd & 0x03) == 0x00) {
        for (unsigned int i = 0; i < count; i++) mem_write_phys(mem, dst + i, mem_read_phys(mem, src + i));
    } else if ((cmd & 0x03) == 0x03 || (format == 11 && (cmd & 0x03) == 0x02)) {
        unsigned char fill_val = mem_read_phys(mem, ptr + 3);
        for (unsigned int i = 0; i < count; i++) mem_write_phys(mem, dst + i, fill_val);
    }
}

bool DMAControllerHandler::io_write(memory_t *mem, uint16_t addr, uint8_t val) {
    mem->mem[addr] = val;
    if (addr == 0xD702) mem->mem[0xD704] = 0;
    if (addr == 0xD705) execute_dma(mem, val, DMA_MODE_ENHANCED);
    else if (addr == 0xD700) execute_dma(mem, val, DMA_MODE_LEGACY);
    return true;
}

void mega65_io_register(memory_t *mem) {
    static MathCoprocessorHandler math_handler;
    static DMAControllerHandler dma_handler;

    vic2_io_register(mem);

    mem->io_registry->register_handler(0xD768, 0xD76B, &math_handler);
    mem->io_registry->register_handler(0xD770, 0xD773, &math_handler);
    mem->io_registry->register_handler(0xD700, 0xD705, &dma_handler);
    mem->io_registry->rebuild_map(mem);
}
