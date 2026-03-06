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
    uint32_t          prof_exec[65536];
    uint32_t          prof_cycles[65536];
    int               prof_enabled;
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
    return s;
}

void sim_destroy(sim_session_t *s) {
    if (!s) return;
    for (int i = 0; i < FAR_NUM_PAGES; i++) {
        if (s->mem.far_pages[i]) { free(s->mem.far_pages[i]); s->mem.far_pages[i] = NULL; }
    }
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
    cpu_type_t cpu_type = s->cpu_type;
    api_select_handlers(s);
    char line[512];
    int pc = 0x0200;
    while (fgets(line, sizeof(line), f)) {
        char *ptr = line;
        while (*ptr && isspace(*ptr)) ptr++;
        if (!*ptr || *ptr == ';') continue;
        if (*ptr == '.') { handle_pseudo_op(ptr, &cpu_type, &pc, NULL, NULL, NULL); continue; }
        char *colon = strchr(ptr, ':');
        if (colon) {
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
        const char *colon = strchr(ptr, ':');
        if (colon) {
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
    s->mem.mem_writes = 0;
    for (int i = 0; i < count; i++) {
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
        execute_from_mem(&s->cpu, &s->mem, &s->dt, s->cpu_type);
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

cpu_t *sim_get_cpu(sim_session_t *s) { return s ? &s->cpu : NULL; }
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
int sim_get_last_writes(sim_session_t *s, uint16_t *addrs, int max_count) { (void)s; (void)addrs; (void)max_count; return 0; }
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
void sim_profiler_enable(sim_session_t *s, int enable) { if (s) s->prof_enabled = enable; }
int sim_profiler_is_enabled(sim_session_t *s) { return s ? s->prof_enabled : 0; }
void sim_profiler_clear(sim_session_t *s) { if (s) { memset(s->prof_exec, 0, sizeof(s->prof_exec)); memset(s->prof_cycles, 0, sizeof(s->prof_cycles)); } }
uint32_t sim_profiler_get_exec(sim_session_t *s, uint16_t addr) { return s ? s->prof_exec[addr] : 0; }
uint32_t sim_profiler_get_cycles(sim_session_t *s, uint16_t addr) { return s ? s->prof_cycles[addr] : 0; }
int sim_profiler_top_exec(sim_session_t *s, uint16_t *out_addrs, uint32_t *out_counts, int max_n) { (void)s; (void)out_addrs; (void)out_counts; (void)max_n; return 0; }
const char *sim_mode_name(unsigned char mode) { return mode_name(mode); }
