#ifndef IO_REGISTRY_H
#define IO_REGISTRY_H

#include "io_handler.h"
#include "interrupts.h"
#include <vector>
#include <memory>
#include <cstring>

/**
 * Concrete implementation of the interrupt line for peripherals.
 */
class CPUInterruptLine : public InterruptLine {
    interrupt_controller_t *ic;
public:
    CPUInterruptLine(void *ic_ptr) : ic(static_cast<interrupt_controller_t*>(ic_ptr)) {}
    void set_irq(bool asserted) override {
        if (asserted) interrupt_request_irq(ic);
        else          interrupt_release_irq(ic);
    }
    void set_nmi(bool asserted) override {
        if (asserted) interrupt_request_nmi(ic);
    }
};

/**
 * Manages all registered IOHandlers, provides range-based mapping and 
 * coordinates system-wide operations like tick and state snapshots.
 */
class IORegistry {
public:
    struct Registration {
        uint16_t start;
        uint16_t end;
        int priority;
        IOHandler *handler;
        bool enabled;
    };

private:
    std::vector<Registration> registrations;
    std::vector<IOHandler*> unique_handlers;
    std::unique_ptr<CPUInterruptLine> int_line;

public:
    IORegistry(void *ic) {
        int_line = std::make_unique<CPUInterruptLine>(ic);
    }

    /**
     * Register a handler for a specific range.
     * Higher priority values override lower ones.
     */
    void register_handler(uint16_t start, uint16_t end, IOHandler *handler, int priority = 0) {
        registrations.push_back({start, end, priority, handler, true});
        
        bool found = false;
        for (auto h : unique_handlers) if (h == handler) { found = true; break; }
        if (!found) {
            unique_handlers.push_back(handler);
            handler->set_interrupt_line(int_line.get());
        }
    }

    const std::vector<Registration>& get_registrations() const { return registrations; }

    void set_enabled(IOHandler* handler, bool enabled) {
        for (auto &reg : registrations) {
            if (reg.handler == handler) {
                reg.enabled = enabled;
            }
        }
    }

    /**
     * Rebuild the flat lookup table in memory_t based on registrations.
     */
    void rebuild_map(memory_t *mem) {
        for (int i = 0; i < 0x10000; i++) mem->io_handlers[i] = nullptr;
        
        // Sort by priority (ascending) so higher priority is written last
        std::vector<Registration> sorted = registrations;
        for (size_t i = 0; i < sorted.size(); i++) {
            for (size_t j = i + 1; j < sorted.size(); j++) {
                if (sorted[i].priority > sorted[j].priority) {
                    Registration tmp = sorted[i];
                    sorted[i] = sorted[j];
                    sorted[j] = tmp;
                }
            }
        }

        for (const auto &reg : sorted) {
            if (!reg.enabled) continue;
            for (uint32_t a = reg.start; a <= reg.end; a++) {
                mem->io_handlers[a] = reg.handler;
            }
        }
    }

    void tick_all(uint64_t cycles) {
        for (auto h : unique_handlers) h->tick(cycles);
    }

    IOHandler* find_handler(const char* name) {
        for (auto h : unique_handlers) {
            if (strcmp(h->get_handler_name(), name) == 0) return h;
        }
        return nullptr;
    }

    void reset_all() {
        for (auto h : unique_handlers) h->reset();
    }

    /* --- Snapshot Support --- */

    size_t get_total_state_size() const {
        size_t total = 0;
        for (auto h : unique_handlers) total += h->state_size();
        return total;
    }

    void save_all_state(uint8_t *buffer) const {
        size_t offset = 0;
        for (auto h : unique_handlers) {
            size_t sz = h->state_size();
            if (sz > 0) {
                h->save_state(buffer + offset);
                offset += sz;
            }
        }
    }

    void load_all_state(const uint8_t *buffer) {
        size_t offset = 0;
        for (auto h : unique_handlers) {
            size_t sz = h->state_size();
            if (sz > 0) {
                h->load_state(buffer + offset);
                offset += sz;
            }
        }
    }
};

#endif
