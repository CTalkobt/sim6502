#include "mega65_io.h"
#include "memory.h"
#include "vic2_io.h"

/* DMA list format versions */
enum { DMA_MODE_LEGACY = 0, DMA_MODE_ENHANCED = 1 };

bool MathCoprocessorHandler::io_write(memory_t *mem, uint16_t addr, uint8_t val) {
    mem->mem[addr] = val;
    
    // According to MEGA65 book, MULTINA ($D770-$D773) and MULTINB ($D774-$D777)
    // are the inputs for BOTH multiplication and division.
    if (addr >= 0xD770 && addr <= 0xD777) {
        unsigned int a = mem->mem[0xD770] | ((unsigned int)mem->mem[0xD771] << 8) |
                         ((unsigned int)mem->mem[0xD772] << 16) | ((unsigned int)mem->mem[0xD773] << 24);
        unsigned int b = mem->mem[0xD774] | ((unsigned int)mem->mem[0xD775] << 8) |
                         ((unsigned int)mem->mem[0xD776] << 16) | ((unsigned int)mem->mem[0xD777] << 24);
        
        // 1. Multiplication: MULTOUT ($D778-$D77F) = MULTINA * MULTINB
        unsigned long long p = (unsigned long long)a * b;
        for (int i = 0; i < 8; i++) {
            mem->mem[0xD778 + i] = (unsigned char)(p >> (i * 8));
        }

        // 2. Division: DIVOUT ($D768-$D76F). 
        // DIVOUT(4-7) [$D76C-$D76F] = Whole part (quotient)
        // DIVOUT(0-3) [$D768-$D76B] = Fractional part (remainder)
        unsigned int q = 0, r = 0;
        if (b != 0) {
            q = a / b;
            r = a % b;
        } else {
            q = 0xFFFFFFFF; // Division by zero behavior
            r = a;
        }

        // Write remainder to $D768-$D76B
        for (int i = 0; i < 4; i++) {
            mem->mem[0xD768 + i] = (unsigned char)(r >> (i * 8));
        }
        // Write quotient to $D76C-$D76F
        for (int i = 0; i < 4; i++) {
            mem->mem[0xD76C + i] = (unsigned char)(q >> (i * 8));
        }

        // Set Busy bits (D70F). Book says MULBUSY is 0, DIVBUSY can be set but
        // for simulation we can keep it simple as it's "immediate" for now.
        // Bit 7: DIVBUSY, Bit 6: MULBUSY
        mem->mem[0xD70F] &= 0x3F; 
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

    // Register $D70F for Busy bits
    mem->io_registry->register_handler(0xD70F, 0xD70F, &math_handler);
    // Register $D770-$D777 for Inputs
    mem->io_registry->register_handler(0xD770, 0xD777, &math_handler);

    mem->io_registry->register_handler(0xD700, 0xD705, &dma_handler);
    mem->io_registry->rebuild_map(mem);
}
