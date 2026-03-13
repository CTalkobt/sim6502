#ifndef DEBUG_CONTEXT_H
#define DEBUG_CONTEXT_H

#include "cpu_observer.h"
#include "debug_types.h"
#include <stdint.h>

struct SnapNode;  /* Defined in debug_context.cpp */
class CPU;        /* Defined in cpu.h */

/*
 * DebugContext — owns all debug/instrumentation state for a sim session.
 * Implements ExecutionObserver so sim_api can attach it to the step loop.
 * Extracted from sim_session to support the lib6502-debug library boundary.
 */
class DebugContext : public ExecutionObserver {
public:
    DebugContext();
    ~DebugContext();

    /* ExecutionObserver: called by sim_api after each instruction */
    void on_after_execute(uint16_t pre_pc,
                          const CPUState &pre_state,
                          const CPUState &post_state,
                          uint64_t cycles_delta,
                          memory_t *mem) override;

    /* ---- History ---- */
    void clear_history();
    void enable_history(int on)        { hist_enabled_ = on; }
    int  history_is_enabled()  const   { return hist_enabled_; }
    int  history_depth()       const   { return hist_cap_; }
    int  history_count()       const   { return hist_count_; }
    int  history_pos()         const   { return hist_pos_; }
    int  step_back(CPU *cpu, memory_t *mem);
    int  step_fwd (CPU *cpu, memory_t *mem);
    int  get_history(int slot, sim_history_entry_t *entry);

    /* ---- Snapshot ---- */
    void take_snapshot();
    int  snapshot_is_valid()   const   { return snap_active_; }
    int  snapshot_diff(sim_diff_entry_t *entries, int cap);

    /* ---- Profiler ---- */
    void     enable_profiler(int on)      { prof_enabled_ = on; }
    int      profiler_is_enabled() const  { return prof_enabled_; }
    void     clear_profiler();
    uint32_t profiler_exec  (uint16_t addr) const { return prof_exec_   ? prof_exec_[addr]   : 0; }
    uint32_t profiler_cycles(uint16_t addr) const { return prof_cycles_ ? prof_cycles_[addr] : 0; }

    /* ---- Trace ---- */
    void enable_trace(int on)     { trace_enabled_ = on; }
    int  trace_is_enabled() const { return trace_enabled_; }
    void clear_trace()            { trace_head_ = 0; trace_count_ = 0; }
    int  trace_count()      const { return trace_count_; }
    int  get_trace(int slot, sim_trace_entry_t *entry);

private:
    /* History ring buffer */
    sim_history_entry_t *hist_buf_;
    int hist_cap_, hist_mask_, hist_write_, hist_count_, hist_enabled_, hist_pos_;

    /* Snapshot: 256-bucket linked-list hash table */
    SnapNode *snap_buckets_[256];
    int snap_active_;
    void snap_record_write(uint16_t addr, uint8_t before, uint8_t after, uint16_t writer_pc);
    void snap_free_nodes();

    /* Profiler */
    uint32_t *prof_exec_;
    uint32_t *prof_cycles_;
    int prof_enabled_;

    /* Trace ring buffer */
    sim_trace_entry_t *trace_buf_;
    int trace_head_, trace_count_, trace_enabled_;
};

#endif
