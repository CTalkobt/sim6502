#include "sid_io.h"
#include "io_registry.h"
#include "audio.h"
#include <string.h>
#include <stdio.h>
#include <math.h>

SIDHandler::SIDHandler(const char* chip_name, float pan_val) {
    strncpy(name, chip_name, sizeof(name) - 1);
    name[sizeof(name) - 1] = '\0';
    pan = pan_val;
    phase = 0;
    freq = 0;
    cycle_acc = 0;
    reset();
}

bool SIDHandler::io_write(memory_t *mem, uint16_t addr, uint8_t val) {
    (void)mem;
    regs[addr & 0x1F] = val;
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
    cycle_acc = 0;
}

void SIDHandler::tick(uint64_t cycles) {
    if (cycles <= total_clocks) return;
    uint64_t delta = cycles - total_clocks;
    total_clocks = cycles;
    
    // Update frequency from current registers
    uint16_t f = regs[0] | (regs[1] << 8);
    freq = (float)f * 0.0596f; 

    // Basic pulse oscillator for verification
    if (freq > 0) {
        float step = freq / 44100.0f;
        
        cycle_acc += (int)delta;
        while (cycle_acc >= 22) {
            cycle_acc -= 22;
            phase += step;
            if (phase > 1.0f) phase -= 1.0f;
            
            float sample = (phase < 0.5f) ? 0.1f : -0.1f;
            
            // Apply simple panning (-1.0 = left, 1.0 = right, 0.0 = center)
            float left_gain = (pan < 0.0f) ? 1.0f : (1.0f - pan);
            float right_gain = (pan > 0.0f) ? 1.0f : (1.0f + pan);
            
            audio_push_sample(sample * left_gain, sample * right_gain);
        }
    }
}

size_t SIDHandler::state_size() const {
    return sizeof(regs) + sizeof(total_clocks) + sizeof(phase) + sizeof(freq);
}

void SIDHandler::save_state(uint8_t *buf) const {
    size_t offset = 0;
    memcpy(buf + offset, regs, sizeof(regs));
    offset += sizeof(regs);
    memcpy(buf + offset, &total_clocks, sizeof(total_clocks));
    offset += sizeof(total_clocks);
    memcpy(buf + offset, &phase, sizeof(phase));
    offset += sizeof(phase);
    memcpy(buf + offset, &freq, sizeof(freq));
}

void SIDHandler::load_state(const uint8_t *buf) {
    size_t offset = 0;
    memcpy(regs, buf + offset, sizeof(regs));
    offset += sizeof(regs);
    memcpy(&total_clocks, buf + offset, sizeof(total_clocks));
    offset += sizeof(total_clocks);
    memcpy(&phase, buf + offset, sizeof(phase));
    offset += sizeof(phase);
    memcpy(&freq, buf + offset, sizeof(freq));
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

    if (num_sids > 0) {
        audio_init(44100);
    }

    for (int i = 0; i < num_sids; i++) {
        char sid_name[32];
        snprintf(sid_name, sizeof(sid_name), "SID #%d", i + 1);
        
        float pan_val = 0.0f; // Center by default
        if (machine == MACHINE_MEGA65) {
            // SIDs 1 & 2 -> Right, SIDs 3 & 4 -> Left
            if (i == 0 || i == 1) pan_val = 0.5f;
            else pan_val = -0.5f;
        }
        
        SIDHandler* h = new SIDHandler(sid_name, pan_val);
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

size_t sid_get_count() {
    return g_sid_instances.size();
}

SIDHandler* sid_get_instance(size_t index) {
    if (index < g_sid_instances.size()) return g_sid_instances[index];
    return nullptr;
}
