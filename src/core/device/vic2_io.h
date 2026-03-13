#ifndef VIC2_IO_H
#define VIC2_IO_H

#include "io_handler.h"

class VIC2Handler : public IOHandler {
private:
    uint64_t total_clocks;
public:
    VIC2Handler() : total_clocks(0) {}
    virtual const char* get_handler_name() const override { return "VIC-II Video Controller"; }
    virtual bool io_write(memory_t *mem, uint16_t addr, uint8_t val) override;
    virtual bool io_read(memory_t *mem, uint16_t addr, uint8_t *val) override;
    virtual void tick(uint64_t cycles) override { total_clocks = cycles; }
};

void vic2_io_register(memory_t *mem);

#endif
