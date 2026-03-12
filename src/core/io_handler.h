#ifndef IO_HANDLER_H
#define IO_HANDLER_H

#include <stdint.h>
#include <stddef.h>
#include "memory_types.h"

/* Forward declarations */
class InterruptLine;

/**
 * IOHandler: Interface for memory-mapped I/O peripherals.
 */
class IOHandler {
public:
    virtual ~IOHandler() {}

    /**
     * Identification for debugging and UI.
     */
    virtual const char* get_handler_name() const = 0;

    /**
     * Life-cycle: Called when the system is reset.
     */
    virtual void reset() {}

    /**
     * Clocking: Called to advance internal peripheral state.
     * @param cycles Total system cycles elapsed since power-on.
     */
    virtual void tick(uint64_t cycles) { (void)cycles; }

    /**
     * Called when the CPU reads from a registered address.
     * Return true if the read was handled (value in *val),
     * false to fall back to standard memory.
     */
    virtual bool io_read(memory_t *mem, uint16_t addr, uint8_t *val) { (void)mem; (void)addr; (void)val; return false; }

    /**
     * Debug/UI Read: Returns value without side-effects (e.g. not clearing status bits).
     */
    virtual bool io_peek(memory_t *mem, uint16_t addr, uint8_t *val) { return io_read(mem, addr, val); }

    /**
     * Called when the CPU writes to a registered address.
     * Return true if the write was handled (intercepted),
     * false to allow standard memory write to also occur.
     */
    virtual bool io_write(memory_t *mem, uint16_t addr, uint8_t val) { (void)mem; (void)addr; (void)val; return false; }

    /**
     * State Persistence: Serialization for history/snapshots.
     */
    virtual size_t state_size() const { return 0; }
    virtual void save_state(uint8_t *buf) const { (void)buf; }
    virtual void load_state(const uint8_t *buf) { (void)buf; }

    /**
     * Interrupt Signaling: Called by the registry to provide a callback line.
     */
    virtual void set_interrupt_line(InterruptLine *line) { (void)line; }
};

/**
 * Interface provided to IOHandlers to signal the CPU.
 */
class InterruptLine {
public:
    virtual ~InterruptLine() {}
    virtual void set_irq(bool asserted) = 0;
    virtual void set_nmi(bool asserted) = 0;
};

#endif
