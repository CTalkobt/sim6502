#include "debug_context.h"
#include "cpu.h"
#include <stdlib.h>
#include <string.h>
#include <chrono>

struct SnapNode {
    uint16_t  addr;
    uint8_t   before;
    uint8_t   after;
    uint16_t  writer_pc;
    SnapNode *next;
};

static int diff_entry_cmp(const void *a, const void *b) {
    return (int)((const sim_diff_entry_t *)a)->addr - (int)((const sim_diff_entry_t *)b)->addr;
}

/* -------------------------------------------------------------------------- */

DebugContext::DebugContext()
    : hist_buf_(nullptr),
      hist_cap_(SIM_HIST_DEFAULT_DEPTH), hist_mask_(SIM_HIST_DEFAULT_DEPTH - 1),
      hist_write_(0), hist_count_(0), hist_enabled_(0), hist_pos_(0),
      snap_active_(0), snap_cycles_(0), snap_timestamp_(0),
      prof_exec_(nullptr), prof_cycles_(nullptr), prof_enabled_(0),
      trace_enabled_(0)
{
    memset(snap_buckets_, 0, sizeof(snap_buckets_));
    hist_buf_    = new sim_history_entry_t[hist_cap_]();
    hist_enabled_ = (hist_buf_ != nullptr) ? 1 : 0;
    prof_exec_   = new uint32_t[65536]();
    prof_cycles_ = new uint32_t[65536]();
    trace_buf_.reserve(1024);
}

DebugContext::~DebugContext() {
    snap_free_nodes();
    delete[] hist_buf_;
    delete[] prof_exec_;
    delete[] prof_cycles_;
}

/* -------------------------------------------------------------------------- */
/* History                                                                     */
/* -------------------------------------------------------------------------- */

void DebugContext::clear_history() {
    hist_write_ = 0;
    hist_count_ = 0;
    hist_pos_   = 0;
}

int DebugContext::step_back(CPU *cpu, memory_t *mem) {
    if (!hist_buf_ || hist_pos_ >= hist_count_) return 0;
    unsigned idx = ((unsigned)hist_write_ - 1u - (unsigned)hist_pos_) & (unsigned)hist_mask_;
    sim_history_entry_t *he = &hist_buf_[idx];
    *static_cast<CPUState*>(cpu) = he->pre_cpu;
    for (int d = 0; d < he->delta_count; d++)
        mem->mem[he->delta_addr[d]] = he->delta_old[d];
    hist_pos_++;
    return 1;
}

int DebugContext::step_fwd(CPU *cpu, memory_t *mem) {
    if (!hist_buf_ || hist_pos_ == 0) return 0;
    unsigned idx = ((unsigned)hist_write_ - (unsigned)hist_pos_) & (unsigned)hist_mask_;
    sim_history_entry_t *he = &hist_buf_[idx];
    *static_cast<CPUState*>(cpu) = he->pre_cpu;
    mem->mem_writes = 0;
    cpu->step();
    hist_pos_--;
    return 1;
}

int DebugContext::get_history(int slot, sim_history_entry_t *entry) {
    if (!hist_buf_ || !entry || slot < 0 || slot >= hist_count_) return 0;
    unsigned idx = ((unsigned)hist_write_ - 1u - (unsigned)slot) & (unsigned)hist_mask_;
    *entry = hist_buf_[idx];
    return 1;
}

/* -------------------------------------------------------------------------- */
/* Snapshot                                                                    */
/* -------------------------------------------------------------------------- */

void DebugContext::snap_free_nodes() {
    for (int i = 0; i < 256; i++) {
        SnapNode *n = snap_buckets_[i];
        while (n) { 
            SnapNode *nx = n->next; 
            delete n;
            n = nx; 
        }
        snap_buckets_[i] = nullptr;
    }
}

void DebugContext::snap_record_write(uint16_t addr, uint8_t before, uint8_t after, uint16_t writer_pc) {
    int bucket = addr & 0xFF;
    for (SnapNode *n = snap_buckets_[bucket]; n; n = n->next) {
        if (n->addr == addr) { n->after = after; n->writer_pc = writer_pc; return; }
    }
    SnapNode *n = new SnapNode();
    if (!n) return;
    n->addr = addr; n->before = before; n->after = after; n->writer_pc = writer_pc;
    n->next = snap_buckets_[bucket];
    snap_buckets_[bucket] = n;
}

void DebugContext::take_snapshot(uint64_t cycles, uint32_t timestamp) {
    snap_free_nodes();
    snap_active_    = 1;
    snap_cycles_    = cycles;
    snap_timestamp_ = timestamp;
}

void DebugContext::clear_snapshot() {
    snap_free_nodes();
    snap_active_ = 0;
}

int DebugContext::snapshot_diff(sim_diff_entry_t *entries, int cap) {
    if (!snap_active_ || !entries || cap <= 0) return -1;
    int count = 0;
    int filled = 0;
    for (int i = 0; i < 256; i++) {
        SnapNode *n = snap_buckets_[i];
        while (n) {
            if (n->before != n->after) {
                if (filled < cap) {
                    entries[filled].addr      = n->addr;
                    entries[filled].before    = n->before;
                    entries[filled].after     = n->after;
                    entries[filled].writer_pc = n->writer_pc;
                    filled++;
                }
                count++;
            }
            n = n->next;
        }
    }
    if (filled > 0) {
        qsort(entries, (size_t)filled, sizeof(sim_diff_entry_t), diff_entry_cmp);
    }
    return count;
}

/* -------------------------------------------------------------------------- */
/* Profiler                                                                    */
/* -------------------------------------------------------------------------- */

void DebugContext::clear_profiler() {
    if (prof_exec_)   memset(prof_exec_,   0, 65536 * sizeof(uint32_t));
    if (prof_cycles_) memset(prof_cycles_, 0, 65536 * sizeof(uint32_t));
}

/* -------------------------------------------------------------------------- */
/* Trace                                                                       */
/* -------------------------------------------------------------------------- */

int DebugContext::get_trace(int slot, sim_trace_entry_t *entry) {
    if (slot < 0 || slot >= (int)trace_buf_.size() || !entry) return 0;
    *entry = trace_buf_[slot];
    return 1;
}

/* -------------------------------------------------------------------------- */
/* ExecutionObserver                                                            */
/* -------------------------------------------------------------------------- */

void DebugContext::on_after_execute(uint16_t pre_pc,
                                    const CPUState &pre_state,
                                    const CPUState &post_state,
                                    uint64_t cycles_delta,
                                    memory_t *mem)
{
    /* --- Snapshot accumulator --- */
    if (snap_active_) {
        int nw = mem->mem_writes < 256 ? mem->mem_writes : 256;
        for (int d = 0; d < nw; d++)
            snap_record_write(mem->mem_addr[d], mem->mem_old_val[d],
                              mem->mem_val[d], pre_pc);
    }

    /* --- History ring buffer --- */
    if (hist_enabled_ && hist_buf_) {
        // If we were in the middle of history and we execute a NEW instruction,
        // we must discard the "future" history that was stepped back from.
        if (hist_pos_ > 0) {
            hist_write_ = ((unsigned)hist_write_ - (unsigned)hist_pos_) & (unsigned)hist_mask_;
            hist_count_ -= hist_pos_;
            if (hist_count_ < 0) hist_count_ = 0;
            hist_pos_ = 0;
        }
        sim_history_entry_t *he = &hist_buf_[hist_write_];
        he->pre_cpu = pre_state;
        he->pc      = pre_pc;
        int dc = mem->mem_writes < 16 ? mem->mem_writes : 16;
        he->delta_count = (uint8_t)dc;
        for (int d = 0; d < dc; d++) {
            he->delta_addr[d] = mem->mem_addr[d];
            he->delta_old[d]  = mem->mem_old_val[d];
        }
        hist_write_ = (hist_write_ + 1) & hist_mask_;
        if (hist_count_ < hist_cap_) hist_count_++;
    }

    /* --- Trace storage --- */
    if (trace_enabled_) {
        if (trace_buf_.size() >= SIM_TRACE_DEPTH) {
            // Cap reached
        } else {
            sim_trace_entry_t te;
            auto now = std::chrono::steady_clock::now();
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
            te.timestamp    = (uint32_t)(ms & 0xFFFFFFFF);
            te.pc           = pre_pc;
            te.cpu          = post_state;
            te.cycles_delta = (int)cycles_delta;
            te.cycles_total = post_state.cycles;
            te.disasm[0]    = '\0'; 
            trace_buf_.push_back(te);
        }
    }

    /* --- Profiler --- */
    if (prof_enabled_ && prof_exec_) {
        prof_exec_[pre_pc]++;
        prof_cycles_[pre_pc] += (uint32_t)cycles_delta;
    }
}
