#include "sid_io.h"
#include "io_registry.h"
#include "audio.h"
#include <string.h>
#include <stdio.h>
#include <math.h>

SIDHandler::SIDHandler(const char* chip_name) {
    strncpy(name, chip_name, sizeof(name) - 1);
    name[sizeof(name) - 1] = '\0';
    phase = 0;
    freq = 0;
    reset();
}

bool SIDHandler::io_write(memory_t *mem, uint16_t addr, uint8_t val) {
    (void)mem;
    regs[addr & 0x1F] = val;
    
    // Update simple oscillator frequency (Voice 1)
    if ((addr & 0x1F) == 0 || (addr & 0x1F) == 1) {
        uint16_t f = regs[0] | (regs[1] << 8);
        freq = (float)f * 0.0596f; // Rough Hz estimate for C64
    }
    return true;
}

bool SIDHandler::io_read(memory_t *mem, uint16_t addr, uint8_t *val) {
    (void)mem;
    *val = regs[addr & 0x1F];
    return true;
}

void SIDHandler::reset() {
    memset(regs, 0, sizeof(regs));
    total_clocks = 0;
    phase = 0;
}

void SIDHandler::tick(uint64_t cycles) {
    total_clocks += cycles;
    
    // Basic pulse oscillator for Phase 3 verification
    if (freq > 0) {
        float step = freq / 44100.0f;
        for (uint64_t i = 0; i < cycles; i++) {
            // This is super inefficient but just for verification
            // In reality we should only push samples when they are needed
        }
        
        // Push a few samples based on cycles (assume 1MHz for now)
        // 1MHz / 44100Hz = ~22 cycles per sample
        static int cycle_acc = 0;
        cycle_acc += (int)cycles;
        if (cycle_acc >= 22) {
            cycle_acc -= 22;
            phase += step;
            if (phase > 1.0f) phase -= 1.0f;
            
            float sample = (phase < 0.5f) ? 0.1f : -0.1f;
            audio_push_sample(sample, sample);
        }
    }
}

static std::vector<SIDHandler*> g_sid_instances;

void sid_io_register(memory_t *mem, machine_type_t machine, std::vector<IOHandler*>& dynamic_handlers) {
    if (!mem->io_registry) return;
    g_sid_instances.clear();

    int num_sids = 0;
    if (machine == MACHINE_C64 || machine == MACHINE_C128) {
        num_sids = 1;
    } else if (machine == MACHINE_MEGA65) {
        num_sids = 4;
    }

    for (int i = 0; i < num_sids; i++) {
        char sid_name[32];
        snprintf(sid_name, sizeof(sid_name), "SID #%d", i + 1);
        SIDHandler* h = new SIDHandler(sid_name);
        dynamic_handlers.push_back(h);
        g_sid_instances.push_back(h);
        mem->io_registry->register_handler(0xD400 + (i * 0x20), 0xD41F + (i * 0x20), h);
    }
}

void sid_print_info(memory_t *mem) {
    (void)mem;
    printf("SID Subsystem: %d active chip(s)\n", (int)g_sid_instances.size());
    for (size_t i = 0; i < g_sid_instances.size(); i++) {
        printf("  %s: %lu clock cycles\n", g_sid_instances[i]->get_handler_name(), g_sid_instances[i]->get_clocks());
    }
}

void sid_print_regs(memory_t *mem) {
    (void)mem;
    for (size_t i = 0; i < g_sid_instances.size(); i++) {
        SIDHandler* h = g_sid_instances[i];
        printf("%s ($%04X):\n", h->get_handler_name(), 0xD400 + (unsigned int)(i * 0x20));
        for (int r = 0; r < 32; r++) {
            uint8_t val = 0;
            h->io_read(NULL, (uint16_t)(0xD400 + i*0x20 + r), &val);
            printf("%02X ", val);
            if ((r + 1) % 7 == 0) printf("\n");
        }
        printf("\n\n");
    }
}

void sid_json_info(memory_t *mem) {
    (void)mem;
    printf("{\"count\":%d}", (int)g_sid_instances.size());
}

void sid_json_regs(memory_t *mem) {
    (void)mem;
    printf("{\"chips\":[");
    for (size_t i = 0; i < g_sid_instances.size(); i++) {
        if (i > 0) printf(",");
        printf("{\"name\":\"%s\",\"regs\":[", g_sid_instances[i]->get_handler_name());
        for (int r = 0; r < 32; r++) {
            uint8_t val = 0;
            g_sid_instances[i]->io_read(NULL, (uint16_t)r, &val);
            printf("%d%s", val, r < 31 ? "," : "");
        }
        printf("]}");
    }
    printf("]}");
}
