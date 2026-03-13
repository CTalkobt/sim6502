#include "cia_io.h"
#include "memory.h"
#include <stdio.h>

CIAHandler::CIAHandler(const char* chip_name, bool nmi) 
    : is_nmi(nmi), int_line(nullptr), last_tick_cycles(0) {
    strncpy(name, chip_name, sizeof(name));
    reset();
}

void CIAHandler::reset() {
    pra = 0xFF;
    prb = 0xFF;
    ddra = 0;
    ddrb = 0;
    ta = 0xFFFF;
    ta_latch = 0xFFFF;
    tb = 0xFFFF;
    tb_latch = 0xFFFF;
    cra = 0;
    crb = 0;
    icr = 0;
    imr = 0;
    irq_active = false;
    tod_10th = tod_sec = tod_min = tod_hr = 0;
    tod_latched = false;
    port_a_in = 0xFF;
    port_b_in = 0xFF;
    for (int i=0; i<8; i++) keyboard_matrix[i] = 0xFF;
}

void CIAHandler::tick(uint64_t cycles) {
    if (last_tick_cycles == 0) {
        last_tick_cycles = cycles;
        return;
    }
    update_timers(cycles);
    last_tick_cycles = cycles;
}

void CIAHandler::update_timers(uint64_t cycles) {
    uint32_t delta = (uint32_t)(cycles - last_tick_cycles);
    if (delta == 0) return;

    bool ta_underflow = false;

    // Timer A
    if (cra & 0x01) { // Start bit
        if (!(cra & 0x20)) { // System cycles
            if (delta >= (uint32_t)ta) {
                ta_underflow = true;
                icr |= 0x01;
                uint32_t remaining = delta - ta;
                uint32_t period = ta_latch ? ta_latch : 0x10000;
                ta = (uint16_t)(period - (remaining % period));
                if (cra & 0x08) cra &= ~0x01; // One-shot mode
            } else {
                ta -= (uint16_t)delta;
            }
        }
    }

    // Timer B
    if (crb & 0x01) { // Start bit
        uint8_t mode = (crb >> 5) & 0x03;
        if (mode == 0x00) { // System cycles
            if (delta >= (uint32_t)tb) {
                icr |= 0x02;
                uint32_t remaining = delta - tb;
                uint32_t period = tb_latch ? tb_latch : 0x10000;
                tb = (uint16_t)(period - (remaining % period));
                if (crb & 0x08) crb &= ~0x01; // One-shot mode
            } else {
                tb -= (uint16_t)delta;
            }
        } else if (mode == 0x02 && ta_underflow) { // Timer A underflows
            if (tb == 0) {
                icr |= 0x02;
                tb = tb_latch;
                if (crb & 0x08) crb &= ~0x01;
            } else {
                tb--;
                if (tb == 0) {
                    icr |= 0x02;
                    tb = tb_latch;
                    if (crb & 0x08) crb &= ~0x01;
                }
            }
        }
    }

    check_interrupts();
}

void CIAHandler::check_interrupts() {
    if (icr & imr) {
        icr |= 0x80; // Set master interrupt bit
        if (!irq_active && int_line) {
            if (is_nmi) int_line->set_nmi(true);
            else        int_line->set_irq(true);
            irq_active = true;
        }
    }
}

bool CIAHandler::io_read(memory_t *mem, uint16_t addr, uint8_t *val) {
    uint8_t reg = addr & 0x0F;
    switch (reg) {
        case 0x00: // Port A
            *val = (pra & ddra) | (port_a_in & ~ddra);
            break;
        case 0x01: // Port B
            {
                uint8_t scan_mask = (pra | ~ddra);
                uint8_t key_data = 0xFF;
                for (int i=0; i<8; i++) {
                    if (!(scan_mask & (1 << i))) {
                        key_data &= keyboard_matrix[i];
                    }
                }
                *val = (prb & ddrb) | (key_data & port_b_in & ~ddrb);
            }
            break;
        case 0x02: // DDR A
            *val = ddra;
            break;
        case 0x03: // DDR B
            *val = ddrb;
            break;
        case 0x04: // Timer A Low
            *val = ta & 0xFF;
            break;
        case 0x05: // Timer A High
            *val = (ta >> 8) & 0xFF;
            break;
        case 0x06: // Timer B Low
            *val = tb & 0xFF;
            break;
        case 0x07: // Timer B High
            *val = (tb >> 8) & 0xFF;
            break;
        case 0x08: *val = tod_latched ? tod_latch_val[0] : tod_10th; break;
        case 0x09: *val = tod_latched ? tod_latch_val[1] : tod_sec; break;
        case 0x0A: *val = tod_latched ? tod_latch_val[2] : tod_min; break;
        case 0x0B: 
            *val = tod_latched ? tod_latch_val[3] : tod_hr; 
            tod_latched = false;
            break;
        case 0x0D: // ICR
            *val = icr;
            icr = 0;
            if (irq_active && int_line) {
                if (!is_nmi) int_line->set_irq(false);
                irq_active = false;
            }
            break;
        case 0x0E: // CRA
            *val = cra;
            break;
        case 0x0F: // CRB
            *val = crb;
            break;
        default:
            *val = 0xFF;
            break;
    }
    return true;
}

bool CIAHandler::io_write(memory_t *mem, uint16_t addr, uint8_t val) {
    uint8_t reg = addr & 0x0F;
    switch (reg) {
        case 0x00: // Port A
            pra = val;
            if (is_nmi) mem->mem[0xDD00] = val; // Ensure VIC-II sees bank selection even if written via mirror
            break;
        case 0x01: // Port B
            prb = val;
            break;
        case 0x02: // DDR A
            ddra = val;
            break;
        case 0x03: // DDR B
            ddrb = val;
            break;
        case 0x04: // Timer A Low Latch
            ta_latch = (ta_latch & 0xFF00) | val;
            break;
        case 0x05: // Timer A High Latch
            ta_latch = (ta_latch & 0x00FF) | (val << 8);
            if (!(cra & 0x01)) {
                ta = ta_latch;
            }
            break;
        case 0x06: // Timer B Low Latch
            tb_latch = (tb_latch & 0xFF00) | val;
            break;
        case 0x07: // Timer B High Latch
            tb_latch = (tb_latch & 0x00FF) | (val << 8);
            if (!(crb & 0x01)) {
                tb = tb_latch;
            }
            break;
        case 0x0D: // ICR
            if (val & 0x80) {
                imr |= (val & 0x1F);
            } else {
                imr &= ~(val & 0x1F);
            }
            check_interrupts();
            break;
        case 0x0E: // CRA
            cra = val;
            if (val & 0x10) {
                ta = ta_latch;
                cra &= ~0x10;
            }
            break;
        case 0x0F: // CRB
            crb = val;
            if (val & 0x10) {
                tb = tb_latch;
                crb &= ~0x10;
            }
            break;
    }
    mem->mem[addr] = val;
    return true;
}

void cia_io_register(memory_t *mem, std::vector<IOHandler*>& dynamic_handlers) {
    CIAHandler *cia1 = new CIAHandler("CIA1", false);
    CIAHandler *cia2 = new CIAHandler("CIA2", true);
    dynamic_handlers.push_back(cia1);
    dynamic_handlers.push_back(cia2);
    mem->io_registry->register_handler(0xDC00, 0xDC0F, cia1);
    mem->io_registry->register_handler(0xDD00, 0xDD0F, cia2);
}
