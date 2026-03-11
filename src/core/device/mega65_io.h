#ifndef MEGA65_IO_H
#define MEGA65_IO_H

#include "io_handler.h"
#include "cpu.h"

class MathCoprocessorHandler : public IOHandler {
public:
    virtual const char* get_handler_name() const override { return "MEGA65 Math Coprocessor"; }
    virtual bool io_write(memory_t *mem, uint16_t addr, uint8_t val) override;
private:
    void update(memory_t *mem, uint16_t addr);
};

class DMAControllerHandler : public IOHandler {
public:
    virtual const char* get_handler_name() const override { return "MEGA65 DMA Controller"; }
    virtual bool io_write(memory_t *mem, uint16_t addr, uint8_t val) override;
};

/* Helper to register all MEGA65 peripherals */
void mega65_io_register(memory_t *mem);

#endif
