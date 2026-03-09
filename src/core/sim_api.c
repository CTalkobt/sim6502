#include "sim_api.h"
#include "cpu.h"
#include "memory.h"
#include "opcodes.h"
#include "cycles.h"
#include "breakpoints.h"
#include "trace.h"
#include "symbols.h"
#include "assembler.h"
#include "disassembler.h"
#include "cpu_engine.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* --- Snapshot linked-list accumulator ---
 * One node per unique virtual address written since the last snapshot.
 * Hashed by (addr & 0xFF) for O(1) average lookup.
 * Total overhead: 256 × sizeof(ptr) = 2 KB fixed + ~12 B per written addr.
 */
typedef struct snap_node {
    uint16_t          addr;
    uint8_t           before;     /* value when first written after snapshot */
    uint8_t           after;      /* most-recent value written               */
    uint16_t          writer_pc;  /* PC of last instruction that wrote here  */
    struct snap_node *next;       /* collision/overflow chain                */
} snap_node_t;

/* Full definition of the opaque sim_session_t handle. */
struct sim_session {
    cpu_t             cpu;
    memory_t          mem;
    dispatch_table_t  dt;
    symbol_table_t    symbols;
    breakpoint_list_t breakpoints;
    cpu_type_t        cpu_type;
    sim_state_t       state;
    unsigned short    start_addr;
    unsigned short    load_size;   /* byte count of the loaded binary (best-effort) */
    instruction_t     session_rom[65536]; /* debug overlay (parallel to mem) */
    char              filename[512];
    sim_event_cb      event_cb;
    void             *event_userdata;
    opcode_handler_t *handlers;
    int               num_handlers;
    sim_trace_entry_t trace_buf[SIM_TRACE_DEPTH];
    int               trace_head;
    int               trace_count;
    int               trace_enabled;
    /* Memory snapshot: 256-bucket hash table of linked lists */
    snap_node_t      *snap_buckets[256];  /* 2 KB fixed overhead */
    int               snap_active;        /* 1 if snapshot has been taken */
    uint32_t          prof_exec[65536];
    uint32_t          prof_cycles[65536];
    int               prof_enabled;
    /* Execution history ring buffer */
    sim_history_entry_t *hist_buf;
    int                  hist_cap;     /* ring buffer capacity (power of two)    */
    int                  hist_mask;    /* = hist_cap - 1                         */
    int                  hist_write;   /* index of next write slot               */
    int                  hist_count;   /* entries stored (0 .. hist_cap)         */
    int                  hist_enabled; /* 1 = recording active                   */
    int                  hist_pos;     /* 0 = present, N = N steps back          */
};

/* --- Internal Helpers --- */

static int handle_trap(const symbol_table_t *st, cpu_t *cpu, memory_t *mem) {
	for (int i = 0; i < st->count; i++) {
		if (st->symbols[i].type != SYM_TRAP) continue;
		if (st->symbols[i].address != cpu->pc) continue;

		cpu->cycles += 6;
		cpu->s++;
		unsigned short lo = mem_read(mem, 0x100 + cpu->s);
		cpu->s++;
		unsigned short hi = mem_read(mem, 0x100 + cpu->s);
		unsigned short ret = (unsigned short)(((unsigned short)hi << 8) | lo);
		ret++;

		if (ret == 0) {
			cpu->pc = ret;
			return -1;
		}
		cpu->pc = ret;
		return 1;
	}
	return 0;
}

static void api_history_clear(sim_session_t *s) {
    s->hist_write = 0;
    s->hist_count = 0;
    s->hist_pos   = 0;
}

/* Record one write into the snapshot accumulator.
 * On first write to addr: stores `before` as the snapshot value.
 * On subsequent writes: only updates `after` and `writer_pc`. */
static void snap_record_write(sim_session_t *s,
                               uint16_t addr, uint8_t before,
                               uint8_t after,  uint16_t writer_pc)
{
    int bucket = addr & 0xFF;
    snap_node_t *n = s->snap_buckets[bucket];
    while (n) {
        if (n->addr == addr) {
            n->after      = after;
            n->writer_pc  = writer_pc;
            return;
        }
        n = n->next;
    }
    n = (snap_node_t *)malloc(sizeof(snap_node_t));
    if (!n) return;
    n->addr      = addr;
    n->before    = before;
    n->after     = after;
    n->writer_pc = writer_pc;
    n->next      = s->snap_buckets[bucket];
    s->snap_buckets[bucket] = n;
}

static void api_select_handlers(sim_session_t *s) {
    switch (s->cpu_type) {
    case CPU_65C02:          s->handlers = opcodes_65c02;    s->num_handlers = OPCODES_65C02_COUNT;    break;
    case CPU_65CE02:         s->handlers = opcodes_65ce02;   s->num_handlers = OPCODES_65CE02_COUNT;   break;
    case CPU_45GS02:         s->handlers = opcodes_45gs02;   s->num_handlers = OPCODES_45GS02_COUNT;   break;
    case CPU_6502_UNDOCUMENTED: s->handlers = opcodes_6502_undoc; s->num_handlers = OPCODES_6502_UNDOC_COUNT; break;
    default:                 s->handlers = opcodes_6502;     s->num_handlers = OPCODES_6502_COUNT;     break;
    }
}

static const char *processor_name_local(cpu_type_t type) {
	switch (type) {
	case CPU_6502: return "6502";
	case CPU_6502_UNDOCUMENTED: return "6502 (undoc)";
	case CPU_65C02: return "65C02";
	case CPU_65CE02: return "65CE02";
	case CPU_45GS02: return "45GS02";
	default: return "unknown";
	}
}

/* --- API Implementation --- */

sim_session_t *sim_create(const char *processor) {
    sim_session_t *s = (sim_session_t *)calloc(1, sizeof(*s));
    if (!s) return NULL;
    s->cpu_type = CPU_6502;
    if (processor) {
        if      (strcmp(processor, "6502-undoc") == 0) s->cpu_type = CPU_6502_UNDOCUMENTED;
        else if (strcmp(processor, "65c02")      == 0) s->cpu_type = CPU_65C02;
        else if (strcmp(processor, "65ce02")     == 0) s->cpu_type = CPU_65CE02;
        else if (strcmp(processor, "45gs02")     == 0) s->cpu_type = CPU_45GS02;
    }
    api_select_handlers(s);
    cpu_init(&s->cpu);
    memset(&s->mem, 0, sizeof(s->mem));
    symbol_table_init(&s->symbols, "Session");
    breakpoint_init(&s->breakpoints);
    s->state = SIM_IDLE;
    s->start_addr = 0x0200;
    /* History ring buffer */
    s->hist_cap     = SIM_HIST_DEFAULT_DEPTH;
    s->hist_mask    = s->hist_cap - 1;
    s->hist_buf     = (sim_history_entry_t *)calloc((size_t)s->hist_cap, sizeof(sim_history_entry_t));
    s->hist_enabled = (s->hist_buf != NULL);
    return s;
}

void sim_destroy(sim_session_t *s) {
    if (!s) return;
    for (int i = 0; i < FAR_NUM_PAGES; i++) {
        if (s->mem.far_pages[i]) { free(s->mem.far_pages[i]); s->mem.far_pages[i] = NULL; }
    }
    for (int i = 0; i < 256; i++) {
        snap_node_t *n = s->snap_buckets[i];
        while (n) { snap_node_t *nx = n->next; free(n); n = nx; }
    }
    free(s->hist_buf);
    free(s);
}

int sim_load_asm(sim_session_t *s, const char *path) {
    if (!s || !path) return -1;
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    memset(&s->mem, 0, sizeof(s->mem));
    memset(s->session_rom, 0, sizeof(s->session_rom));
    symbol_table_init(&s->symbols, "Session");
    breakpoint_init(&s->breakpoints);
    api_history_clear(s);
    cpu_type_t cpu_type = s->cpu_type;
    api_select_handlers(s);
    char line[512];
    int pc = 0x0200;
    while (fgets(line, sizeof(line), f)) {
        char *ptr = line;
        while (*ptr && isspace(*ptr)) ptr++;
        if (!*ptr || *ptr == ';') continue;
        if (*ptr == '.') { handle_pseudo_op(ptr, &cpu_type, &pc, NULL, NULL, NULL); continue; }
        char *semi  = strchr(ptr, ';');
        char *colon = strchr(ptr, ':');
        if (colon && (!semi || colon < semi)) {
            char label_name[64]; int len = (int)(colon - ptr); if (len >= 64) len = 63;
            strncpy(label_name, ptr, len); label_name[len] = 0;
            symbol_add(&s->symbols, label_name, (unsigned short)pc, SYM_LABEL, "Source");
            const char *after = colon + 1;
            while (*after && isspace(*after)) after++;
            if (*after == '.') { handle_pseudo_op(after, &cpu_type, &pc, NULL, NULL, NULL); continue; }
        }
        instruction_t instr; parse_line(line, &instr, NULL, pc);
        if (instr.op[0]) {
            int len;
            if (strcmp(instr.op, "BRK") == 0) len = (cpu_type == CPU_45GS02) ? 1 : 2;
            else {
                len = get_encoded_length(instr.op, instr.mode, s->handlers, s->num_handlers, cpu_type);
                if (len < 0) len = get_instruction_length(instr.mode);
            }
            pc += len;
        }
    }
    s->cpu_type = cpu_type;
    api_select_handlers(s);
    cpu_init(&s->cpu);
    s->cpu.pc = 0x0200;
    s->start_addr = 0x0200;
    if (s->cpu_type == CPU_45GS02) set_flag(&s->cpu, FLAG_E, 1);
    rewind(f);
    pc = 0x0200;
    while (fgets(line, sizeof(line), f)) {
        const char *ptr = line;
        while (*ptr && isspace(*ptr)) ptr++;
        if (!*ptr || *ptr == ';') continue;
        if (*ptr == '.') { handle_pseudo_op(ptr, &s->cpu_type, &pc, &s->mem, &s->symbols, NULL); continue; }
        const char *semi2  = strchr(ptr, ';');
        const char *colon = strchr(ptr, ':');
        if (colon && (!semi2 || colon < semi2)) {
            const char *after = colon + 1;
            while (*after && isspace(*after)) after++;
            if (*after == '.') { handle_pseudo_op(after, &s->cpu_type, &pc, &s->mem, &s->symbols, NULL); continue; }
        }
        instruction_t instr; parse_line(line, &instr, &s->symbols, pc);
        if (instr.op[0]) {
            s->session_rom[pc] = instr;
            int enc = encode_to_mem(&s->mem, pc, &instr, s->handlers, s->num_handlers, s->cpu_type);
            if (strcmp(instr.op, "BRK") == 0) pc += (s->cpu_type == CPU_45GS02) ? 1 : 2;
            else if (enc > 0) pc += enc;
            else pc += get_instruction_length(instr.mode);
        }
    }
    fclose(f);
    dispatch_build(&s->dt, s->handlers, s->num_handlers, s->cpu_type);
    strncpy(s->filename, path, sizeof(s->filename) - 1);
    s->load_size = (unsigned short)(pc > s->start_addr ? pc - s->start_addr : 0);
    s->state = SIM_READY;
    return 0;
}

/* --------------------------------------------------------------------------
 * Binary load/save
 * -------------------------------------------------------------------------- */

/* Shared setup for binary loads: reset session state, init CPU. */
static void binary_load_common(sim_session_t *s, uint16_t load_addr, int byte_count)
{
    cpu_init(&s->cpu);
    s->cpu.pc    = load_addr;
    s->start_addr = load_addr;
    s->load_size  = (unsigned short)(byte_count > 0xFFFF ? 0xFFFF : byte_count);
    if (s->cpu_type == CPU_45GS02) set_flag(&s->cpu, FLAG_E, 1);
    dispatch_build(&s->dt, s->handlers, s->num_handlers, s->cpu_type);
    s->state = SIM_READY;
}

int sim_load_bin(sim_session_t *s, const char *path, uint16_t load_addr)
{
    if (!s || !path) return -1;
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    memset(&s->mem, 0, sizeof(s->mem));
    memset(s->session_rom, 0, sizeof(s->session_rom));
    symbol_table_init(&s->symbols, "Session");
    breakpoint_init(&s->breakpoints);
    api_history_clear(s);
    int n = 0, c;
    while ((c = fgetc(f)) != EOF) {
        uint32_t dst = (uint32_t)load_addr + (uint32_t)n;
        if (dst < 0x10000) s->mem.mem[dst] = (uint8_t)c;
        n++;
    }
    fclose(f);
    if (n == 0) return -1;
    binary_load_common(s, load_addr, n);
    strncpy(s->filename, path, sizeof(s->filename) - 1);
    return 0;
}

int sim_load_prg(sim_session_t *s, const char *path, uint16_t override_addr)
{
    if (!s || !path) return -1;
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    int lo = fgetc(f), hi = fgetc(f);
    if (lo == EOF || hi == EOF) { fclose(f); return -1; }
    uint16_t load_addr = override_addr
        ? override_addr
        : (uint16_t)((unsigned)lo | ((unsigned)hi << 8));
    memset(&s->mem, 0, sizeof(s->mem));
    memset(s->session_rom, 0, sizeof(s->session_rom));
    symbol_table_init(&s->symbols, "Session");
    breakpoint_init(&s->breakpoints);
    api_history_clear(s);
    int n = 0, c;
    while ((c = fgetc(f)) != EOF) {
        uint32_t dst = (uint32_t)load_addr + (uint32_t)n;
        if (dst < 0x10000) s->mem.mem[dst] = (uint8_t)c;
        n++;
    }
    fclose(f);
    binary_load_common(s, load_addr, n);
    strncpy(s->filename, path, sizeof(s->filename) - 1);
    return 0;
}

int sim_save_bin(sim_session_t *s, const char *path,
                 uint16_t addr_start, uint16_t count)
{
    if (!s || !path || count == 0) return -1;
    FILE *f = fopen(path, "wb");
    if (!f) return -1;
    for (uint32_t i = 0; i < (uint32_t)count; i++) {
        if (fputc(s->mem.mem[(uint32_t)addr_start + i], f) == EOF) {
            fclose(f); return -1;
        }
    }
    fclose(f);
    return 0;
}

int sim_save_prg(sim_session_t *s, const char *path,
                 uint16_t addr_start, uint16_t count)
{
    if (!s || !path || count == 0) return -1;
    FILE *f = fopen(path, "wb");
    if (!f) return -1;
    fputc((int)(addr_start & 0xFF),        f);
    fputc((int)((addr_start >> 8) & 0xFF), f);
    for (uint32_t i = 0; i < (uint32_t)count; i++) {
        if (fputc(s->mem.mem[(uint32_t)addr_start + i], f) == EOF) {
            fclose(f); return -1;
        }
    }
    fclose(f);
    return 0;
}

void sim_get_load_info(sim_session_t *s, uint16_t *addr_out, uint16_t *size_out)
{
    if (!s) return;
    if (addr_out) *addr_out = s->start_addr;
    if (size_out) *size_out = s->load_size;
}

int sim_step(sim_session_t *s, int count) {
    if (!s || s->state == SIM_IDLE || s->state == SIM_FINISHED) return -1;
    for (int i = 0; i < count; i++) {
        s->mem.mem_writes = 0;
        int tr = handle_trap(&s->symbols, &s->cpu, &s->mem);
        if (tr < 0) { s->state = SIM_FINISHED; if (s->event_cb) s->event_cb(s, SIM_EVENT_BRK, s->event_userdata); return SIM_EVENT_BRK; }
        if (tr > 0) continue;
        if (breakpoint_hit(&s->breakpoints, &s->cpu)) { s->state = SIM_PAUSED; if (s->event_cb) s->event_cb(s, SIM_EVENT_BREAK, s->event_userdata); return SIM_EVENT_BREAK; }
        unsigned char opc = mem_read(&s->mem, s->cpu.pc);
        if (opc == 0x00) {
            s->cpu.pc += (s->cpu_type == CPU_45GS02) ? 1 : 2;
            s->state = SIM_FINISHED;
            if (s->event_cb) s->event_cb(s, SIM_EVENT_BRK, s->event_userdata);
            return SIM_EVENT_BRK;
        }
        const dispatch_entry_t *te = peek_dispatch(&s->cpu, &s->mem, &s->dt, s->cpu_type);
        if (te && te->mnemonic && strcmp(te->mnemonic, "STP") == 0) {
            s->state = SIM_FINISHED;
            if (s->event_cb) s->event_cb(s, SIM_EVENT_STP, s->event_userdata);
            return SIM_EVENT_STP;
        }
        uint16_t pre_pc = s->cpu.pc;
        unsigned long pre_cycles = s->cpu.cycles;
        cpu_t pre_cpu = s->cpu;
        execute_from_mem(&s->cpu, &s->mem, &s->dt, s->cpu_type);
        /* Update snapshot accumulator */
        if (s->snap_active) {
            int _nw = s->mem.mem_writes < 256 ? s->mem.mem_writes : 256;
            for (int _d = 0; _d < _nw; _d++)
                snap_record_write(s, s->mem.mem_addr[_d], s->mem.mem_old_val[_d],
                                  s->mem.mem_val[_d], pre_pc);
        }
        /* Push history entry */
        if (s->hist_enabled && s->hist_buf) {
            if (s->hist_pos > 0) {
                /* Branched off history: truncate "future" entries */
                s->hist_write = (s->hist_write - s->hist_pos + s->hist_cap * 2) % s->hist_cap;
                s->hist_count -= s->hist_pos;
                if (s->hist_count < 0) s->hist_count = 0;
                s->hist_pos = 0;
            }
            sim_history_entry_t *he = &s->hist_buf[s->hist_write];
            he->pre_cpu = pre_cpu;
            he->pc      = pre_pc;
            int dc = s->mem.mem_writes < 16 ? s->mem.mem_writes : 16;
            he->delta_count = (uint8_t)dc;
            for (int d = 0; d < dc; d++) {
                he->delta_addr[d] = s->mem.mem_addr[d];
                he->delta_old[d]  = s->mem.mem_old_val[d];
            }
            s->hist_write = (s->hist_write + 1) & s->hist_mask;
            if (s->hist_count < s->hist_cap) s->hist_count++;
        }
        if (s->trace_enabled) {
            sim_trace_entry_t *tr = &s->trace_buf[s->trace_head];
            tr->pc = pre_pc; tr->cpu = s->cpu; tr->cycles_delta = (int)(s->cpu.cycles - pre_cycles);
            disasm_one(&s->mem, &s->dt, s->cpu_type, pre_pc, tr->disasm, (int)sizeof(tr->disasm));
            s->trace_head = (s->trace_head + 1) % SIM_TRACE_DEPTH;
            if (s->trace_count < SIM_TRACE_DEPTH) s->trace_count++;
        }
        if (s->prof_enabled) { s->prof_exec[pre_pc]++; s->prof_cycles[pre_pc] += (uint32_t)(s->cpu.cycles - pre_cycles); }
    }
    s->state = SIM_PAUSED;
    return 0;
}

int sim_step_cycles(sim_session_t *s, unsigned long max_cycles) {
    if (!s || s->state == SIM_IDLE || s->state == SIM_FINISHED) return -1;
    unsigned long start = s->cpu.cycles;
    while (s->cpu.cycles - start < max_cycles) {
        s->mem.mem_writes = 0;
        int tr = handle_trap(&s->symbols, &s->cpu, &s->mem);
        if (tr < 0) { s->state = SIM_FINISHED; if (s->event_cb) s->event_cb(s, SIM_EVENT_BRK, s->event_userdata); return SIM_EVENT_BRK; }
        if (tr > 0) continue;
        if (breakpoint_hit(&s->breakpoints, &s->cpu)) { s->state = SIM_PAUSED; if (s->event_cb) s->event_cb(s, SIM_EVENT_BREAK, s->event_userdata); return SIM_EVENT_BREAK; }
        unsigned char opc = mem_read(&s->mem, s->cpu.pc);
        if (opc == 0x00) {
            s->cpu.pc += (s->cpu_type == CPU_45GS02) ? 1 : 2;
            s->state = SIM_FINISHED;
            if (s->event_cb) s->event_cb(s, SIM_EVENT_BRK, s->event_userdata);
            return SIM_EVENT_BRK;
        }
        const dispatch_entry_t *te = peek_dispatch(&s->cpu, &s->mem, &s->dt, s->cpu_type);
        if (te && te->mnemonic && strcmp(te->mnemonic, "STP") == 0) {
            s->state = SIM_FINISHED;
            if (s->event_cb) s->event_cb(s, SIM_EVENT_STP, s->event_userdata);
            return SIM_EVENT_STP;
        }
        uint16_t pre_pc = s->cpu.pc;
        unsigned long pre_cycles = s->cpu.cycles;
        cpu_t pre_cpu = s->cpu;
        execute_from_mem(&s->cpu, &s->mem, &s->dt, s->cpu_type);
        /* Update snapshot accumulator */
        if (s->snap_active) {
            int _nw = s->mem.mem_writes < 256 ? s->mem.mem_writes : 256;
            for (int _d = 0; _d < _nw; _d++)
                snap_record_write(s, s->mem.mem_addr[_d], s->mem.mem_old_val[_d],
                                  s->mem.mem_val[_d], pre_pc);
        }
        if (s->hist_enabled && s->hist_buf) {
            if (s->hist_pos > 0) {
                s->hist_write = (s->hist_write - s->hist_pos + s->hist_cap * 2) % s->hist_cap;
                s->hist_count -= s->hist_pos;
                if (s->hist_count < 0) s->hist_count = 0;
                s->hist_pos = 0;
            }
            sim_history_entry_t *he = &s->hist_buf[s->hist_write];
            he->pre_cpu = pre_cpu;
            he->pc      = pre_pc;
            int dc = s->mem.mem_writes < 16 ? s->mem.mem_writes : 16;
            he->delta_count = (uint8_t)dc;
            for (int d = 0; d < dc; d++) {
                he->delta_addr[d] = s->mem.mem_addr[d];
                he->delta_old[d]  = s->mem.mem_old_val[d];
            }
            s->hist_write = (s->hist_write + 1) & s->hist_mask;
            if (s->hist_count < s->hist_cap) s->hist_count++;
        }
        if (s->trace_enabled) {
            sim_trace_entry_t *te2 = &s->trace_buf[s->trace_head];
            te2->pc = pre_pc; te2->cpu = s->cpu; te2->cycles_delta = (int)(s->cpu.cycles - pre_cycles);
            disasm_one(&s->mem, &s->dt, s->cpu_type, pre_pc, te2->disasm, (int)sizeof(te2->disasm));
            s->trace_head = (s->trace_head + 1) % SIM_TRACE_DEPTH;
            if (s->trace_count < SIM_TRACE_DEPTH) s->trace_count++;
        }
        if (s->prof_enabled) { s->prof_exec[pre_pc]++; s->prof_cycles[pre_pc] += (uint32_t)(s->cpu.cycles - pre_cycles); }
    }
    s->state = SIM_PAUSED;
    return 0;
}

void sim_reset(sim_session_t *s) {
    if (!s) return;
    cpu_init(&s->cpu); s->cpu.pc = s->start_addr;
    if (s->cpu_type == CPU_45GS02) set_flag(&s->cpu, FLAG_E, 1);
    s->state = (s->filename[0] != '\0') ? SIM_READY : SIM_IDLE;
}

int sim_disassemble_one(sim_session_t *s, uint16_t addr, char *buf, size_t len) {
    if (!s || !buf || len == 0) return 1;
    if (s->state == SIM_IDLE) { snprintf(buf, len, "%04X: --", addr); return 1; }
    return disasm_one(&s->mem, &s->dt, s->cpu_type, addr, buf, (int)len);
}

cpu_t          *sim_get_cpu(sim_session_t *s)    { return s ? &s->cpu : NULL; }
const memory_t *sim_get_memory(sim_session_t *s) { return s ? &s->mem : NULL; }
uint8_t sim_mem_read_byte(sim_session_t *s, uint16_t addr) { return s ? mem_read(&s->mem, addr) : 0; }
void sim_mem_write_byte(sim_session_t *s, uint16_t addr, uint8_t val) { if (s) mem_write(&s->mem, addr, val); }
sim_state_t sim_get_state(sim_session_t *s) { return s ? s->state : SIM_IDLE; }
const char *sim_get_filename(sim_session_t *s) { return (s && s->filename[0]) ? s->filename : "(none)"; }
const char *sim_processor_name(sim_session_t *s) { return s ? processor_name_local(s->cpu_type) : "6502"; }
const char *sim_state_name(sim_state_t state) {
    switch (state) {
    case SIM_IDLE:     return "IDLE";
    case SIM_READY:    return "READY";
    case SIM_PAUSED:   return "PAUSED";
    case SIM_FINISHED: return "FINISHED";
    default:           return "UNKNOWN";
    }
}
void sim_set_processor(sim_session_t *s, const char *name) {
    if (!s || !name) return;
    if      (strcmp(name, "6502") == 0) s->cpu_type = CPU_6502;
    else if (strcmp(name, "6502-undoc") == 0) s->cpu_type = CPU_6502_UNDOCUMENTED;
    else if (strcmp(name, "65c02") == 0) s->cpu_type = CPU_65C02;
    else if (strcmp(name, "65ce02") == 0) s->cpu_type = CPU_65CE02;
    else if (strcmp(name, "45gs02") == 0) s->cpu_type = CPU_45GS02;
    api_select_handlers(s);
    dispatch_build(&s->dt, s->handlers, s->num_handlers, s->cpu_type);
}
cpu_type_t sim_get_cpu_type(sim_session_t *s) { return s ? s->cpu_type : CPU_6502; }
void sim_set_event_callback(sim_session_t *s, sim_event_cb cb, void *userdata) { if (s) { s->event_cb = cb; s->event_userdata = userdata; } }
int sim_break_set(sim_session_t *s, uint16_t addr, const char *cond) { return s ? breakpoint_add(&s->breakpoints, addr, cond) : 0; }
void sim_break_clear(sim_session_t *s, uint16_t addr) { if (s) breakpoint_remove(&s->breakpoints, addr); }
const char *sim_sym_by_addr(sim_session_t *s, uint16_t addr) { return s ? symbol_lookup_addr_name(&s->symbols, addr) : NULL; }
int sim_break_is_enabled(sim_session_t *s, int idx) { if (!s || idx < 0 || idx >= s->breakpoints.count) return 0; return s->breakpoints.breakpoints[idx].enabled; }
int sim_break_toggle(sim_session_t *s, int idx) { if (!s || idx < 0 || idx >= s->breakpoints.count) return -1; s->breakpoints.breakpoints[idx].enabled = !s->breakpoints.breakpoints[idx].enabled; return s->breakpoints.breakpoints[idx].enabled; }
void sim_trace_enable(sim_session_t *s, int enable) { if (s) s->trace_enabled = enable; }
int sim_trace_is_enabled(sim_session_t *s) { return s ? s->trace_enabled : 0; }
void sim_trace_clear(sim_session_t *s) { if (s) { s->trace_head = 0; s->trace_count = 0; } }
int sim_trace_count(sim_session_t *s) { return s ? s->trace_count : 0; }
int sim_trace_get(sim_session_t *s, int slot, sim_trace_entry_t *entry) {
    if (!s || slot < 0 || slot >= s->trace_count) return 0;
    int idx = (s->trace_head - 1 - slot + SIM_TRACE_DEPTH) % SIM_TRACE_DEPTH;
    *entry = s->trace_buf[idx]; return 1;
}
int sim_has_breakpoint(sim_session_t *s, uint16_t addr) { if (!s) return 0; for (int i = 0; i < s->breakpoints.count; i++) if (s->breakpoints.breakpoints[i].address == addr) return 1; return 0; }
int sim_get_opcode_cycles(sim_session_t *s, uint16_t addr) {
    if (!s) return 0;
    const dispatch_entry_t *e = peek_dispatch(&s->cpu, &s->mem, &s->dt, s->cpu_type);
    return e ? e->cycles : 0;
}
int sim_get_last_writes(sim_session_t *s, uint16_t *addrs, int max_count) {
    if (!s || !addrs || max_count <= 0) return 0;
    int n = s->mem.mem_writes < 256 ? s->mem.mem_writes : 256;
    if (n > max_count) n = max_count;
    for (int i = 0; i < n; i++) addrs[i] = s->mem.mem_addr[i];
    return n;
}
void sim_set_pc(sim_session_t *s, uint16_t pc) { if (s) s->cpu.pc = pc; }
void sim_set_reg_byte(sim_session_t *s, const char *name, uint8_t val) {
    if (!s || !name) return;
    if (strcmp(name, "A") == 0) s->cpu.a = val;
    else if (strcmp(name, "X") == 0) s->cpu.x = val;
    else if (strcmp(name, "Y") == 0) s->cpu.y = val;
    else if (strcmp(name, "Z") == 0) s->cpu.z = val;
    else if (strcmp(name, "B") == 0) s->cpu.b = val;
    else if (strcmp(name, "S") == 0) s->cpu.s = val;
    else if (strcmp(name, "P") == 0) s->cpu.p = val;
}
int sim_break_count(sim_session_t *s) { return s ? s->breakpoints.count : 0; }
int sim_break_get(sim_session_t *s, int idx, uint16_t *addr, char *cond, int cond_sz) {
    if (!s || idx < 0 || idx >= s->breakpoints.count) return 0;
    if (addr) *addr = s->breakpoints.breakpoints[idx].address;
    if (cond && s->breakpoints.breakpoints[idx].condition[0]) strncpy(cond, s->breakpoints.breakpoints[idx].condition, cond_sz);
    return 1;
}
int sim_opcode_count(sim_session_t *s) { return s ? s->num_handlers : 0; }
int sim_opcode_get(sim_session_t *s, int idx, sim_opcode_info_t *info) {
    if (!s || idx < 0 || idx >= s->num_handlers || !info) return 0;
    strncpy(info->mnemonic, s->handlers[idx].mnemonic, 7); info->mnemonic[7] = 0;
    info->mode = s->handlers[idx].mode;
    memcpy(info->opcode_bytes, s->handlers[idx].opcode_bytes, 4);
    info->opcode_len = s->handlers[idx].opcode_len;
    info->cycles = s->handlers[idx].cycles_6502;
    info->instr_bytes = get_instruction_length(info->mode) + info->opcode_len - 1;
    return 1;
}
int sim_opcode_by_byte(sim_session_t *s, uint8_t byte_val, sim_opcode_info_t *info) {
    if (!s || !info) return 0;
    const dispatch_entry_t *e = &s->dt.base[byte_val];
    if (!e || !e->mnemonic) return 0;
    strncpy(info->mnemonic, e->mnemonic, 7); info->mnemonic[7] = 0;
    info->mode = e->mode;
    info->cycles = e->cycles;
    info->instr_bytes = get_instruction_length(e->mode);
    return 1;
}
int sim_sym_count(sim_session_t *s) { return s ? s->symbols.count : 0; }
int sim_sym_get_idx(sim_session_t *s, int idx, uint16_t *addr, char *name_buf, int name_sz, int *type_out, char *comment_buf, int comment_sz) {
    if (!s || idx < 0 || idx >= s->symbols.count) return 0;
    if (addr) *addr = s->symbols.symbols[idx].address;
    if (name_buf) strncpy(name_buf, s->symbols.symbols[idx].name, name_sz);
    if (type_out) *type_out = s->symbols.symbols[idx].type;
    if (comment_buf) strncpy(comment_buf, s->symbols.symbols[idx].comment, comment_sz);
    return 1;
}
const char *sim_sym_type_name(int type) {
    switch (type) {
    case SYM_LABEL: return "LABEL";
    case SYM_TRAP: return "TRAP";
    default: return "UNKNOWN";
    }
}
int sim_sym_remove_idx(sim_session_t *s, int idx) { (void)s; (void)idx; return 0; }
int sim_sym_add(sim_session_t *s, uint16_t addr, const char *name, const char *type_str) {
    if (!s || !name || !type_str) return 0;
    symbol_type_t type = SYM_LABEL;
    if (strcmp(type_str, "TRAP") == 0) type = SYM_TRAP;
    return symbol_add(&s->symbols, name, addr, type, "API");
}
int sim_sym_load_file(sim_session_t *s, const char *path) { return s ? symbol_load_file(&s->symbols, path) : 0; }
/* --- Execution History --- */

void sim_history_enable(sim_session_t *s, int enable) { if (s) s->hist_enabled = enable; }
int  sim_history_is_enabled(sim_session_t *s)         { return s ? s->hist_enabled : 0; }
void sim_history_clear(sim_session_t *s)               { if (s) api_history_clear(s); }
int  sim_history_depth(sim_session_t *s)               { return s ? s->hist_cap   : 0; }
int  sim_history_count(sim_session_t *s)               { return s ? s->hist_count : 0; }
int  sim_history_position(sim_session_t *s)            { return s ? s->hist_pos   : 0; }

int sim_history_step_back(sim_session_t *s) {
    if (!s || !s->hist_buf || s->hist_pos >= s->hist_count) return 0;
    /* Index of the entry to undo (most recent unvisited) */
    unsigned idx = ((unsigned)s->hist_write - 1u - (unsigned)s->hist_pos) & (unsigned)s->hist_mask;
    sim_history_entry_t *he = &s->hist_buf[idx];
    /* Restore CPU */
    s->cpu = he->pre_cpu;
    /* Restore memory: write old values directly (no write-log side effect) */
    for (int d = 0; d < he->delta_count; d++)
        s->mem.mem[he->delta_addr[d]] = he->delta_old[d];
    s->hist_pos++;
    s->state = SIM_PAUSED;
    return 1;
}

int sim_history_step_fwd(sim_session_t *s) {
    if (!s || !s->hist_buf || s->hist_pos == 0) return 0;
    /* Index of the entry to re-execute */
    unsigned idx = ((unsigned)s->hist_write - (unsigned)s->hist_pos) & (unsigned)s->hist_mask;
    sim_history_entry_t *he = &s->hist_buf[idx];
    /* Restore CPU to pre-execution state, then re-execute */
    s->cpu = he->pre_cpu;
    s->mem.mem_writes = 0;
    execute_from_mem(&s->cpu, &s->mem, &s->dt, s->cpu_type);
    s->hist_pos--;
    s->state = SIM_PAUSED;
    return 1;
}

int sim_history_get(sim_session_t *s, int slot, sim_history_entry_t *entry) {
    if (!s || !s->hist_buf || !entry || slot < 0 || slot >= s->hist_count) return 0;
    unsigned idx = ((unsigned)s->hist_write - 1u - (unsigned)slot) & (unsigned)s->hist_mask;
    *entry = s->hist_buf[idx];
    return 1;
}

/* --- Profiler --- */

void sim_profiler_enable(sim_session_t *s, int enable) { if (s) s->prof_enabled = enable; }
int sim_profiler_is_enabled(sim_session_t *s) { return s ? s->prof_enabled : 0; }
void sim_profiler_clear(sim_session_t *s) { if (s) { memset(s->prof_exec, 0, sizeof(s->prof_exec)); memset(s->prof_cycles, 0, sizeof(s->prof_cycles)); } }
uint32_t sim_profiler_get_exec(sim_session_t *s, uint16_t addr) { return s ? s->prof_exec[addr] : 0; }
uint32_t sim_profiler_get_cycles(sim_session_t *s, uint16_t addr) { return s ? s->prof_cycles[addr] : 0; }
int sim_profiler_top_exec(sim_session_t *s, uint16_t *out_addrs, uint32_t *out_counts, int max_n) { (void)s; (void)out_addrs; (void)out_counts; (void)max_n; return 0; }
const char *sim_mode_name(unsigned char mode) { return mode_name(mode); }

/* --- Memory Snapshot & Diff --- */

static int diff_entry_cmp(const void *a, const void *b) {
    return (int)((const sim_diff_entry_t *)a)->addr - (int)((const sim_diff_entry_t *)b)->addr;
}

void sim_snapshot_take(sim_session_t *s) {
    if (!s) return;
    for (int i = 0; i < 256; i++) {
        snap_node_t *n = s->snap_buckets[i];
        while (n) { snap_node_t *nx = n->next; free(n); n = nx; }
        s->snap_buckets[i] = NULL;
    }
    s->snap_active = 1;
}

int sim_snapshot_valid(sim_session_t *s) { return s ? s->snap_active : 0; }

int sim_snapshot_diff(sim_session_t *s, sim_diff_entry_t *entries, int entries_cap) {
    if (!s || !s->snap_active || !entries || entries_cap <= 0) return -1;
    int count = 0;
    for (int i = 0; i < 256; i++) {
        snap_node_t *n = s->snap_buckets[i];
        while (n) {
            if (n->before != n->after && count < entries_cap) {
                entries[count].addr      = n->addr;
                entries[count].before    = n->before;
                entries[count].after     = n->after;
                entries[count].writer_pc = n->writer_pc;
                count++;
            }
            n = n->next;
        }
    }
    qsort(entries, (size_t)count, sizeof(sim_diff_entry_t), diff_entry_cmp);
    return count;
}

/* --- Trace Run --- */

int sim_trace_run(sim_session_t *s,
                  int start_addr, int max_instr, int stop_on_brk,
                  sim_trace_entry_t *entries, int entries_cap,
                  char *stop_reason_out, int stop_reason_sz)
{
    if (!s || !entries || entries_cap <= 0 || max_instr <= 0) {
        if (stop_reason_out && stop_reason_sz > 0) strncpy(stop_reason_out, "count", stop_reason_sz - 1);
        return 0;
    }
    if (start_addr >= 0) s->cpu.pc = (uint16_t)start_addr;
    if (max_instr > entries_cap) max_instr = entries_cap;

    const char *reason = "count";
    int n = 0;

    while (n < max_instr) {
        s->mem.mem_writes = 0;
        int tr = handle_trap(&s->symbols, &s->cpu, &s->mem);
        if (tr < 0) { reason = "trap"; break; }
        if (tr > 0) continue;

        if (breakpoint_hit(&s->breakpoints, &s->cpu)) { reason = "bp"; break; }

        uint16_t pre_pc = s->cpu.pc;
        unsigned char opc = mem_read(&s->mem, pre_pc);

        /* Disassemble before executing */
        disasm_one(&s->mem, &s->dt, s->cpu_type, pre_pc, entries[n].disasm,
                   (int)sizeof(entries[n].disasm));
        entries[n].pc = pre_pc;

        if (opc == 0x00) {
            entries[n].cpu = s->cpu;
            entries[n].cycles_delta = 0;
            n++;
            reason = "brk";
            s->state = SIM_FINISHED;
            break;  /* always stop at BRK (recording it) regardless of stop_on_brk */
        }

        const dispatch_entry_t *te = peek_dispatch(&s->cpu, &s->mem, &s->dt, s->cpu_type);
        if (te && te->mnemonic && strcmp(te->mnemonic, "STP") == 0) {
            entries[n].cpu = s->cpu;
            entries[n].cycles_delta = 0;
            n++;
            reason = "stp";
            s->state = SIM_FINISHED;
            break;
        }

        unsigned long pre_cycles = s->cpu.cycles;
        execute_from_mem(&s->cpu, &s->mem, &s->dt, s->cpu_type);
        entries[n].cpu = s->cpu;
        entries[n].cycles_delta = (int)(s->cpu.cycles - pre_cycles);
        if (s->snap_active) {
            int _nw = s->mem.mem_writes < 256 ? s->mem.mem_writes : 256;
            for (int _d = 0; _d < _nw; _d++)
                snap_record_write(s, s->mem.mem_addr[_d], s->mem.mem_old_val[_d],
                                  s->mem.mem_val[_d], pre_pc);
        }
        n++;
    }

    if (s->state != SIM_FINISHED) s->state = SIM_PAUSED;
    if (stop_reason_out && stop_reason_sz > 0) {
        strncpy(stop_reason_out, reason, (size_t)(stop_reason_sz - 1));
        stop_reason_out[stop_reason_sz - 1] = '\0';
    }
    return n;
}

/* --- Validate Routine --- */

int sim_validate_routine(sim_session_t          *s,
                         uint16_t                routine_addr,
                         uint16_t                scratch_addr,
                         int                     max_steps,
                         const sim_test_in_t    *inputs,
                         const sim_test_expect_t *expects,
                         sim_test_result_t      *results,
                         int                     count)
{
    if (!s || !inputs || !expects || !results || count <= 0) return 0;
    if (scratch_addr == 0) scratch_addr = 0xFFF8;
    if (max_steps    <= 0) max_steps    = 100000;

    /* Save 4 bytes at scratch area */
    uint8_t saved[4];
    for (int i = 0; i < 4; i++) saved[i] = s->mem.mem[(scratch_addr + i) & 0xFFFF];

    /* Write: JSR routine_addr (3 bytes) + BRK (1 byte) */
    s->mem.mem[(scratch_addr    ) & 0xFFFF] = 0x20;
    s->mem.mem[(scratch_addr + 1) & 0xFFFF] = (uint8_t)(routine_addr & 0xFF);
    s->mem.mem[(scratch_addr + 2) & 0xFFFF] = (uint8_t)(routine_addr >> 8);
    s->mem.mem[(scratch_addr + 3) & 0xFFFF] = 0x00;

    cpu_t       saved_cpu   = s->cpu;
    sim_state_t saved_state = s->state;
    int         total_pass  = 0;

    for (int t = 0; t < count; t++) {
        const sim_test_in_t     *in  = &inputs[t];
        const sim_test_expect_t *exp = &expects[t];
        sim_test_result_t       *res = &results[t];

        /* Reset registers to neutral state; keep memory */
        cpu_init(&s->cpu);
        if (s->cpu_type == CPU_45GS02) set_flag(&s->cpu, FLAG_E, 1);
        s->cpu.pc = scratch_addr;

        /* Apply input register overrides */
        if (in->a >= 0) s->cpu.a = (uint8_t)in->a;
        if (in->x >= 0) s->cpu.x = (uint8_t)in->x;
        if (in->y >= 0) s->cpu.y = (uint8_t)in->y;
        if (in->z >= 0) s->cpu.z = (uint8_t)in->z;
        if (in->b >= 0) s->cpu.b = (uint8_t)in->b;
        if (in->s >= 0) s->cpu.s = (uint8_t)in->s;
        if (in->p >= 0) s->cpu.p = (uint8_t)in->p;

        /* Apply input memory writes */
        for (int m = 0; m < in->mem_count && m < SIM_VALIDATE_MEM_OPS; m++)
            s->mem.mem[in->mem_addr[m]] = in->mem_val[m];

        /* Execute until BRK or step limit */
        s->state = SIM_READY;
        int ev = sim_step(s, max_steps);

        /* Capture actual register values */
        res->a = s->cpu.a;  res->x = s->cpu.x;
        res->y = s->cpu.y;  res->z = s->cpu.z;
        res->b = s->cpu.b;  res->s = s->cpu.s;  res->p = s->cpu.p;
        res->fail_msg[0] = '\0';
        res->passed = 1;

        if (ev != SIM_EVENT_BRK && ev != SIM_EVENT_STP) {
            /* Did not complete normally */
            res->passed = 0;
            const char *why = (ev == 0) ? "timeout" :
                              (ev == SIM_EVENT_BREAK) ? "breakpoint" : "trap";
            snprintf(res->fail_msg, sizeof(res->fail_msg),
                     "did not complete (%s at $%04X, %d steps)",
                     why, (unsigned)s->cpu.pc, max_steps);
        } else {
            /* Compare expected register values */
            char buf[32];
#define VR(NM, F) \
            if (exp->F >= 0 && res->F != (uint8_t)(exp->F)) { \
                snprintf(buf, sizeof(buf), NM "=$%02X exp $%02X; ", \
                         (unsigned)(uint8_t)res->F, (unsigned)(uint8_t)exp->F); \
                strncat(res->fail_msg, buf, sizeof(res->fail_msg) - strlen(res->fail_msg) - 1); \
                res->passed = 0; \
            }
            VR("A",a) VR("X",x) VR("Y",y) VR("Z",z) VR("B",b) VR("P",p)
#undef VR
            /* Compare expected memory values */
            for (int m = 0; m < exp->mem_count && m < SIM_VALIDATE_MEM_OPS; m++) {
                uint8_t got = s->mem.mem[exp->mem_addr[m]];
                if (got != exp->mem_val[m]) {
                    snprintf(buf, sizeof(buf), "[$%04X]=$%02X exp $%02X; ",
                             (unsigned)exp->mem_addr[m], (unsigned)got,
                             (unsigned)exp->mem_val[m]);
                    strncat(res->fail_msg, buf, sizeof(res->fail_msg) - strlen(res->fail_msg) - 1);
                    res->passed = 0;
                }
            }
        }
        if (res->passed) total_pass++;
    }

    /* Restore scratch area and CPU */
    for (int i = 0; i < 4; i++) s->mem.mem[(scratch_addr + i) & 0xFFFF] = saved[i];
    s->cpu   = saved_cpu;
    s->state = saved_state;
    return total_pass;
}
