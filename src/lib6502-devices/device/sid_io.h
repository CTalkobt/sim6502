#ifndef SID_IO_H
#define SID_IO_H

#include "io_handler.h"
#include "machine.h"
#include <stdint.h>
#include <vector>

class SIDHandler : public IOHandler {
private:
    uint8_t regs[32];
    char name[32];
    uint64_t total_clocks;
    float phase;
    float freq;
    float pan; // -1.0 (Left) to 1.0 (Right)
    int cycle_acc;

public:
    SIDHandler(const char* chip_name = "SID", float pan_val = 0.0f);
    virtual const char* get_handler_name() const override { return name; }
    virtual bool io_write(memory_t *mem, uint16_t addr, uint8_t val) override;
    virtual bool io_read(memory_t *mem, uint16_t addr, uint8_t *val) override;
    virtual void reset() override;
    virtual void tick(uint64_t cycles) override;
    virtual size_t state_size() const override;
    virtual void save_state(uint8_t *buf) const override;
    virtual void load_state(const uint8_t *buf) override;
    
    void set_pan(float p) { pan = p; }
    float get_pan() const { return pan; }
    uint64_t get_clocks() const { return total_clocks; }
};

void sid_io_register(memory_t *mem, machine_type_t machine, std::vector<IOHandler*>& dynamic_handlers);

void sid_print_info(memory_t *mem);
void sid_print_regs(memory_t *mem);
void sid_json_info(memory_t *mem);
void sid_json_regs(memory_t *mem);

size_t sid_get_count();
SIDHandler* sid_get_instance(size_t index);

#endif
