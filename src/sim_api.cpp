#include "version.h"
#include "debug_context.h"
#include "cpu.h"
#include "memory.h"
#include "opcodes.h"
#include "cycles.h"
#include "breakpoints.h"
#include "trace.h"
#include "symbols.h"
#include "list_parser.h"
#include "metadata.h"
#include "disassembler.h"
#include "patterns.h"
#include "cpu_engine.h"
#include "cpu_6502.h"
#include "cli/commands.h"
#include "cli/commands/CommandRegistry.h"
#include "device/mega65_io.h"
#include "device/vic2_io.h"
#include "device/vic2.h"
#include "device/sid_io.h"
#include "device/cia_io.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <vector>
#include <algorithm>
#include <chrono>

extern int g_verbose;
#define LOG_V2(...) if (g_verbose >= 2) fprintf(stderr, __VA_ARGS__)

/* Full definition of the opaque sim_session_t handle. */
struct sim_session {
    CPU              *cpu;
    memory_t          *mem;
    symbol_table_t    *symbols;
    source_map_t      *source_map;
    breakpoint_list_t *breakpoints;
    machine_type_t    machine_type;
    cpu_type_t        cpu_type;
    sim_state_t       state;
    unsigned short    start_addr;
    unsigned short    load_size;   /* byte count of the loaded binary (best-effort) */
    char              filename[512];
    char              last_error[2048];
    sim_event_cb      event_cb;
    void             *event_userdata;
    sim_log_cb        log_cb;
    void             *log_userdata;
    DebugContext     *debug_ctx;
    std::vector<IOHandler*> dynamic_handlers;
    std::vector<sim_opcode_info_t> opcodes_cache;

    sim_session() {
        cpu = nullptr;
        mem = new memory_t();
        memset(mem, 0, sizeof(memory_t));
        symbols = new symbol_table_t();
        memset(symbols, 0, sizeof(symbol_table_t));
        source_map = new source_map_t();
        source_map_init(source_map);
        breakpoints = new breakpoint_list_t();
        memset(breakpoints, 0, sizeof(breakpoint_list_t));
        
        machine_type = MACHINE_C64;
        cpu_type = CPU_6502;
        state = SIM_IDLE;
        start_addr = 0x0200;
        load_size = 0;
        filename[0] = '\0';
        last_error[0] = '\0';
        event_cb = nullptr;
        event_userdata = nullptr;
        log_cb = nullptr;
        log_userdata = nullptr;
        debug_ctx = nullptr;
    }

    ~sim_session() {
        if (source_map) delete source_map;
        if (mem) delete mem;
        if (symbols) delete symbols;
        if (breakpoints) delete breakpoints;
    }
};

/* --- Internal Helpers --- */

static void update_opcode_cache(sim_session_t *s) {
    s->opcodes_cache.clear();
    if (!s->cpu) return;
    dispatch_table_t *dt = s->cpu->dispatch_table();
    if (!dt) return;

    auto add_from_table = [&](dispatch_entry_t *table, int prefix_len, uint8_t *prefix) {
        for (int i = 0; i < 256; i++) {
            if (table[i].fn) {
                sim_opcode_info_t info;
                memset(&info, 0, sizeof(info));
                strncpy(info.mnemonic, table[i].mnemonic, sizeof(info.mnemonic) - 1);
                info.mnemonic[sizeof(info.mnemonic) - 1] = '\0';
                info.mode = table[i].mode;
                info.opcode_len = (unsigned char)(prefix_len + 1);
                for (int j = 0; j < prefix_len; j++) info.opcode_bytes[j] = prefix[j];
                info.opcode_bytes[prefix_len] = (uint8_t)i;
                info.cycles = table[i].cycles;
                info.instr_bytes = get_instruction_length(info.mode);
                s->opcodes_cache.push_back(info);
            }
        }
    };

    uint8_t p_none[1] = {0}; // dummy
    add_from_table(dt->base, 0, p_none);

    uint8_t p_eom[1] = {0xEA};
    add_from_table(dt->eom, 1, p_eom);

    uint8_t p_quad[2] = {0x42, 0x42};
    add_from_table(dt->quad, 2, p_quad);

    uint8_t p_quad_eom[3] = {0x42, 0x42, 0xEA};
    add_from_table(dt->quad_eom, 3, p_quad_eom);
}

static int handle_trap(const symbol_table_t *st, CPU *cpu, memory_t *mem) {
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

static void machine_init_hardware(sim_session_t *s); /* forward decl */

/* Return the most appropriate machine type for a given CPU when no explicit
 * machine has been specified.  Mirrors the defaults used in sim_create(). */
static machine_type_t default_machine_for_cpu(cpu_type_t cpu) {
    switch (cpu) {
    case CPU_45GS02:
    case CPU_65CE02:  return MACHINE_MEGA65;
    case CPU_65C02:   return MACHINE_X16;
    default:          return MACHINE_C64;
    }
}

/* Replace the CPU object when the processor type changes.
 * Preserves the memory pointer; call before machine_init_hardware. */
static void apply_cpu_type(sim_session_t *s, cpu_type_t new_type) {
    if (s->cpu && new_type == s->cpu_type) return;
    s->cpu_type = new_type;
    delete s->cpu;
    s->cpu = CPUFactory::create(new_type);
    s->cpu->mem = s->mem;
    update_opcode_cache(s);
}

/* After loading, scan the symbol table for SYM_PROCESSOR symbols emitted by the
 * metadata pipeline (SIM_CPU / SIM_MACHINE markers from .print injection or .sym_add).
 * Applies any cpu/machine type changes found, re-initialising hardware if needed.
 * When only sim_cpu is present (no explicit sim_machine), the machine type is
 * derived automatically via default_machine_for_cpu(). */
static void apply_session_processor_symbols(sim_session_t *s) {
    bool has_cpu_sym = false, has_machine_sym = false;
    for (int i = 0; i < s->symbols->count; i++) {
        const symbol_t *sym = &s->symbols->symbols[i];
        if (sym->type != SYM_PROCESSOR) continue;
        if (strcmp(sym->name, "sim_cpu")     == 0) has_cpu_sym     = true;
        if (strcmp(sym->name, "sim_machine") == 0) has_machine_sym = true;
    }
    bool cpu_changed = false, machine_changed = false;
    if (has_cpu_sym) {
        cpu_type_t t = cpu_type_from_symbols(s->symbols);
        if (t != s->cpu_type) { apply_cpu_type(s, t); cpu_changed = true; }
    }
    if (has_machine_sym) {
        machine_type_t m = machine_type_from_symbols(s->symbols);
        if (m != s->machine_type) { s->machine_type = m; machine_changed = true; }
    } else if (has_cpu_sym) {
        /* No explicit machine directive — derive a sensible default for the CPU.
         * Uses has_cpu_sym (not cpu_changed) so this fires even when the CPU type
         * was already set by an early detect_asm_cpu_type() scan. */
        machine_type_t m = default_machine_for_cpu(s->cpu_type);
        if (m != s->machine_type) { s->machine_type = m; machine_changed = true; }
    }
    if (cpu_changed || machine_changed) {
        s->cpu->mem = s->mem;
        machine_init_hardware(s);
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

static const char *machine_name_local(machine_type_t type) {
    switch (type) {
    case MACHINE_RAW6502: return "raw6502";
    case MACHINE_C64:     return "c64";
    case MACHINE_C128:    return "c128";
    case MACHINE_MEGA65:  return "mega65";
    case MACHINE_X16:     return "x16";
    default:              return "unknown";
    }
}

static void sim_init_vic2_defaults(sim_session_t *s) {
    if (!s) return;
    /* Common VIC-II colour / mode defaults */
    sim_mem_write_byte(s, 0xD011, 0x1B); /* ECM=0 BMM=0 DEN=1 RSEL=1 yscroll=3  */
    sim_mem_write_byte(s, 0xD016, 0x08); /* MCM=0 CSEL=1 xscroll=0               */
    sim_mem_write_byte(s, 0xD020, 0x0E); /* border: light blue (14)               */
    sim_mem_write_byte(s, 0xD021, 0x06); /* BG0:   blue (6)                       */
    sim_mem_write_byte(s, 0xD022, 0x01); /* BG1/MC1: white (1)                    */
    sim_mem_write_byte(s, 0xD023, 0x02); /* BG2/MC2: red (2)                      */

    if (s->machine_type == MACHINE_MEGA65) {
        /* TEMPORARY: Mega65 VIC-IV charset addressing uses CHARPTR ($D068–$D06A).
         * Bank 1 ($4000–$7FFF) has no hardwired char-ROM exception, so the VIC
         * pane still reads charset from RAM at $4000 until VIC-IV CHARPTR
         * emulation is implemented (see vic2.h TODO).
         *   DD00 bits[1:0]=10  → VIC bank 1 ($4000–$7FFF)
         *   D018 VMA=2, CB=0   → screen $4800, charset $4000               */
        sim_mem_write_byte(s, 0xDD00, 0x36); /* CIA2 PA: bits[1:0]=10 → bank 1   */
        sim_mem_write_byte(s, 0xD018, 0x21); /* screen=$4800  charset=$4000       */
    } else {
        /* C64 / C128 / X16 / RAW: VIC bank 0; charset at VIC offset $1000 is
         * served from char_rom[] by vic_read() — no RAM collision.            */
        sim_mem_write_byte(s, 0xDD00, 0x37); /* CIA2 PA: bits[1:0]=11 → bank 0   */
        sim_mem_write_byte(s, 0xD018, 0x15); /* screen=$0400  charset=$1000       */
    }
}

static void sim_load_default_charset(sim_session_t *s) {
    if (!s) return;
    FILE *f = fopen("presets/default-pet-upper.bin", "rb");
    if (!f) return;
    uint8_t buf[2048];
    size_t n = fread(buf, 1, sizeof(buf), f);
    fclose(f);
    /* Always populate char_rom — used by vic_read() for bank-0/bank-2 VIC
     * exceptions and by mem_read() for CPU reads with CHAREN=0, HIRAM=1. */
    memcpy(s->mem->char_rom, buf, n);
    if (s->machine_type == MACHINE_MEGA65) {
        /* Mega65 character ROM lives at physical $2D000 in the 28-bit address
         * space — accessible to Mega65 programs via far/DMA reads.            */
        for (size_t i = 0; i < n; i++)
            far_mem_write(s->mem, 0x2D000 + i, buf[i]);
        /* VIC bank 1 has no hardwired ROM exception; also mirror to RAM $4000
         * so the VIC screen pane can render until VIC-IV CHARPTR is
         * implemented (see vic2.h TODO).                                       */
        for (size_t i = 0; i < n; i++)
            s->mem->mem[0x4000 + i] = buf[i];
    }
}

static void machine_init_hardware(sim_session_t *s) {
    if (s->mem->io_registry) {
        delete s->mem->io_registry;
    }
    s->mem->io_registry = new IORegistry(s->cpu->get_interrupt_controller());

    switch (s->machine_type) {
        case MACHINE_RAW6502:
            // No I/O devices registered
            break;
        case MACHINE_MEGA65:
            mega65_io_register(s->mem);
            sid_io_register(s->mem, s->machine_type, s->dynamic_handlers);
            cia_io_register(s->mem, s->dynamic_handlers);
            s->mem->io_registry->rebuild_map(s->mem);
            sim_init_vic2_defaults(s);
            sim_load_default_charset(s);
            break;
        case MACHINE_C64:
        case MACHINE_C128:
            vic2_io_register(s->mem);
            sid_io_register(s->mem, s->machine_type, s->dynamic_handlers);
            cia_io_register(s->mem, s->dynamic_handlers);
            s->mem->io_registry->rebuild_map(s->mem);
            sim_init_vic2_defaults(s);
            sim_load_default_charset(s);
            break;
        case MACHINE_X16:
        default:
            vic2_io_register(s->mem);
            sid_io_register(s->mem, s->machine_type, s->dynamic_handlers);
            cia_io_register(s->mem, s->dynamic_handlers);
            s->mem->io_registry->rebuild_map(s->mem);
            sim_init_vic2_defaults(s);
            break;
    }
}

/* --- API Implementation --- */

sim_session_t *sim_create(const char *processor) {
    sim_session_t *s = new sim_session();
    s->cpu_type = CPU_6502;
    s->machine_type = MACHINE_C64; // Default to C64 for backward compatibility with 6502 scripts
    if (processor) {
        if      (strcmp(processor, "6502-undoc") == 0) s->cpu_type = CPU_6502_UNDOCUMENTED;
        else if (strcmp(processor, "65c02")      == 0) { s->cpu_type = CPU_65C02; s->machine_type = MACHINE_X16; }
        else if (strcmp(processor, "65ce02")     == 0) s->cpu_type = CPU_65CE02;
        else if (strcmp(processor, "45gs02")     == 0) { s->cpu_type = CPU_45GS02; s->machine_type = MACHINE_MEGA65; }
    }
    s->cpu = CPUFactory::create(s->cpu_type);
    s->cpu->mem = s->mem;
    machine_init_hardware(s);
    update_opcode_cache(s);
    symbol_table_init(s->symbols, "Session");
    breakpoint_init(s->breakpoints);
    s->state = SIM_IDLE;
    s->start_addr = 0x0200;
    s->debug_ctx = new DebugContext();
    return s;
}

void sim_destroy(sim_session_t *s) {
    if (!s) return;
    mem_free_far_pages(s->mem);
    delete s->debug_ctx;
    for (auto h : s->dynamic_handlers) delete h;
    if (s->mem->io_registry) delete s->mem->io_registry;
    delete s->cpu;
    delete s;
}

/* --------------------------------------------------------------------------
 * Binary load/save
 * -------------------------------------------------------------------------- */

/* Shared setup for binary loads: reset session state, init CPU. */
static void binary_load_common(sim_session_t *s, uint16_t load_addr, int byte_count)
{
    s->cpu->reset();
    s->cpu->pc    = load_addr;
    s->start_addr = load_addr;
    s->load_size  = (unsigned short)(byte_count > 0xFFFF ? 0xFFFF : (byte_count < 0 ? 0 : byte_count));
    if (s->cpu_type == CPU_45GS02) s->cpu->set_flag( FLAG_E, 1);
    s->state = SIM_READY;
}

int sim_load_asm(sim_session_t *s, const char *path) {
    if (!s || !path) return -1;
    if (g_verbose >= 2) fprintf(stderr, "[DEBUG] sim_load_asm: %s (g_verbose=%d)\n", path, g_verbose);
    
    char base[512];
    strncpy(base, path, sizeof(base)-1);
    base[sizeof(base)-1] = 0;
    char *dot = strrchr(base, '.');
    if (dot && (strcasecmp(dot, ".asm") == 0 || strcasecmp(dot, ".s") == 0)) *dot = 0;
    
    /* Detect processor type from .cpu / //.cpu directive before hardware init. */
    cpu_type_t detected_type = s->cpu_type;
    {
        char asm_check[512];
        snprintf(asm_check, sizeof(asm_check), "%s.asm", base);
        if (g_verbose >= 2) fprintf(stderr, "DEBUG: sim_load_asm checking for asm: %s\n", asm_check);
        
        FILE *f = fopen(asm_check, "r");
        if (f) {
            fclose(f);
            detected_type = detect_asm_cpu_type(asm_check);
            if (g_verbose >= 2) fprintf(stderr, "DEBUG: sim_load_asm detected type: %d\n", (int)detected_type);
        }
    }

    /* Reset session state */
    IORegistry *old_registry = s->mem->io_registry;
    mem_free_far_pages(s->mem);
    memset(s->mem, 0, sizeof(memory_t));
    s->mem->io_registry = old_registry;
    
    /* Apply detected or preserved CPU type BEFORE machine_init_hardware */
    if (detected_type != s->cpu_type) {
        apply_cpu_type(s, detected_type);
    }
    s->cpu->mem = s->mem;

    /* Derive and apply machine type */
    machine_type_t derived_machine = default_machine_for_cpu(s->cpu_type);
    if (derived_machine != s->machine_type) {
        s->machine_type = derived_machine;
    }

    machine_init_hardware(s);
    symbol_table_init(s->symbols, "Toolchain");
    source_map_init(s->source_map);
    breakpoint_init(s->breakpoints);
    s->debug_ctx->clear_history();
    s->last_error[0] = '\0';
    
    int bundle_load_addr = 0x0801;
    if (load_toolchain_bundle(s->mem, s->symbols, s->source_map, base, &bundle_load_addr, s->last_error, sizeof(s->last_error))) {
        strncpy(s->filename, path, sizeof(s->filename) - 1);
        /* Apply cpu/machine type from SIM_CPU/SIM_MACHINE markers in the metadata pipeline.
         * This overrides the early detect_asm_cpu_type() scan when the metadata is definitive. */
        apply_session_processor_symbols(s);

        /* FINAL OVERRIDE: If detect_asm_cpu_type found an explicit pseudo-op that is 
         * more specific than what was in the symbols, re-apply it. */
        if (detected_type != CPU_6502 && detected_type != s->cpu_type) {
            if (g_verbose >= 2) fprintf(stderr, "DEBUG: sim_load_asm re-applying detected type: %d\n", (int)detected_type);
            apply_cpu_type(s, detected_type);
        }

        /* Try to find a reasonable start address; fall back to PRG load address */
        unsigned short start_addr = (unsigned short)bundle_load_addr;
        symbol_lookup_name(s->symbols, "main", &start_addr);
        if (start_addr == (unsigned short)bundle_load_addr) symbol_lookup_name(s->symbols, "start", &start_addr);

        binary_load_common(s, start_addr, 0);
        return 0;
    }
    
    if (s->last_error[0] == '\0') {
        snprintf(s->last_error, sizeof(s->last_error), "Failed to load bundle for %s. (Is KickAssembler missing?)", base);
    }
    
    return -1;
}

int sim_load_bin(sim_session_t *s, const char *path, uint16_t load_addr)
{
    if (!s || !path) return -1;
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    IORegistry *old_registry = s->mem->io_registry;
    mem_free_far_pages(s->mem);
    memset(s->mem, 0, sizeof(memory_t));
    s->mem->io_registry = old_registry;
    machine_init_hardware(s);
    symbol_table_init(s->symbols, "Session");
    breakpoint_init(s->breakpoints);
    s->debug_ctx->clear_history();
    int n = 0, c;
    while ((c = fgetc(f)) != EOF) {
        uint32_t dst = (uint32_t)load_addr + (uint32_t)n;
        if (dst < 0x10000) s->mem->mem[dst] = (uint8_t)c;
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
    IORegistry *old_registry = s->mem->io_registry;
    mem_free_far_pages(s->mem);
    memset(s->mem, 0, sizeof(memory_t));
    s->mem->io_registry = old_registry;
    machine_init_hardware(s);
    symbol_table_init(s->symbols, "Session");
    breakpoint_init(s->breakpoints);
    s->debug_ctx->clear_history();
    int n = 0, c;
    while ((c = fgetc(f)) != EOF) {
        uint32_t dst = (uint32_t)load_addr + (uint32_t)n;
        if (dst < 0x10000) s->mem->mem[dst] = (uint8_t)c;
        n++;
    }
    fclose(f);
    binary_load_common(s, load_addr, n);
    strncpy(s->filename, path, sizeof(s->filename) - 1);

    /* Load companion annotation files (.list, .sym, .stdout) if present */
    char base[512];
    strncpy(base, path, sizeof(base) - 1);
    base[sizeof(base) - 1] = 0;
    char *dot = strrchr(base, '.');
    if (dot) *dot = 0;
    load_companion_files(s->symbols, s->source_map, base);
    /* Apply cpu/machine type from SIM_CPU/SIM_MACHINE markers in companion files */
    apply_session_processor_symbols(s);

    return 0;
}

int sim_save_bin(sim_session_t *s, const char *path,
                 uint16_t addr_start, uint16_t count)
{
    if (!s || !path || count == 0) return -1;
    FILE *f = fopen(path, "wb");
    if (!f) return -1;
    for (uint32_t i = 0; i < (uint32_t)count; i++) {
        if (fputc(s->mem->mem[(uint32_t)addr_start + i], f) == EOF) {
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
        if (fputc(s->mem->mem[(uint32_t)addr_start + i], f) == EOF) {
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
    int skip_bp = (s->state == SIM_PAUSED || s->state == SIM_READY) ? 1 : 0;
    for (int i = 0; i < count; i++) {
        s->mem->mem_writes = 0;
        int tr = handle_trap(s->symbols, s->cpu, s->mem);
        if (tr < 0) { s->state = SIM_FINISHED; if (s->event_cb) s->event_cb(s, SIM_EVENT_BRK, s->event_userdata); return SIM_EVENT_BRK; }
        if (tr > 0) continue;
        if (!skip_bp && breakpoint_hit(s->breakpoints, s->cpu)) { s->state = SIM_PAUSED; if (s->event_cb) s->event_cb(s, SIM_EVENT_BREAK, s->event_userdata); return SIM_EVENT_BREAK; }
        skip_bp = 0;
        unsigned char opc = mem_read(s->mem, s->cpu->pc);
        if (opc == 0x60 && (uint8_t)s->cpu->s == 0xFF) {
            s->state = SIM_FINISHED;
            if (s->event_cb) s->event_cb(s, SIM_EVENT_BRK, s->event_userdata);
            return SIM_EVENT_BRK;
        }

        uint16_t pre_pc = s->cpu->pc;
        uint64_t pre_cycles = s->cpu->cycles;
        CPUState pre_state = *static_cast<CPUState*>(s->cpu);

        s->cpu->step();
        s->debug_ctx->on_after_execute(pre_pc, pre_state,
                                        *static_cast<CPUState*>(s->cpu),
                                        s->cpu->cycles - pre_cycles, s->mem);
    }
    s->state = SIM_PAUSED;
    return 0;
}

int sim_step_over(sim_session_t *s) {
    if (!s || s->state == SIM_IDLE || s->state == SIM_FINISHED) return -1;
    
    unsigned char opc = mem_read(s->mem, s->cpu->pc);
    const dispatch_entry_t *te = peek_dispatch(s->cpu, s->mem, s->cpu->dispatch_table(), s->cpu_type);
    
    bool is_jsr = (opc == 0x20); // Standard JSR abs
    if (!is_jsr && te && te->mnemonic) {
        if (strcmp(te->mnemonic, "JSR") == 0 || strcmp(te->mnemonic, "BSR") == 0) {
            is_jsr = true;
        }
    }
    
    if (is_jsr && te) {
        char dummy[128];
        int instr_bytes = disasm_one(s->mem, s->cpu->dispatch_table(), s->cpu_type, s->cpu->pc, dummy, sizeof(dummy));
        uint16_t next_pc = (uint16_t)(s->cpu->pc + instr_bytes);
        bool added_bp = !sim_has_breakpoint(s, next_pc);
        if (added_bp) sim_break_set(s, next_pc, NULL);

        while (1) {
            int ev = sim_step(s, 1);
            if (ev != 0) {
                if (added_bp) sim_break_clear(s, next_pc);
                return ev;
            }
            if (s->cpu->pc == next_pc) {
                if (added_bp) sim_break_clear(s, next_pc);
                return 0;
            }
        }
    } else {
        return sim_step(s, 1);
    }
}

int sim_step_out(sim_session_t *s) {
    if (!s || s->state == SIM_IDLE || s->state == SIM_FINISHED) return -1;
    
    uint16_t sp = s->cpu->s;
    uint8_t lo = mem_read(s->mem, (uint16_t)(0x0100 + ((sp + 1) & 0xFF)));
    uint8_t hi = mem_read(s->mem, (uint16_t)(0x0100 + ((sp + 2) & 0xFF)));
    uint16_t return_addr = (uint16_t)((hi << 8) | lo) + 1;
    
    if (return_addr == 1) return -1; // Empty stack
    
    while (1) {
        int ev = sim_step(s, 1);
        if (ev != 0) return ev;
        if (s->cpu->pc == return_addr) return 0;
    }
}

int sim_step_cycles(sim_session_t *s, unsigned long max_cycles) {
    if (!s || s->state == SIM_IDLE || s->state == SIM_FINISHED) return -1;
    unsigned long start = s->cpu->cycles;
    int skip_bp = (s->state == SIM_PAUSED || s->state == SIM_READY) ? 1 : 0;
    while (s->cpu->cycles - start < max_cycles) {
        s->mem->mem_writes = 0;
        int tr = handle_trap(s->symbols, s->cpu, s->mem);
        if (tr < 0) { s->state = SIM_FINISHED; if (s->event_cb) s->event_cb(s, SIM_EVENT_BRK, s->event_userdata); return SIM_EVENT_BRK; }
        if (tr > 0) continue;
        if (!skip_bp && breakpoint_hit(s->breakpoints, s->cpu)) { s->state = SIM_PAUSED; if (s->event_cb) s->event_cb(s, SIM_EVENT_BREAK, s->event_userdata); return SIM_EVENT_BREAK; }
        skip_bp = 0;
        unsigned char opc = mem_read(s->mem, s->cpu->pc);
        if (opc == 0x60 && (uint8_t)s->cpu->s == 0xFF) {
            s->state = SIM_FINISHED;
            if (s->event_cb) s->event_cb(s, SIM_EVENT_BRK, s->event_userdata);
            return SIM_EVENT_BRK;
        }
        uint16_t pre_pc = s->cpu->pc;
        uint64_t pre_cycles = s->cpu->cycles;
        CPUState pre_state = *static_cast<CPUState*>(s->cpu);
        s->cpu->step();
        s->debug_ctx->on_after_execute(pre_pc, pre_state,
                                        *static_cast<CPUState*>(s->cpu),
                                        s->cpu->cycles - pre_cycles, s->mem);
    }
    s->state = SIM_PAUSED;
    return 0;
}

void sim_reset(sim_session_t *s) {
    if (!s) return;
    s->cpu->reset(); s->cpu->pc = s->start_addr;
    s->cpu->cycles = 0;
    if (s->cpu_type == CPU_45GS02) s->cpu->set_flag( FLAG_E, 1);
    s->state = (s->filename[0] != '\0') ? SIM_READY : SIM_IDLE;
}

void sim_clear_cycles(sim_session_t *s) {
    if (s && s->cpu) {
        s->cpu->cycles = 0;
    }
}

int sim_disassemble_entry(sim_session_t *s, uint16_t addr, sim_disasm_entry_t *out) {
    if (!s || !out) return -1;

    disasm_entry_t internal_entry;
    int res = disasm_one_entry(s->mem, s->cpu->dispatch_table(), s->cpu_type, addr, &internal_entry);

    out->address = internal_entry.address;
    out->size = internal_entry.size;
    strncpy(out->bytes, internal_entry.bytes, sizeof(out->bytes));
    strncpy(out->mnemonic, internal_entry.mnemonic, sizeof(out->mnemonic));
    strncpy(out->operand, internal_entry.operand, sizeof(out->operand));
    out->cycles = internal_entry.cycles;
    out->target_addr = internal_entry.target_addr;
    out->has_target = internal_entry.has_target;

    return (res > 0) ? 0 : -1;
}

int sim_disassemble_one(sim_session_t *s, uint16_t addr, char *buf, size_t len) {
    if (!s) return 0;
    return disasm_one(s->mem, s->cpu->dispatch_table(), s->cpu_type, addr, buf, (int)len);
}

cpu_t          *sim_get_cpu(sim_session_t *s)    { return s ? s->cpu : NULL; }
const memory_t *sim_get_memory(sim_session_t *s) { return s ? s->mem : NULL; }
uint8_t sim_mem_read_byte(sim_session_t *s, uint16_t addr) { return s ? mem_read(s->mem, addr) : 0; }
void sim_mem_write_byte(sim_session_t *s, uint16_t addr, uint8_t val) { if (s) mem_write(s->mem, addr, val); }
const char *sim_get_version(void) {
    return SIM_VERSION;
}

sim_state_t sim_get_state(sim_session_t *s) { return s ? s->state : SIM_IDLE; }
void sim_set_state(sim_session_t *s, sim_state_t state) { if (s) s->state = state; }
const char *sim_get_filename(sim_session_t *s) { return (s && s->filename[0]) ? s->filename : "(none)"; }
const char *sim_get_last_error(sim_session_t *s) { return (s && s->last_error[0]) ? s->last_error : NULL; }
const char *sim_processor_name(sim_session_t *s) { return s ? processor_name_local(s->cpu_type) : "6502"; }

machine_type_t sim_get_machine_type(sim_session_t *s) { return s ? s->machine_type : MACHINE_RAW6502; }
const char *sim_machine_name(machine_type_t type) { return machine_name_local(type); }

int sim_device_add(sim_session_t *s, const char *name, uint16_t address) {
    if (!s || !name || !s->mem->io_registry) return -1;

    IOHandler *h = nullptr;
    uint16_t end = address;

    if (strcmp(name, "vic2") == 0) {
        h = new VIC2Handler();
        end = address + 0x2E;
    } else if (strcmp(name, "mega65_math") == 0) {
        h = new MathCoprocessorHandler();
        end = address + 0x03;
    } else if (strcmp(name, "mega65_dma") == 0) {
        h = new DMAControllerHandler();
        end = address + 0x05;
    } else if (strcmp(name, "sid") == 0) {
        /* SIDHandler implementation is in sid.plan.md, not yet in code.
         * For now, we will return -1 or stub it. 
         * Once SIDHandler is added to the project, this can be uncommented.
         */
        // h = new SIDHandler();
        // end = address + 0x1F;
        return -1;
    }

    if (h) {
        s->dynamic_handlers.push_back(h);
        s->mem->io_registry->register_handler(address, end, h);
        s->mem->io_registry->rebuild_map(s->mem);
        return 0;
    }
    return -1;
}

int sim_get_device_count(sim_session_t *s) {
    if (!s || !s->mem->io_registry) return 0;
    return (int)s->mem->io_registry->get_registrations().size();
}

int sim_get_device_info(sim_session_t *s, int idx, char *name_out, int name_sz, uint16_t *start_out, uint16_t *end_out) {
    if (!s || !s->mem->io_registry) return -1;
    const auto& regs = s->mem->io_registry->get_registrations();
    if (idx < 0 || idx >= (int)regs.size()) return -1;
    
    if (name_out) strncpy(name_out, regs[idx].handler->get_handler_name(), (size_t)name_sz);
    if (start_out) *start_out = regs[idx].start;
    if (end_out) *end_out = regs[idx].end;
    return 0;
}

int sim_snippet_count(void) { return g_snippet_count; }

int sim_snippet_get(int idx, sim_snippet_t *out) {
    if (idx < 0 || idx >= g_snippet_count || !out) return -1;
    out->name = g_snippets[idx].name;
    out->category = g_snippets[idx].category;
    out->summary = g_snippets[idx].summary;
    out->processor = g_snippets[idx].processor;
    out->requires_device = g_snippets[idx].requires_device;
    out->body = g_snippets[idx].body;
    // fprintf(stderr, "sim_snippet_get: idx=%d name=%s body_len=%zu\n", idx, out->name, out->body ? strlen(out->body) : 0);
    return 0;
}

int sim_snippet_find(const char *name, sim_snippet_t *out) {
    const snippet_t *s = snippet_find(name);
    if (!s || !out) return -1;
    out->name = s->name;
    out->category = s->category;
    out->summary = s->summary;
    out->processor = s->processor;
    out->requires_device = s->requires_device;
    out->body = s->body;
    return 0;
}


void sim_set_machine_type(sim_session_t *s, machine_type_t machine) {
    if (!s) return;
    s->machine_type = machine;
    // Hardware purge: Dynamic handlers are tied to the machine context
    for (auto h : s->dynamic_handlers) delete h;
    s->dynamic_handlers.clear();

    cpu_type_t new_cpu = s->cpu_type;
    switch (machine) {
        case MACHINE_C64:      new_cpu = CPU_6502; break;
        case MACHINE_C128:     new_cpu = CPU_6502; break; // Actually 8502 but we use 6502
        case MACHINE_MEGA65:   new_cpu = CPU_45GS02; break;
        case MACHINE_X16:      new_cpu = CPU_65C02; break;
        case MACHINE_RAW6502:  new_cpu = CPU_6502; break;
    }
    apply_cpu_type(s, new_cpu);
    machine_init_hardware(s);
}

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
    cpu_type_t new_type = s->cpu_type;
    if      (strcmp(name, "6502") == 0)       new_type = CPU_6502;
    else if (strcmp(name, "6502-undoc") == 0) new_type = CPU_6502_UNDOCUMENTED;
    else if (strcmp(name, "65c02") == 0)      new_type = CPU_65C02;
    else if (strcmp(name, "65ce02") == 0)     new_type = CPU_65CE02;
    else if (strcmp(name, "45gs02") == 0)     new_type = CPU_45GS02;
    apply_cpu_type(s, new_type);
}
cpu_type_t sim_get_cpu_type(sim_session_t *s) { return s ? s->cpu_type : CPU_6502; }
void sim_set_debug(sim_session_t *s, bool debug) { if (s && s->cpu) s->cpu->debug = debug; }
void sim_set_event_callback(sim_session_t *s, sim_event_cb cb, void *userdata) { if (s) { s->event_cb = cb; s->event_userdata = userdata; } }
int sim_break_set(sim_session_t *s, uint16_t addr, const char *cond) { return s ? breakpoint_add(s->breakpoints, addr, cond) : 0; }
void sim_break_clear(sim_session_t *s, uint16_t addr) { if (s) breakpoint_remove(s->breakpoints, addr); }
const char *sim_sym_by_addr(sim_session_t *s, uint16_t addr) { return s ? symbol_lookup_addr_name(s->symbols, addr) : NULL; }
int sim_break_is_enabled(sim_session_t *s, int idx) { if (!s || idx < 0 || idx >= s->breakpoints->count) return 0; return s->breakpoints->breakpoints[idx].enabled; }
int sim_break_toggle(sim_session_t *s, int idx) { if (!s || idx < 0 || idx >= s->breakpoints->count) return -1; s->breakpoints->breakpoints[idx].enabled = !s->breakpoints->breakpoints[idx].enabled; return s->breakpoints->breakpoints[idx].enabled; }
void sim_trace_enable(sim_session_t *s, int enable) { if (s) s->debug_ctx->enable_trace(enable); }
int sim_trace_is_enabled(sim_session_t *s) { return s ? s->debug_ctx->trace_is_enabled() : 0; }
void sim_trace_clear(sim_session_t *s) { if (s) s->debug_ctx->clear_trace(); }
int sim_trace_count(sim_session_t *s) { return s ? s->debug_ctx->trace_count() : 0; }
uint64_t sim_trace_total_count(sim_session_t *s) { return s ? s->debug_ctx->trace_total() : 0; }
int sim_trace_get(sim_session_t *s, int slot, sim_trace_entry_t *entry) {
    return s ? s->debug_ctx->get_trace(slot, entry) : 0;
}
int sim_has_breakpoint(sim_session_t *s, uint16_t addr) { if (!s) return 0; for (int i = 0; i < s->breakpoints->count; i++) if (s->breakpoints->breakpoints[i].address == addr) return 1; return 0; }
int sim_get_last_writes(sim_session_t *s, uint16_t *addrs, int max_count) {
    if (!s || !addrs || max_count <= 0) return 0;
    int n = s->mem->mem_writes < 256 ? s->mem->mem_writes : 256;
    if (n > max_count) n = max_count;
    for (int i = 0; i < n; i++) addrs[i] = s->mem->mem_addr[i];
    return n;
}
void sim_set_log_callback(sim_session_t *s, sim_log_cb cb, void *userdata) {
    if (s) {
        s->log_cb = cb;
        s->log_userdata = userdata;
    }
}

void sim_exec_command(sim_session_t *s, const char *cmd) {
    if (!s || !cmd) return;

    // Set up the CLI logger to use our session callback
    cli_set_log_callback((cli_log_cb)s->log_cb, s->log_userdata);

    cli_process_command(cmd, s->cpu, s->mem, &s->cpu_type, s->breakpoints, s->symbols);

    // Clear the logger to avoid accidental use
    cli_set_log_callback(nullptr, nullptr);
}

std::vector<std::string> sim_get_completions(const char *prefix) {
    return cli_get_completions(prefix ? prefix : "");
}

std::vector<std::string> sim_get_symbol_completions(sim_session_t *s, const char *prefix) {
    if (!s) return {};
    const size_t plen = strlen(prefix ? prefix : "");
    const char *pfx = prefix ? prefix : "";
    std::vector<std::string> out;
    for (int i = 0; i < s->symbols->count; i++) {
        const symbol_t &sym = s->symbols->symbols[i];
        // Only offer user-facing symbol types; skip internal metadata.
        if (sym.type != SYM_LABEL && sym.type != SYM_CONSTANT &&
            sym.type != SYM_TRAP  && sym.type != SYM_PROCESSOR) continue;
        if (strncasecmp(sym.name, pfx, plen) == 0)
            out.push_back(sym.name);
    }
    std::sort(out.begin(), out.end());
    return out;
}

void sim_set_pc(sim_session_t *s, uint16_t pc) { if (s) s->cpu->pc = pc; }
void sim_set_reg_byte(sim_session_t *s, const char *name, uint8_t val) {
    /* (uint16_t)val zero-extends correctly, which is safe and deliberate even 
     * for 16-bit registers (like S) that might be set via this byte interface. */
    sim_set_reg_value(s, name, (uint16_t)val);
}

void sim_set_reg_value(sim_session_t *s, const char *name, uint16_t val) {
    if (!s || !name) return;
    if (strcmp(name, "A") == 0) s->cpu->a = (uint8_t)val;
    else if (strcmp(name, "X") == 0) s->cpu->x = (uint8_t)val;
    else if (strcmp(name, "Y") == 0) s->cpu->y = (uint8_t)val;
    else if (strcmp(name, "Z") == 0) s->cpu->z = (uint8_t)val;
    else if (strcmp(name, "B") == 0) s->cpu->b = (uint8_t)val;
    else if (strcmp(name, "S") == 0) s->cpu->s = val;
    else if (strcmp(name, "P") == 0) s->cpu->p = (uint8_t)val;
}
int sim_break_count(sim_session_t *s) { return s ? s->breakpoints->count : 0; }
int sim_break_get(sim_session_t *s, int idx, uint16_t *addr, char *cond, int cond_sz) {
    if (!s || idx < 0 || idx >= s->breakpoints->count) return 0;
    if (addr) *addr = s->breakpoints->breakpoints[idx].address;
    if (cond && s->breakpoints->breakpoints[idx].condition[0]) strncpy(cond, s->breakpoints->breakpoints[idx].condition, cond_sz);
    return 1;
}
int sim_opcode_count(sim_session_t *s) {
    return s ? (int)s->opcodes_cache.size() : 0;
}
int sim_opcode_get(sim_session_t *s, int idx, sim_opcode_info_t *info) {
    if (!s || idx < 0 || idx >= (int)s->opcodes_cache.size() || !info) return -1;
    *info = s->opcodes_cache[idx];
    return 0;
}
int sim_sym_count(sim_session_t *s) { return s ? s->symbols->count : 0; }
int sim_sym_get_idx(sim_session_t *s, int idx, uint16_t *addr, char *name_buf, int name_sz, int *type_out, char *comment_buf, int comment_sz) {
    if (!s || idx < 0 || idx >= s->symbols->count) return 0;
    if (addr) *addr = s->symbols->symbols[idx].address;
    if (name_buf) strncpy(name_buf, s->symbols->symbols[idx].name, name_sz);
    if (type_out) *type_out = s->symbols->symbols[idx].type;
    if (comment_buf) strncpy(comment_buf, s->symbols->symbols[idx].comment, comment_sz);
    return 1;
}
const char *sim_sym_type_name(int type) {
    switch (type) {
    case SYM_LABEL: return "LABEL";
    case SYM_VARIABLE: return "VAR";
    case SYM_CONSTANT: return "CONST";
    case SYM_FUNCTION: return "FUNC";
    case SYM_IO_PORT: return "IO";
    case SYM_MEMORY_REGION: return "REGION";
    case SYM_TRAP: return "TRAP";
    case SYM_INSPECT: return "INSPECT";
    case SYM_PROCESSOR: return "PROC";
    default: return "UNKNOWN";
    }
}
int sim_sym_remove_idx(sim_session_t *s, int idx) {
    if (!s) return 0;
    return symbol_remove_idx(s->symbols, idx);
}
int sim_sym_rename(sim_session_t *s, int idx, const char *new_name) {
    if (!s) return 0;
    return symbol_rename(s->symbols, idx, new_name);
}
int sim_sym_set_addr(sim_session_t *s, int idx, uint16_t addr) {
    if (!s) return 0;
    return symbol_set_addr(s->symbols, idx, addr);
}
int sim_sym_add(sim_session_t *s, uint16_t addr, const char *name, const char *type_str) {
    if (!s || !name || !type_str) return 0;
    symbol_type_t type = SYM_LABEL;
    if (strcmp(type_str, "TRAP") == 0) type = SYM_TRAP;
    return symbol_add(s->symbols, name, addr, type, "API");
}
int sim_sym_load_file(sim_session_t *s, const char *path) { return s ? symbol_load_file(s->symbols, path) : 0; }
int sim_sym_save_file(sim_session_t *s, const char *path) { return s ? symbol_save_file(s->symbols, path) : 0; }

int sim_source_lookup_addr(sim_session_t *s, uint16_t addr, char *path_out, int *line_out) {
    if (!s) return 0;
    return source_map_lookup_addr(s->source_map, addr, path_out, line_out) ? 1 : 0;
}

int sim_source_lookup_line(sim_session_t *s, const char *path, int line, uint16_t *addr_out) {
    if (!s) return 0;
    uint32_t a32;
    if (source_map_lookup_line(s->source_map, path, line, &a32)) {
        if (addr_out) *addr_out = (uint16_t)a32;
        return 1;
    }
    return 0;
}

void sim_vic_render_framebuffer(sim_session_t *s, uint8_t *buf) {
    if (s && buf) vic2_render_rgb(s->mem, buf);
}

void sim_vic_render_active_framebuffer(sim_session_t *s, uint8_t *buf) {
    if (s && buf) vic2_render_rgb_active(s->mem, buf);
}

void sim_vic_render_sprite(sim_session_t *s, int index, uint8_t *buf) {
    if (s && buf) vic2_render_sprite(s->mem, index, buf);
}

void sim_vic_render_char(sim_session_t *s, uint16_t char_base, int char_index, int mcm, uint8_t c0, uint8_t c1, uint8_t c2, uint8_t c3, uint8_t *buf) {
    if (s && buf) vic2_render_char(s->mem, char_base, char_index, mcm, c0, c1, c2, c3, buf);
}
/* --- Execution History --- */

void sim_history_enable(sim_session_t *s, int enable) { if (s) s->debug_ctx->enable_history(enable); }
int  sim_history_is_enabled(sim_session_t *s)         { return s ? s->debug_ctx->history_is_enabled() : 0; }
void sim_history_clear(sim_session_t *s)               { if (s) s->debug_ctx->clear_history(); }
int  sim_history_depth(sim_session_t *s)               { return s ? s->debug_ctx->history_depth()    : 0; }
int  sim_history_count(sim_session_t *s)               { return s ? s->debug_ctx->history_count()    : 0; }
int  sim_history_position(sim_session_t *s)            { return s ? s->debug_ctx->history_pos()      : 0; }

int sim_history_step_back(sim_session_t *s) {
    if (!s) return 0;
    int r = s->debug_ctx->step_back(s->cpu, s->mem);
    if (r) s->state = SIM_PAUSED;
    return r;
}

int sim_history_step_fwd(sim_session_t *s) {
    if (!s) return 0;
    int r = s->debug_ctx->step_fwd(s->cpu, s->mem);
    if (r) s->state = SIM_PAUSED;
    return r;
}

int sim_history_get(sim_session_t *s, int slot, sim_history_entry_t *entry) {
    return s ? s->debug_ctx->get_history(slot, entry) : 0;
}

/* --- Profiler --- */

void sim_profiler_enable(sim_session_t *s, int enable) { if (s) s->debug_ctx->enable_profiler(enable); }
int sim_profiler_is_enabled(sim_session_t *s) { return s ? s->debug_ctx->profiler_is_enabled() : 0; }
void sim_profiler_clear(sim_session_t *s) { if (s) s->debug_ctx->clear_profiler(); }
uint32_t sim_profiler_get_exec(sim_session_t *s, uint16_t addr) { return s ? s->debug_ctx->profiler_exec(addr) : 0; }
uint32_t sim_profiler_get_cycles(sim_session_t *s, uint16_t addr) { return s ? s->debug_ctx->profiler_cycles(addr) : 0; }
const char *sim_mode_name(unsigned char mode) { return mode_name(mode); }

/* --- Memory Snapshot & Diff --- */

void sim_snapshot_take(sim_session_t *s) {
    if (s && s->cpu && s->debug_ctx) {
        auto now = std::chrono::steady_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
        s->debug_ctx->take_snapshot(s->cpu->cycles, (uint32_t)(ms & 0xFFFFFFFF));
    }
}
void sim_snapshot_clear(sim_session_t *s) {
    if (s && s->debug_ctx) s->debug_ctx->clear_snapshot();
}
int  sim_snapshot_valid(sim_session_t *s) { return (s && s->debug_ctx) ? s->debug_ctx->snapshot_is_valid() : 0; }
int  sim_snapshot_diff(sim_session_t *s, sim_diff_entry_t *entries, int entries_cap) {
    return (s && s->debug_ctx) ? s->debug_ctx->snapshot_diff(entries, entries_cap) : -1;
}
uint64_t sim_snapshot_cycles(sim_session_t *s) { return (s && s->debug_ctx) ? s->debug_ctx->snapshot_cycles() : 0; }
uint32_t sim_snapshot_timestamp(sim_session_t *s) { return (s && s->debug_ctx) ? s->debug_ctx->snapshot_timestamp() : 0; }

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
    if (start_addr >= 0) s->cpu->pc = (uint16_t)start_addr;
    if (max_instr > entries_cap) max_instr = entries_cap;

    const char *reason = "count";
    int n = 0;

    while (n < max_instr) {
        s->mem->mem_writes = 0;
        int tr = handle_trap(s->symbols, s->cpu, s->mem);
        if (tr < 0) { reason = "trap"; break; }
        if (tr > 0) continue;

        if (breakpoint_hit(s->breakpoints, s->cpu)) { reason = "bp"; break; }

        uint16_t pre_pc = s->cpu->pc;
        unsigned char opc = mem_read(s->mem, pre_pc);

        /* Disassemble before executing */
        // disasm_one(s->mem, &s->dt, s->cpu_type, pre_pc, entries[n].disasm,
                //    (int)sizeof(entries[n].disasm));
        entries[n].pc = pre_pc;

        if (opc == 0x60 && (uint8_t)s->cpu->s == 0xFF) {
            entries[n].cpu = *static_cast<CPUState*>(s->cpu);
            entries[n].cycles_delta = 0;
            n++;
            reason = "brk";
            s->state = SIM_FINISHED;
            break;  /* stop at top-level RTS (program returned to simulator) */
        }

        uint64_t pre_cycles = s->cpu->cycles;
        CPUState pre_state = *static_cast<CPUState*>(s->cpu);
        s->cpu->step();
        entries[n].cpu = *static_cast<CPUState*>(s->cpu);
        entries[n].cycles_delta = (int)(s->cpu->cycles - pre_cycles);
        s->debug_ctx->on_after_execute(pre_pc, pre_state,
                                        *static_cast<CPUState*>(s->cpu),
                                        s->cpu->cycles - pre_cycles, s->mem);
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
    for (int i = 0; i < 4; i++) saved[i] = s->mem->mem[(scratch_addr + i) & 0xFFFF];

    /* Write: JSR routine_addr (3 bytes) + RTS (1 byte) */
    s->mem->mem[(scratch_addr    ) & 0xFFFF] = 0x20;
    s->mem->mem[(scratch_addr + 1) & 0xFFFF] = (uint8_t)(routine_addr & 0xFF);
    s->mem->mem[(scratch_addr + 2) & 0xFFFF] = (uint8_t)(routine_addr >> 8);
    s->mem->mem[(scratch_addr + 3) & 0xFFFF] = 0x60;

    CPUState saved_cpu_state = *static_cast<CPUState*>(s->cpu);
    sim_state_t saved_state = s->state;
    int         total_pass  = 0;

    for (int t = 0; t < count; t++) {
        const sim_test_in_t     *in  = &inputs[t];
        const sim_test_expect_t *exp = &expects[t];
        sim_test_result_t       *res = &results[t];

        /* Reset registers to neutral state; keep memory */
        s->cpu->reset();
        if (s->cpu_type == CPU_45GS02) s->cpu->set_flag( FLAG_E, 1);
        s->cpu->pc = scratch_addr;

        /* Apply input register overrides */
        if (in->a >= 0) s->cpu->a = (uint8_t)in->a;
        if (in->x >= 0) s->cpu->x = (uint8_t)in->x;
        if (in->y >= 0) s->cpu->y = (uint8_t)in->y;
        if (in->z >= 0) s->cpu->z = (uint8_t)in->z;
        if (in->b >= 0) s->cpu->b = (uint8_t)in->b;
        if (in->s >= 0) s->cpu->s = (uint8_t)in->s;
        if (in->p >= 0) s->cpu->p = (uint8_t)in->p;

        /* Apply input memory writes */
        for (int m = 0; m < in->mem_count && m < SIM_VALIDATE_MEM_OPS; m++)
            s->mem->mem[in->mem_addr[m]] = in->mem_val[m];

        /* Execute until BRK or step limit */
        s->state = SIM_READY;
        int ev = sim_step(s, max_steps);

        /* Capture actual register values */
        res->a = s->cpu->a;  res->x = s->cpu->x;
        res->y = s->cpu->y;  res->z = s->cpu->z;
        res->b = s->cpu->b;  res->s = s->cpu->s;  res->p = s->cpu->p;
        res->fail_msg[0] = '\0';
        res->passed = 1;

        if (ev != SIM_EVENT_BRK && ev != SIM_EVENT_STP) {
            /* Did not complete normally */
            res->passed = 0;
            const char *why = (ev == 0) ? "timeout" :
                              (ev == SIM_EVENT_BREAK) ? "breakpoint" : "trap";
            snprintf(res->fail_msg, sizeof(res->fail_msg),
                     "did not complete (%s at $%04X, %d steps)",
                     why, (unsigned)s->cpu->pc, max_steps);
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
                uint8_t got = s->mem->mem[exp->mem_addr[m]];
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
    for (int i = 0; i < 4; i++) s->mem->mem[(scratch_addr + i) & 0xFFFF] = saved[i];
    *static_cast<CPUState*>(s->cpu) = saved_cpu_state;
    s->state = saved_state;
    return total_pass;
}
