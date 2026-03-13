#ifndef CIA_IO_H
#define CIA_IO_H

#include "io_handler.h"
#include <stdint.h>
#include <string.h>
#include <vector>

class CIAHandler : public IOHandler {
private:
    char name[32];
    bool is_nmi;
    InterruptLine *int_line;

    // Registers
    uint8_t pra;    // $00: Port A
    uint8_t prb;    // $01: Port B
    uint8_t ddra;   // $02: Data Direction Register A
    uint8_t ddrb;   // $03: Data Direction Register B
    
    // Timer A
    uint16_t ta;            // $04-$05: Current value
    uint16_t ta_latch;      // Latch value
    uint8_t cra;            // $0E: Control Register A
    
    // Timer B
    uint16_t tb;            // $06-$07: Current value
    uint16_t tb_latch;      // Latch value
    uint8_t crb;            // $0F: Control Register B

    // TOD
    uint8_t tod_10th;       // $08
    uint8_t tod_sec;        // $09
    uint8_t tod_min;        // $0A
    uint8_t tod_hr;         // $0B
    bool tod_latched;
    uint8_t tod_latch_val[4];

    // Interrupts
    uint8_t icr;            // $0D: Interrupt Control Register (Read: Status, Write: Mask)
    uint8_t imr;            // Internal Interrupt Mask Register
    bool irq_active;

    // External inputs
    uint8_t port_a_in;
    uint8_t port_b_in;
    uint8_t keyboard_matrix[8];

    uint64_t last_tick_cycles;

    void update_timers(uint64_t cycles);
    void check_interrupts();

public:
    CIAHandler(const char* chip_name = "CIA1", bool nmi = false);
    virtual const char* get_handler_name() const override { return name; }
    virtual bool io_write(memory_t *mem, uint16_t addr, uint8_t val) override;
    virtual bool io_read(memory_t *mem, uint16_t addr, uint8_t *val) override;
    virtual void reset() override;
    virtual void tick(uint64_t cycles) override;
    virtual void set_interrupt_line(InterruptLine *line) override { int_line = line; }

    void set_port_a_input(uint8_t val) { port_a_in = val; }
    void set_port_b_input(uint8_t val) { port_b_in = val; }
    void set_keyboard_row(int row, uint8_t val) { if (row >= 0 && row < 8) keyboard_matrix[row] = val; }
    uint8_t get_port_a_output() const { return pra; }
    uint8_t get_port_b_output() const { return prb; }
};

void cia_io_register(memory_t *mem, std::vector<IOHandler*>& dynamic_handlers);

#endif
