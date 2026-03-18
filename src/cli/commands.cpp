#include "sim_api.h"
#include "commands.h"
#include "version.h"
#include <algorithm>
#ifdef HAVE_READLINE
#  include <readline/readline.h>
#  include <readline/history.h>
#endif
#include "cpu_engine.h"
#include "opcodes/opcodes.h"
#include "condition.h"
#include "device/vic2.h"
#include "device/sid_io.h"
#include "patterns.h"
#include "metadata.h"
#include "commands/CommandRegistry.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <stdarg.h>
#include <sstream>
#include <string>
#include <vector>

/* Speed throttle: 0.0 = unlimited, 1.0 = C64 PAL (~985 kHz) */
static float  g_cli_speed   = 1.0f;
static const double CLI_C64_HZ = 985248.0;

int g_json_mode = 0;
void cli_set_json_mode(int mode) { g_json_mode = mode; }

static cli_log_cb s_cli_log = nullptr;
static void *s_cli_log_userdata = nullptr;

void cli_set_log_callback(cli_log_cb cb, void *userdata) {
    s_cli_log = cb;
    s_cli_log_userdata = userdata;
}

int cliIsInteractiveMode(void) {
    /* The GUI sets a log callback before dispatching each command and clears
     * it after.  Pure CLI (stdin/stdout) never sets one.  So the absence of
     * a log callback reliably means we are running in the text-only CLI —
     * i.e. interactive terminal mode rather than the GUI console pane. */
    return s_cli_log == nullptr;
}

void cli_printf(const char *fmt, ...) {
    char buf[2048];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    if (s_cli_log) {
        s_cli_log(buf, s_cli_log_userdata);
    } else {
        fprintf(stdout, "%s", buf);
    }
}

/* Forward declarations */
void json_inspect_result(const char *name, uint16_t pc, IOHandler *h, memory_t *mem, cpu_t *cpu);

/* --------------------------------------------------------------------------
 * History (simple ring buffer for CLI)
 * -------------------------------------------------------------------------- */
#define CLI_HIST_CAP 1024
typedef struct {
    CPUState pre_cpu;
    uint16_t pc;
    uint16_t delta_addr[16];
    uint8_t  delta_old[16];
    uint8_t  delta_count;
} cli_hist_entry_t;

static cli_hist_entry_t s_cli_hist[CLI_HIST_CAP];
static int s_cli_hist_write = 0;
static int s_cli_hist_count = 0;
static int s_cli_hist_pos   = 0;

void cli_hist_push(const CPUState *pre, const memory_t *mem) {
    if (s_cli_hist_pos > 0) {
        s_cli_hist_write = (s_cli_hist_write - s_cli_hist_pos + CLI_HIST_CAP) % CLI_HIST_CAP;
        s_cli_hist_count -= s_cli_hist_pos;
        if (s_cli_hist_count < 0) s_cli_hist_count = 0;
        s_cli_hist_pos = 0;
    }
    cli_hist_entry_t *e = &s_cli_hist[s_cli_hist_write];
    e->pre_cpu = *pre;
    e->pc = pre->pc;
    int dc = mem->mem_writes < 16 ? mem->mem_writes : 16;
    e->delta_count = (uint8_t)dc;
    for (int i = 0; i < dc; i++) {
        e->delta_addr[i] = mem->mem_addr[i];
        e->delta_old[i]  = mem->mem_old_val[i];
    }
    s_cli_hist_write = (s_cli_hist_write + 1) % CLI_HIST_CAP;
    if (s_cli_hist_count < CLI_HIST_CAP) s_cli_hist_count++;
}

int cli_hist_step_back(cpu_t *cpu, memory_t *mem) {
    if (s_cli_hist_pos >= s_cli_hist_count) return 0;
    int idx = ((s_cli_hist_write - 1 - s_cli_hist_pos) % CLI_HIST_CAP + CLI_HIST_CAP) % CLI_HIST_CAP;
    cli_hist_entry_t *e = &s_cli_hist[idx];
    *cpu = e->pre_cpu;
    for (int i = 0; i < e->delta_count; i++)
        mem->mem[e->delta_addr[i]] = e->delta_old[i];
    s_cli_hist_pos++;
    return 1;
}

int cli_hist_step_fwd(cpu_t *cpu, memory_t *mem, dispatch_table_t *dt, cpu_type_t cpu_type) {
    (void)dt; (void)cpu_type;
    if (s_cli_hist_pos == 0) return 0;
    int idx = ((s_cli_hist_write - s_cli_hist_pos) % CLI_HIST_CAP + CLI_HIST_CAP) % CLI_HIST_CAP;
    cli_hist_entry_t *e = &s_cli_hist[idx];
    *cpu = e->pre_cpu;
    mem->mem_writes = 0;
    static_cast<CPU*>(cpu)->step();
    s_cli_hist_pos--;
    return 1;
}

int handle_trap_local(const symbol_table_t *st, cpu_t *cpu, memory_t *mem) {
    if (!st) return 0;
    for (int i = 0; i < st->count; i++) {
        if (st->symbols[i].address != cpu->pc) continue;

        if (st->symbols[i].type == SYM_INSPECT) {
            IOHandler *h = mem->io_registry ? mem->io_registry->find_handler(st->symbols[i].name) : nullptr;
            if (g_json_mode) {
                json_inspect_result(st->symbols[i].name, cpu->pc, h, mem, cpu);
            } else {
                cli_printf("[INSPECT] %s at $%04X\n", st->symbols[i].name, cpu->pc);
                if (strcasecmp(st->symbols[i].name, "cpu") == 0) {
                    cli_printf("  REGS: A=%02X X=%02X Y=%02X Z=%02X B=%02X S=%04X P=%02X PC=%04X\n",
                           cpu->a, cpu->x, cpu->y, cpu->z, cpu->b, cpu->s, cpu->p, cpu->pc);
                } else if (h) {
                    cli_printf("  Device: %s\n", h->get_handler_name());
                    cli_printf("  Registers: ");
                    for (int r = 0; r < 32; r++) {
                        uint8_t val = 0;
                        if (h->io_read(mem, (uint16_t)r, &val)) {
                            cli_printf("%02X ", val);
                            if ((r+1)%8 == 0 && r < 31) cli_printf("\n             ");
                        }
                    }
                    cli_printf("\n");
                } else {
                    const char *nptr = st->symbols[i].name;
                    if (*nptr == '$') nptr++;
                    unsigned long addr = strtoul(nptr, NULL, 16);
                    if (addr == 0 && nptr[0] != '0') addr = cpu->pc;
                    cli_printf("  Memory at $%04lX: ", addr);
                    for (int r = 0; r < 16; r++) cli_printf("%02X ", mem_read(mem, (uint16_t)(addr + r)));
                    cli_printf("\n");
                }
            }
            continue;
        }

        if (st->symbols[i].type != SYM_TRAP) continue;
        
        if (g_json_mode) {
            /* We don't have a specific JSON trap result yet, just skip for now or use exec_result */
        } else {
            cli_printf("[TRAP] %-20s $%04X  A=%02X X=%02X Y=%02X",
                st->symbols[i].name, cpu->pc, cpu->a, cpu->x, cpu->y);
            if (cpu->pc > 0) cli_printf(" Z=%02X B=%02X", cpu->z, cpu->b);
            cli_printf(" S=%02X P=%02X", cpu->s, cpu->p);
            if (st->symbols[i].comment[0]) cli_printf("  ; %s", st->symbols[i].comment);
            cli_printf("\n");
        }
        cpu->cycles += 6;
        cpu->s++;
        unsigned short lo = mem_read(mem, 0x100 + cpu->s);
        cpu->s++;
        unsigned short hi = mem_read(mem, 0x100 + cpu->s);
        unsigned short ret = (unsigned short)(((unsigned short)hi << 8) | lo);
        ret++;
        if (ret == 0) return -1;
        cpu->pc = ret;
        return 1;
    }
    return 0;
}

/* --------------------------------------------------------------------------
 * JSON helpers
 * -------------------------------------------------------------------------- */

static void json_reg_fields(const cpu_t *cpu) {
    cli_printf("\"a\":%d,\"x\":%d,\"y\":%d,\"z\":%d,\"b\":%d,"
           "\"sp\":%d,\"pc\":%d,\"p\":%d,\"cycles\":%lu,"
           "\"flags\":{\"N\":%d,\"V\":%d,\"U\":%d,\"B\":%d,"
                      "\"D\":%d,\"I\":%d,\"Z\":%d,\"C\":%d}",
           cpu->a, cpu->x, cpu->y, cpu->z, cpu->b,
           cpu->s, cpu->pc, cpu->p, cpu->cycles,
           (cpu->p>>7)&1, (cpu->p>>6)&1, (cpu->p>>5)&1, (cpu->p>>4)&1,
           (cpu->p>>3)&1, (cpu->p>>2)&1, (cpu->p>>1)&1, cpu->p&1);
}

void json_exec_result(const char *cmd, const char *stop_reason, const cpu_t *cpu) {
    cli_printf("{\"cmd\":\"%s\",\"ok\":true,\"data\":{\"stop_reason\":\"%s\",", cmd, stop_reason);
    json_reg_fields(cpu);
    cli_printf("}}\n");
}

void json_inspect_result(const char *name, uint16_t pc, IOHandler *h, memory_t *mem, cpu_t *cpu) {
    cli_printf("{\"cmd\":\"inspect\",\"ok\":true,\"data\":{\"name\":\"%s\",\"pc\":%d", name, pc);
    if (h) {
        cli_printf(",\"device\":\"%s\",\"regs\":[", h->get_handler_name());
        for (int r = 0; r < 32; r++) {
            uint8_t val = 0;
            h->io_read(mem, (uint16_t)r, &val);
            cli_printf("%d%s", val, r < 31 ? "," : "");
        }
        cli_printf("]");
    } else if (cpu && strcasecmp(name, "cpu") == 0) {
        cli_printf(",\"cpu\":{");
        json_reg_fields(cpu);
        cli_printf("}");
    } else {
        const char *nptr = name;
        if (*nptr == '$') nptr++;
        unsigned long addr = strtoul(nptr, NULL, 16);
        if (addr == 0 && nptr[0] != '0') addr = pc;
        cli_printf(",\"memory\":{\"address\":%lu,\"bytes\":[", addr);
        for (int r = 0; r < 16; r++) {
            cli_printf("%d%s", mem_read(mem, (uint16_t)(addr + r)), r < 15 ? "," : "");
        }
        cli_printf("]}");
    }
    cli_printf("}}\n");
}

static void json_ok(const char *cmd) {
    cli_printf("{\"cmd\":\"%s\",\"ok\":true,\"data\":{}}\n", cmd);
}
void json_err(const char *cmd, const char *msg) {
    cli_printf("{\"cmd\":\"%s\",\"ok\":false,\"error\":\"%s\"}\n", cmd, msg);
}

/* --------------------------------------------------------------------------
 * Memory Snapshot & Diff
 * -------------------------------------------------------------------------- */
typedef struct cli_snap_node {
    uint16_t addr;
    uint8_t  before;
    uint8_t  after;
    uint16_t writer_pc;
    struct cli_snap_node *next;
} cli_snap_node_t;

static cli_snap_node_t *s_snap_buckets[256];
int s_snap_active = 0;

void cli_snap_reset() {
    for (int i = 0; i < 256; i++) {
        cli_snap_node_t *n = s_snap_buckets[i];
        while (n) { 
            cli_snap_node_t *nx = n->next; 
            delete n; 
            n = nx; 
        }
        s_snap_buckets[i] = NULL;
    }
}

void cli_snap_record(uint16_t addr, uint8_t before, uint8_t after, uint16_t writer_pc) {
    int b = addr & 0xFF;
    cli_snap_node_t *n = s_snap_buckets[b];
    while (n) {
        if (n->addr == addr) { n->after = after; n->writer_pc = writer_pc; return; }
        n = n->next;
    }
    n = new cli_snap_node_t();
    n->addr = addr; n->before = before; n->after = after; n->writer_pc = writer_pc;
    n->next = s_snap_buckets[b]; s_snap_buckets[b] = n;
}

typedef struct { uint16_t addr; uint8_t before; uint8_t after; uint16_t writer_pc; } cli_diff_t;
#define CLI_DIFF_CAP 4096
static cli_diff_t s_diff_buf[CLI_DIFF_CAP];
static int cli_diff_cmp(const void *a, const void *b) { return (int)((cli_diff_t*)a)->addr - (int)((cli_diff_t*)b)->addr; }

/* --------------------------------------------------------------------------
 * Validate command
 * -------------------------------------------------------------------------- */
static void cmd_validate(const char *line, cpu_t *cpu, memory_t *mem, cpu_type_t *p_cpu_type, breakpoint_list_t *breakpoints, symbol_table_t *symbols) {
    const char *p = line;
    while (*p && !isspace((unsigned char)*p)) p++; 
    while (*p && isspace((unsigned char)*p)) p++;

    unsigned long routine_addr = 0;
    if (!parse_mon_value(&p, &routine_addr)) {
        if (g_json_mode) json_err("validate", "Address required");
        else cli_printf("Usage: validate <addr> [A=v X=v ...] : [A=v X=v ...]\n");
        return;
    }

    int in_a=-1, in_x=-1, in_y=-1, in_z=-1, in_b=-1, in_s=-1, in_p=-1;
    int ex_a=-1, ex_x=-1, ex_y=-1, ex_z=-1, ex_b=-1, ex_s=-1, ex_p=-1;

    /* Parse inputs */
    while (*p && *p != ':') {
        while (*p && isspace((unsigned char)*p)) p++;
        if (*p == ':') break;
        char rname[8]; int val;
        if (sscanf(p, "%7[^=]=%i", rname, &val) == 2) {
            if      (strcasecmp(rname, "A")==0) in_a = val;
            else if (strcasecmp(rname, "X")==0) in_x = val;
            else if (strcasecmp(rname, "Y")==0) in_y = val;
            else if (strcasecmp(rname, "Z")==0) in_z = val;
            else if (strcasecmp(rname, "B")==0) in_b = val;
            else if (strcasecmp(rname, "S")==0) in_s = val;
            else if (strcasecmp(rname, "P")==0) in_p = val;
        }
        while (*p && !isspace((unsigned char)*p)) p++;
    }

    if (*p == ':') p++;

    /* Parse expectations */
    while (*p) {
        while (*p && isspace((unsigned char)*p)) p++;
        char rname[8]; int val;
        if (sscanf(p, "%7[^=]=%i", rname, &val) == 2) {
            if      (strcasecmp(rname, "A")==0) ex_a = val;
            else if (strcasecmp(rname, "X")==0) ex_x = val;
            else if (strcasecmp(rname, "Y")==0) ex_y = val;
            else if (strcasecmp(rname, "Z")==0) ex_z = val;
            else if (strcasecmp(rname, "B")==0) ex_b = val;
            else if (strcasecmp(rname, "S")==0) ex_s = val;
            else if (strcasecmp(rname, "P")==0) ex_p = val;
        }
        while (*p && !isspace((unsigned char)*p)) p++;
    }

    unsigned short old_pc = cpu->pc;
    cpu->pc = (unsigned short)routine_addr;
    if (in_a >= 0) cpu->a = (unsigned char)in_a;
    if (in_x >= 0) cpu->x = (unsigned char)in_x;
    if (in_y >= 0) cpu->y = (unsigned char)in_y;
    if (in_z >= 0) cpu->z = (unsigned char)in_z;
    if (in_b >= 0) cpu->b = (unsigned char)in_b;
    if (in_s >= 0) cpu->s = (unsigned short)in_s;
    if (in_p >= 0) cpu->p = (unsigned char)in_p;

    const int VLD_MAX = 100000;
    int steps = 0;
    const char *stop = "count";
    while (steps < VLD_MAX) {
        mem->mem_writes = 0;
        int tr = handle_trap_local(symbols, cpu, mem);
        if (tr < 0) { stop = "trap";  break; }
        if (tr > 0) continue;
        unsigned char opc = mem_read(mem, cpu->pc);
        if (opc == 0x00) { stop = "brk"; break; }
        const dispatch_entry_t *te = peek_dispatch(cpu, mem, static_cast<CPU*>(cpu)->dispatch_table(), *p_cpu_type);
        if (te && te->mnemonic && strcmp(te->mnemonic, "STP") == 0) { stop = "stp"; break; }
        if (breakpoint_hit(breakpoints, cpu)) { stop = "bp"; break; }
        static_cast<CPU*>(cpu)->step();
        steps++;
    }

    int passed = (strcmp(stop,"brk")==0 || strcmp(stop,"stp")==0);
    if (passed) {
        if (ex_a>=0 && cpu->a!=ex_a) passed = 0;
        if (ex_x>=0 && cpu->x!=ex_x) passed = 0;
        if (ex_y>=0 && cpu->y!=ex_y) passed = 0;
    }

    if (g_json_mode) {
        cli_printf("{\"cmd\":\"validate\",\"ok\":true,\"data\":{\"passed\":%d,\"stop_reason\":\"%s\",", passed, stop);
        json_reg_fields(cpu);
        cli_printf("}}\n");
    } else {
        cli_printf("Validation %s (stop=%s, PC=$%04X, steps=%d)\n", passed?"PASSED":"FAILED", stop, cpu->pc, steps);
    }
    cpu->pc = old_pc;
}

/* --------------------------------------------------------------------------
 * Main Command Processing
 * -------------------------------------------------------------------------- */

#define SKIP_CMD(lp) do { while (*(lp) && !isspace((unsigned char)*(lp))) (lp)++; } while (0)

static std::vector<std::string> split_line(const std::string& line) {
    std::vector<std::string> args;
    std::istringstream iss(line);
    std::string arg;
    while (iss >> arg) args.push_back(arg);
    return args;
}

static bool handle_manual_execution(const std::string& line, CPU *cpu, memory_t *mem, cpu_type_t cpu_type) {
    if (line.size() < 2) return true;
    std::string instr = line.substr(1);
    
    // 1. Create a temporary .asm file
    char tmp_asm[] = "manual_exec.asm";
    char tmp_prg[] = "manual_exec.prg";
    FILE *f = fopen(tmp_asm, "w");
    if (!f) return true;
    
    // Add CPU directive to match current type
    const char *cpu_dir = "_6502";
    if (cpu_type == CPU_65C02) cpu_dir = "_65c02";
    else if (cpu_type == CPU_65CE02) cpu_dir = "_65ce02";
    else if (cpu_type == CPU_45GS02) cpu_dir = "_45gs02";
    
    fprintf(f, ".cpu %s\n", cpu_dir);
    fprintf(f, "* = $FF00\n"); // Use a safe high memory area for temporary execution
    fprintf(f, "%s\n", instr.c_str());
    fclose(f);
    
    // 2. Assemble using KickAssembler
    char cmd[1024];
    char tmp_err[] = "manual_exec.err";
    snprintf(cmd, sizeof(cmd), "java -jar tools/KickAss65CE02.jar %s -o %s > %s 2>&1", tmp_asm, tmp_prg, tmp_err);
    int rc = system(cmd);
    
    if (rc == 0) {
        // 3. Load the assembled bytes into memory
        int load_addr = 0;
        int size = load_prg(mem, tmp_prg, &load_addr);
        if (size > 0) {
            uint16_t old_pc = cpu->pc;
            cpu->pc = (uint16_t)load_addr;
            
            // Execute until PC reaches load_addr + size
            while (cpu->pc < load_addr + size) {
                cpu->step();
            }
            
            cpu->pc = old_pc; // Restore PC
            if (g_json_mode) json_exec_result("exec", "ok", cpu);
            else {
                cli_printf("Executed: %s  (Registers updated, PC preserved)\n", instr.c_str());
                cli_printf("REGS A=%02X X=%02X Y=%02X S=%04X P=%02X PC=%04X Cycles=%lu\n", 
                       cpu->a, cpu->x, cpu->y, cpu->s, cpu->p, cpu->pc, cpu->cycles);
            }
        }
    } else {
        if (g_json_mode) json_err("exec", "Assembly failed");
        else {
            cli_printf("Error: Assembly failed for '%s'\n", instr.c_str());
            FILE *ef = fopen(tmp_err, "r");
            if (ef) {
                char ebuf[512];
                while (fgets(ebuf, sizeof(ebuf), ef)) {
                    cli_printf("  %s", ebuf);
                }
                fclose(ef);
            }
        }
    }
    
    remove(tmp_asm);
    remove(tmp_prg);
    remove(tmp_err);
    return true;
}

static CommandRegistry& get_registry() {
    static CommandRegistry registry;
    return registry;
}

std::vector<std::string> cli_get_completions(const char *prefix) {
    // All commands handled directly in cli_process_command (not via registry).
    static const char* const s_hardcoded[] = {
        "asm", "bload", "bsave", "clear", "cls", "diff", "disasm", "exit",
        "flag", "info", "jump", "list", "mem", "processor", "processors",
        "quit", "regs", "reset", "run", "set", "sid.info", "sid.regs",
        "snapshot", "speed", "symbols", "trace", "validate",
        "vic2.info", "vic2.regs", "vic2.savebitmap", "vic2.savescreen",
        "vic2.sprites", "write", nullptr
    };

    const size_t prefix_len = strlen(prefix);
    std::vector<std::string> out;

    // Registry-registered commands (dynamic, may overlap with hardcoded aliases)
    for (const auto& kv : get_registry().getAllCommands()) {
        if (kv.first.compare(0, prefix_len, prefix) == 0)
            out.push_back(kv.first);
    }

    // Hardcoded commands — skip duplicates already added from the registry
    for (int i = 0; s_hardcoded[i]; i++) {
        if (strncmp(s_hardcoded[i], prefix, prefix_len) != 0) continue;
        bool already = false;
        for (const auto& s : out) { if (s == s_hardcoded[i]) { already = true; break; } }
        if (!already) out.push_back(s_hardcoded[i]);
    }

    std::sort(out.begin(), out.end());
    return out;
}

#ifdef HAVE_READLINE
// Symbols available during the current readline session. Set by
// run_interactive_mode so the completion callbacks can reach them.
static const symbol_table_t *s_completion_symbols = nullptr;

// Generator for command-name completion (first word).
static char *cli_rl_cmd_generator(const char *text, int state) {
    static std::vector<std::string> s_matches;
    static size_t s_idx;
    if (state == 0) {
        s_matches = cli_get_completions(text);
        s_idx = 0;
    }
    if (s_idx < s_matches.size())
        return strdup(s_matches[s_idx++].c_str());
    return nullptr;
}

// Generator for symbol-name completion (second and later words).
// Only offers SYM_LABEL and SYM_CONSTANT entries — skips internal metadata.
static char *cli_rl_sym_generator(const char *text, int state) {
    static std::vector<std::string> s_sym_matches;
    static size_t s_sym_idx;
    if (state == 0) {
        s_sym_matches.clear();
        s_sym_idx = 0;
        if (s_completion_symbols) {
            const size_t len = strlen(text);
            for (int i = 0; i < s_completion_symbols->count; i++) {
                const symbol_t &sym = s_completion_symbols->symbols[i];
                if (sym.type != SYM_LABEL && sym.type != SYM_CONSTANT &&
                    sym.type != SYM_TRAP  && sym.type != SYM_PROCESSOR) continue;
                if (strncasecmp(sym.name, text, len) == 0)
                    s_sym_matches.push_back(sym.name);
            }
            std::sort(s_sym_matches.begin(), s_sym_matches.end());
        }
    }
    if (s_sym_idx < s_sym_matches.size())
        return strdup(s_sym_matches[s_sym_idx++].c_str());
    return nullptr;
}

// Completion dispatcher: command names for the first word, symbol names for
// all subsequent words. Always suppress readline's filename fallback.
static char **cli_rl_completer(const char *text, int start, int end) {
    (void)end;
    rl_attempted_completion_over = 1; // never fall back to filename completion
    if (start == 0)
        return rl_completion_matches(text, cli_rl_cmd_generator);
    return rl_completion_matches(text, cli_rl_sym_generator);
}
#endif

bool cli_process_command(const std::string& line,
                                  CPU *cpu, memory_t *mem,
                                  cpu_type_t *p_cpu_type,
                                  breakpoint_list_t *breakpoints,
                                  symbol_table_t *symbols) {
    // Trim leading/trailing whitespace and newlines
    std::string trimmed = line;
    size_t first = trimmed.find_first_not_of(" \t\r\n");
    if (std::string::npos == first) return true; // empty line
    size_t last = trimmed.find_last_not_of(" \t\r\n");
    trimmed = trimmed.substr(first, (last - first + 1));

    if (trimmed[0] == '.') {
        return handle_manual_execution(trimmed, cpu, mem, *p_cpu_type);
    }
    std::vector<std::string> args = split_line(trimmed);
    if (args.empty()) {
        int tr = handle_trap_local(symbols, cpu, mem);
        if (tr == 0) { 
            unsigned char opc = mem_read(mem, cpu->pc); 
            if (opc != 0x00) {
                cpu->step();
                if (mem->io_registry) mem->io_registry->tick_all(cpu->cycles);
            }
        }
        if (g_json_mode) json_exec_result("step", "step", cpu);
        else cli_printf("STOP %04X\n", cpu->pc);
        return true;
    }

    const std::string& cmd = args[0];
    CLICommand* command = get_registry().getCommand(cmd);
    if (command) {
        return command->execute(args, cpu, mem, p_cpu_type, cpu->dispatch_table(), breakpoints, symbols);
    }

    if (cmd == "quit" || cmd == "exit") return false;

    if (cmd == "run") {
        const char *stop_reason = "brk";
        struct timespec t0; clock_gettime(CLOCK_MONOTONIC, &t0);
        unsigned long cyc0 = cpu->cycles;
        while (1) {
            mem->mem_writes = 0;
            int tr = handle_trap_local(symbols, cpu, mem); if (tr < 0) { stop_reason = "trap"; break; } if (tr > 0) continue;
            unsigned char opc = mem_read(mem, cpu->pc); if (opc == 0x00) { stop_reason = "brk"; break; }
            const dispatch_entry_t *te = peek_dispatch(cpu, mem, cpu->dispatch_table(), *p_cpu_type);
            if (te->mnemonic && strcmp(te->mnemonic, "STP") == 0) { stop_reason = "stp"; break; }
            if (breakpoint_hit(breakpoints, cpu)) { stop_reason = "breakpoint"; break; }
            CPUState pre = *cpu;
            cpu->step();
            if (mem->io_registry) mem->io_registry->tick_all(cpu->cycles);
            cli_hist_push(&pre, mem);
            if (g_cli_speed > 0.0f && ((cpu->cycles - cyc0) & 0x3FF) < 8) {
                struct timespec tnow; clock_gettime(CLOCK_MONOTONIC, &tnow);
                double elapsed = (tnow.tv_sec - t0.tv_sec) + (tnow.tv_nsec - t0.tv_nsec) * 1e-9;
                double target  = (double)(cpu->cycles - cyc0) / (CLI_C64_HZ * (double)g_cli_speed);
                if (target > elapsed) {
                    double d = target - elapsed;
                    struct timespec ts = { (time_t)d, (long)((d - (time_t)d) * 1e9) };
                    nanosleep(&ts, NULL);
                }
            }
        }
        if (g_json_mode) json_exec_result("run", stop_reason, cpu);
        else cli_printf("STOP at $%04X\n", cpu->pc);
    } else if (cmd == "regs") {
        if (g_json_mode) { cli_printf("{\"cmd\":\"regs\",\"ok\":true,\"data\":{"); json_reg_fields(cpu); cli_printf("}}\n"); }
        else cli_printf("REGS A=%02X X=%02X Y=%02X S=%04X P=%02X PC=%04X Cycles=%lu\n", cpu->a, cpu->x, cpu->y, cpu->s, cpu->p, cpu->pc, cpu->cycles);
    } else if (cmd == "jump") {
        const char *p = line.c_str(); SKIP_CMD(p); unsigned long addr;
        if (parse_mon_value(&p, &addr)) { cpu->pc = (unsigned short)addr; if (g_json_mode) json_ok("jump"); else cli_printf("PC set to $%04X\n", cpu->pc); }
    } else if (cmd == "write") {
        const char *p = line.c_str(); SKIP_CMD(p); unsigned long addr, val;
        if (parse_mon_value(&p, &addr) && parse_mon_value(&p, &val)) { mem_write(mem, (unsigned short)addr, (unsigned char)val); if (g_json_mode) json_ok("write"); else cli_printf("OK\n"); }
    } else if (cmd == "list") {
        if (g_json_mode) {
            cli_printf("{\"cmd\":\"list\",\"ok\":true,\"data\":{\"breakpoints\":[");
            for (int i = 0; i < breakpoints->count; i++) {
                cli_printf("{\"address\":%u,\"enabled\":%d,\"condition\":\"%s\"}%s", 
                    breakpoints->breakpoints[i].address,
                    breakpoints->breakpoints[i].enabled,
                    breakpoints->breakpoints[i].condition,
                    i < breakpoints->count - 1 ? "," : "");
            }
            cli_printf("]}}\n");
        } else {
            breakpoint_list(breakpoints);
        }
    } else if (cmd == "clear") {
        const char *p = line.c_str(); SKIP_CMD(p); unsigned long addr;
        if (parse_mon_value(&p, &addr)) {
            if (breakpoint_remove(breakpoints, (unsigned short)addr)) {
                if (g_json_mode) cli_printf("{\"cmd\":\"clear\",\"ok\":true,\"data\":{\"address\":%lu}}\n", addr & 0xFFFF);
                else cli_printf("Breakpoint at $%04lX removed.\n", addr & 0xFFFF);
            } else {
                if (g_json_mode) json_err("clear", "No breakpoint at address");
                else cli_printf("No breakpoint at $%04lX.\n", addr & 0xFFFF);
            }
        } else {
            if (g_json_mode) json_err("clear", "Usage: clear <addr>");
            else cli_printf("Usage: clear <addr>\n");
        }
    } else if (cmd == "set") {
        const char *p = line.c_str(); SKIP_CMD(p); char reg[16]; unsigned long val;
        if (sscanf(p, "%15s", reg) == 1) {
            p += strlen(reg);
            if (parse_mon_value(&p, &val)) {
                if      (strcasecmp(reg, "A") == 0) cpu->a = (uint8_t)val;
                else if (strcasecmp(reg, "X") == 0) cpu->x = (uint8_t)val;
                else if (strcasecmp(reg, "Y") == 0) cpu->y = (uint8_t)val;
                else if (strcasecmp(reg, "Z") == 0) cpu->z = (uint8_t)val;
                else if (strcasecmp(reg, "B") == 0) cpu->b = (uint8_t)val;
                else if (strcasecmp(reg, "S") == 0 || strcasecmp(reg, "SP") == 0) cpu->s = (uint16_t)val;
                else if (strcasecmp(reg, "P") == 0) cpu->p = (uint8_t)val;
                else if (strcasecmp(reg, "PC") == 0) cpu->pc = (uint16_t)val;
                else {
                    if (g_json_mode) json_err("set", "Unknown register");
                    else cli_printf("Unknown register: %s\n", reg);
                    return true;
                }
                if (g_json_mode) json_ok("set");
                else cli_printf("%s set to $%04lX\n", reg, val);
            } else {
                if (g_json_mode) json_err("set", "Value required");
                else cli_printf("Usage: set <reg> <val>\n");
            }
        } else {
            if (g_json_mode) json_err("set", "Usage: set <reg> <val>");
            else cli_printf("Usage: set <reg> <val>\n");
        }
    } else if (cmd == "flag") {
        const char *p = line.c_str(); SKIP_CMD(p); char flag_name[16]; unsigned long val;
        if (sscanf(p, "%15s", flag_name) == 1) {
            p += strlen(flag_name);
            if (parse_mon_value(&p, &val)) {
                uint8_t bit = 0;
                if      (strcasecmp(flag_name, "C") == 0) bit = FLAG_C;
                else if (strcasecmp(flag_name, "Z") == 0) bit = FLAG_Z;
                else if (strcasecmp(flag_name, "I") == 0) bit = FLAG_I;
                else if (strcasecmp(flag_name, "D") == 0) bit = FLAG_D;
                else if (strcasecmp(flag_name, "B") == 0) bit = FLAG_B;
                else if (strcasecmp(flag_name, "V") == 0) bit = FLAG_V;
                else if (strcasecmp(flag_name, "N") == 0) bit = FLAG_N;
                else if (strcasecmp(flag_name, "E") == 0) bit = FLAG_E;
                else {
                    if (g_json_mode) json_err("flag", "Unknown flag");
                    else cli_printf("Unknown flag: %s\n", flag_name);
                    return true;
                }
                if (val) cpu->p |= bit; else cpu->p &= ~bit;
                if (g_json_mode) json_ok("flag");
                else cli_printf("Flag %s set to %d\n", flag_name, val ? 1 : 0);
            } else {
                if (g_json_mode) json_err("flag", "Value required");
                else cli_printf("Usage: flag <flag> <0|1>\n");
            }
        } else {
            if (g_json_mode) json_err("flag", "Usage: flag <flag> <0|1>");
            else cli_printf("Usage: flag <flag> <0|1>\n");
        }
    } else if (cmd == "bload") {
        const char *p = line.c_str(); SKIP_CMD(p); char file[256]; unsigned long addr = cpu->pc;
        if (sscanf(p, " \"%[^\"]\"", file) == 1) {
            p = strchr(p, '\"') + 1; p = strchr(p, '\"') + 1;
            parse_mon_value(&p, &addr);
            FILE *f = fopen(file, "rb");
            if (f) {
                int n = 0, c;
                while ((c = fgetc(f)) != EOF) {
                    if (addr + n < 0x10000) mem_write(mem, (unsigned short)(addr + n), (unsigned char)c);
                    n++;
                }
                fclose(f);
                if (g_json_mode) cli_printf("{\"cmd\":\"bload\",\"ok\":true,\"data\":{\"address\":%lu,\"size\":%d}}\n", addr, n);
                else cli_printf("Loaded %d bytes at $%04lX\n", n, addr);
            } else {
                if (g_json_mode) json_err("bload", "Failed to open file");
                else cli_printf("Error: Could not open %s\n", file);
            }
        } else {
             if (g_json_mode) json_err("bload", "Filename required");
             else cli_printf("Usage: bload \"file\" [addr]\n");
        }
    } else if (cmd == "bsave") {
        const char *p = line.c_str(); SKIP_CMD(p); char file[256]; unsigned long start, end;
        if (sscanf(p, " \"%[^\"]\"", file) == 1) {
            p = strchr(p, '\"') + 1; p = strchr(p, '\"') + 1;
            if (parse_mon_value(&p, &start) && parse_mon_value(&p, &end)) {
                FILE *f = fopen(file, "wb");
                if (f) {
                    for (unsigned long a = start; a <= end; a++) {
                        fputc(mem_read(mem, (unsigned short)a), f);
                    }
                    fclose(f);
                    if (g_json_mode) cli_printf("{\"cmd\":\"bsave\",\"ok\":true,\"data\":{\"path\":\"%s\"}}\n", file);
                    else cli_printf("Saved $%04lX-$%04lX to %s\n", start, end, file);
                } else {
                    if (g_json_mode) json_err("bsave", "Failed to open file");
                    else cli_printf("Error: Could not open %s for writing\n", file);
                }
            } else {
                if (g_json_mode) json_err("bsave", "Range required");
                else cli_printf("Usage: bsave \"file\" <start> <end>\n");
            }
        } else {
            if (g_json_mode) json_err("bsave", "Filename required");
            else cli_printf("Usage: bsave \"file\" <start> <end>\n");
        }
    } else if (cmd == "info") {
        const char *p = line.c_str(); SKIP_CMD(p); char mnem[16];
        if (sscanf(p, "%15s", mnem) == 1) {
            print_opcode_info(*p_cpu_type, mnem);
        } else {
            if (g_json_mode) json_err("info", "Mnemonic required");
            else cli_printf("Usage: info <mnemonic>\n");
        }
    } else if (cmd == "mem") {
        const char *p = line.c_str(); SKIP_CMD(p); unsigned long addr, len = 16, tmp;
        if (parse_mon_value(&p, &addr)) {
            if (parse_mon_value(&p, &tmp)) len = tmp;
            if (g_json_mode) {
                cli_printf("{\"cmd\":\"mem\",\"ok\":true,\"data\":{\"address\":%lu,\"length\":%lu,\"bytes\":[", addr & 0xFFFF, len);
                for (unsigned long i = 0; i < len; i++) cli_printf("%d%s", mem_read(mem, (unsigned short)(addr + i)), i<len-1?",":"");
                cli_printf("]}}\n");
            } else {
                for (unsigned long i = 0; i < len; i++) { if (i % 16 == 0) cli_printf("\n%04lX: ", addr + i); cli_printf("%02X ", mem_read(mem, (unsigned short)(addr + i))); }
                cli_printf("\n");
            }
        }
    } else if (cmd == "reset") {
        cpu->reset(); if (*p_cpu_type == CPU_45GS02) cpu->set_flag(FLAG_E, 1);
        if (g_json_mode) json_ok("reset"); else cli_printf("Reset.\n");
    } else if (cmd == "processors") {
        if (g_json_mode) cli_printf("{\"cmd\":\"processors\",\"ok\":true,\"data\":{\"processors\":[\"6502\",\"6502-undoc\",\"65c02\",\"65ce02\",\"45gs02\"]}}\n");
        else list_processors();
    } else if (cmd == "processor") {
        char type[16]; if (sscanf(line.c_str(), "%*s %15s", type) == 1) {
            if      (strcmp(type, "6502") == 0)       *p_cpu_type = CPU_6502;
            else if (strcmp(type, "6502-undoc") == 0) *p_cpu_type = CPU_6502_UNDOCUMENTED;
            else if (strcmp(type, "65c02") == 0)      *p_cpu_type = CPU_65C02;
            else if (strcmp(type, "65ce02") == 0)     *p_cpu_type = CPU_65CE02;
            else if (strcmp(type, "45gs02") == 0)     *p_cpu_type = CPU_45GS02;
            dispatch_table_t *dt = cpu->dispatch_table();
            memset(dt, 0, sizeof(dispatch_table_t));
            dispatch_build(dt, opcodes_6502,   OPCODES_6502_COUNT,   *p_cpu_type);
            if (*p_cpu_type == CPU_6502_UNDOCUMENTED) dispatch_build(dt, opcodes_6502_undoc, OPCODES_6502_UNDOC_COUNT, *p_cpu_type);
            if (*p_cpu_type >= CPU_65C02)  dispatch_build(dt, opcodes_65c02,   OPCODES_65C02_COUNT,   *p_cpu_type);
            if (*p_cpu_type >= CPU_65CE02) dispatch_build(dt, opcodes_65ce02,  OPCODES_65CE02_COUNT,  *p_cpu_type);
            if (*p_cpu_type >= CPU_45GS02) dispatch_build(dt, opcodes_45gs02,  OPCODES_45GS02_COUNT,  *p_cpu_type);
            if (g_json_mode) json_ok("processor"); else cli_printf("Processor: %s\n", type);
        }
    } else if (cmd == "sid.info") {
        if (g_json_mode) { cli_printf("{\"cmd\":\"sid.info\",\"ok\":true,\"data\":"); sid_json_info(mem); cli_printf("}\n"); }
        else sid_print_info(mem);
    } else if (cmd == "sid.regs") {
        if (g_json_mode) { cli_printf("{\"cmd\":\"sid.regs\",\"ok\":true,\"data\":"); sid_json_regs(mem); cli_printf("}\n"); }
        else sid_print_regs(mem);
    } else if (cmd == "vic2.info") {
        if (g_json_mode) { cli_printf("{\"cmd\":\"vic2.info\",\"ok\":true,\"data\":"); vic2_json_info(mem); cli_printf("}\n"); }
        else vic2_print_info(mem);
    } else if (cmd == "vic2.regs") {
        if (g_json_mode) { cli_printf("{\"cmd\":\"vic2.regs\",\"ok\":true,\"data\":"); vic2_json_regs(mem); cli_printf("}\n"); }
        else vic2_print_regs(mem);
    } else if (cmd == "vic2.sprites") {
        if (g_json_mode) { cli_printf("{\"cmd\":\"vic2.sprites\",\"ok\":true,\"data\":"); vic2_json_sprites(mem); cli_printf("}\n"); }
        else vic2_print_sprites(mem);
    } else if (cmd == "vic2.savescreen") {
        std::string path = (args.size() > 1) ? args[1] : "vic2screen.ppm";
        if (vic2_render_ppm(mem, path.c_str()) == 0) {
            if (g_json_mode) cli_printf("{\"cmd\":\"vic2.savescreen\",\"ok\":true,\"data\":{\"path\":\"%s\"}}\n", path.c_str());
            else cli_printf("Screen saved to %s\n", path.c_str());
        } else {
            if (g_json_mode) json_err("vic2.savescreen", "Failed to save PPM");
            else cli_printf("Error: Failed to save PPM to %s\n", path.c_str());
        }
    } else if (cmd == "vic2.savebitmap") {
        std::string path = (args.size() > 1) ? args[1] : "vic2bitmap.ppm";
        if (vic2_render_ppm_active(mem, path.c_str()) == 0) {
            if (g_json_mode) cli_printf("{\"cmd\":\"vic2.savebitmap\",\"ok\":true,\"data\":{\"path\":\"%s\"}}\n", path.c_str());
            else cli_printf("Bitmap saved to %s\n", path.c_str());
        } else {
            if (g_json_mode) json_err("vic2.savebitmap", "Failed to save PPM");
            else cli_printf("Error: Failed to save PPM to %s\n", path.c_str());
        }
    } else if (cmd == "speed") {
        float s = 0.0f; const char *p = line.c_str(); SKIP_CMD(p);
        if (sscanf(p, " %f", &s) == 1) g_cli_speed = s >= 0.0f ? s : 0.0f;
        if (g_json_mode) cli_printf("{\"cmd\":\"speed\",\"ok\":true,\"data\":{\"scale\":%.4f}}\n", g_cli_speed);
        else cli_printf("Speed: %.4fx\n", g_cli_speed);
    } else if (cmd == "snapshot") {
        cli_snap_reset(); s_snap_active = 1; if (g_json_mode) json_ok("snapshot"); else cli_printf("Memory snapshot taken.\n");
    } else if (cmd == "diff") {
        if (!s_snap_active) { if (g_json_mode) json_err("diff", "No snapshot"); else cli_printf("No snapshot.\n"); }
        else {
            int count = 0;
            for (int i = 0; i < 256; i++) {
                cli_snap_node_t *n = s_snap_buckets[i];
                while (n) {
                    if (n->before != n->after && count < CLI_DIFF_CAP) {
                        s_diff_buf[count].addr = n->addr; s_diff_buf[count].before = n->before;
                        s_diff_buf[count].after = n->after; s_diff_buf[count].writer_pc = n->writer_pc;
                        count++;
                    }
                    n = n->next;
                }
            }
            qsort(s_diff_buf, (size_t)count, sizeof(cli_diff_t), cli_diff_cmp);
            if (g_json_mode) {
                cli_printf("{\"cmd\":\"diff\",\"ok\":true,\"data\":{\"changes\":[");
                for (int i = 0; i < count; i++) cli_printf("{\"addr\":%d,\"before\":%d,\"after\":%d}%s", s_diff_buf[i].addr, s_diff_buf[i].before, s_diff_buf[i].after, i<count-1?",":"");
                cli_printf("]}}\n");
            } else {
                for (int i = 0; i < count; i++) cli_printf("  $%04X: %02X -> %02X by $%04X\n", s_diff_buf[i].addr, s_diff_buf[i].before, s_diff_buf[i].after, s_diff_buf[i].writer_pc);
            }
        }
    } else if (cmd == "validate") {
        cmd_validate(line.c_str(), cpu, mem, p_cpu_type, breakpoints, symbols);
    } else if (cmd == "disasm") {
        const char *p = line.c_str(); SKIP_CMD(p); unsigned long tmp;
        unsigned short daddr = parse_mon_value(&p, &tmp) ? (unsigned short)tmp : cpu->pc;
        int dcount = parse_mon_value(&p, &tmp) ? (int)tmp : 15;
        char dbuf[80]; for (int i = 0; i < dcount; i++) { int consumed = disasm_one(mem, cpu->dispatch_table(), *p_cpu_type, daddr, dbuf, sizeof(dbuf)); cli_printf("%s\n", dbuf); daddr = (unsigned short)(daddr + consumed); }
    } else {
        if (!g_json_mode) cli_printf("Unknown command: %s\n", cmd.c_str());
    }

    return true;
}

void run_interactive_mode(cpu_t *cpu, memory_t *mem,
                                 cpu_type_t *p_cpu_type,
                                 unsigned short start_addr, breakpoint_list_t *breakpoints,
                                 symbol_table_t *symbols,
                                 const std::vector<std::string>& initial_cmds) {
    (void)start_addr;
    char line_buf[256];
    setvbuf(stdout, NULL, _IONBF, 0);

    CPU *cpu_obj = static_cast<CPU*>(cpu);
    for (const auto& line : initial_cmds) {
        if (!cli_process_command(line, cpu_obj, mem, p_cpu_type, breakpoints, symbols))
            return;
    }

    if (!g_json_mode) cli_printf("6502 Simulator Interactive Mode\nType 'help' for commands.\n");
#ifdef HAVE_READLINE
    rl_attempted_completion_function = cli_rl_completer;
    s_completion_symbols = symbols; // expose to symbol-completion callback
    while (1) {
        char *rl_line = readline("> ");
        if (!rl_line) break; // EOF / Ctrl-D
        strncpy(line_buf, rl_line, sizeof(line_buf) - 1);
        line_buf[sizeof(line_buf) - 1] = '\0';
        if (*rl_line) add_history(rl_line); // non-empty lines go into readline history
        free(rl_line);
        if (!cli_process_command(line_buf, cpu_obj, mem, p_cpu_type, breakpoints, symbols))
            break;
    }
#else
    while (1) {
        cli_printf("> "); if (!fgets(line_buf, sizeof(line_buf), stdin)) break;
        if (!cli_process_command(line_buf, cpu_obj, mem, p_cpu_type, breakpoints, symbols))
            break;
    }
#endif
}

void list_processors(void) { cli_printf("Available Processors: 6502, 6502-undoc, 65c02, 65ce02, 45gs02\n"); }
void list_opcodes(cpu_type_t type) { (void)type; cli_printf("Opcode listing not implemented in CLI helpers yet.\n"); }

void print_detailed_help(const char *cmd_name) {
    CLICommand* command = get_registry().getCommand(cmd_name);
    if (command) {
        command->render_help();
        return;
    }

    /* Hardcoded commands in cli_process_command */
    if (strcmp(cmd_name, "run") == 0) {
        cli_printf("Usage: run\nContinuously execute instructions until a BRK instruction, breakpoint, or trap is encountered.\n");
    } else if (strcmp(cmd_name, "regs") == 0) {
        cli_printf("Usage: regs\nDisplay current CPU register values (A, X, Y, S, P, PC) and total cycle count.\n");
    } else if (strcmp(cmd_name, "jump") == 0) {
        cli_printf("Usage: jump <addr>\nSet the Program Counter (PC) to the specified address.\n");
    } else if (strcmp(cmd_name, "write") == 0) {
        cli_printf("Usage: write <addr> <val>\nWrite a byte value <val> to the specified memory address <addr>.\n");
    } else if (strcmp(cmd_name, "mem") == 0) {
        cli_printf("Usage: mem <addr> [len]\nDump 'len' bytes of memory starting at <addr>. Default length is 16 bytes.\n");
    } else if (strcmp(cmd_name, "list") == 0) {
        cli_printf("Usage: list\nList all currently set breakpoints and their conditions.\n");
    } else if (strcmp(cmd_name, "clear") == 0) {
        cli_printf("Usage: clear <addr>\nRemove the breakpoint at the specified address.\n");
    } else if (strcmp(cmd_name, "set") == 0) {
        cli_printf("Usage: set <reg> <val>\nSet CPU register <reg> to value <val>. Supported: A, X, Y, Z, B, S, P, PC.\n");
    } else if (strcmp(cmd_name, "flag") == 0) {
        cli_printf("Usage: flag <f> <0|1>\nSet or clear CPU status flag <f>. Supported: C, Z, I, D, B, V, N, E.\n");
    } else if (strcmp(cmd_name, "reset") == 0) {
        cli_printf("Usage: reset\nPerform a CPU reset (sets PC to reset vector, etc.).\n");
    } else if (strcmp(cmd_name, "processors") == 0) {
        cli_printf("Usage: processors\nList all supported CPU architectures.\n");
    } else if (strcmp(cmd_name, "processor") == 0) {
        cli_printf("Usage: processor <type>\nSwitch the current CPU architecture. Example: processor 65c02\n");
    } else if (strcmp(cmd_name, "info") == 0) {
        cli_printf("Usage: info <mnemonic>\nShow detailed information about a specific instruction mnemonic.\n");
    } else if (strcmp(cmd_name, "bload") == 0) {
        cli_printf("Usage: bload \"file\" [addr]\nLoad a raw binary file into memory. Default address is the current PC.\n");
    } else if (strcmp(cmd_name, "bsave") == 0) {
        cli_printf("Usage: bsave \"file\" <start> <end>\nSave a memory range [start, end] to a binary file.\n");
    } else if (strcmp(cmd_name, "disasm") == 0) {
        cli_printf("Usage: disasm [addr [count]]\nDisassemble instructions starting at 'addr'. Default is current PC and 15 instructions.\n");
    } else if (strncmp(cmd_name, "vic2.", 5) == 0) {
        cli_printf("VIC-II commands: vic2.info, vic2.regs, vic2.sprites, vic2.savescreen, vic2.savebitmap\n");
    } else if (strncmp(cmd_name, "sid.", 4) == 0) {
        cli_printf("SID commands: sid.info, sid.regs\n");
    } else if (strcmp(cmd_name, "snapshot") == 0) {
        cli_printf("Usage: snapshot\nTake a memory snapshot for later comparison with 'diff'.\n");
    } else if (strcmp(cmd_name, "diff") == 0) {
        cli_printf("Usage: diff\nShow memory changes since the last 'snapshot'.\n");
    } else if (strcmp(cmd_name, "speed") == 0) {
        cli_printf("Usage: speed [scale]\nGet or set execution speed (1.0 = normal C64 PAL, 0.0 = unlimited).\n");
    } else if (strcmp(cmd_name, "quit") == 0 || strcmp(cmd_name, "exit") == 0) {
        cli_printf("Usage: quit\nExit the simulator.\n");
    } else {
        cli_printf("Unknown command: %s\n", cmd_name);
    }
}

void print_help(const char *progname) {
    cli_printf("6502 Simulator v%s\nUsage: %s [options] <file.asm>\n\n", SIM_VERSION, progname);
    cli_printf("Options:\n"
           "  -M, --machine <TYPE> Select machine: raw6502, c64, c128, mega65, x16\n"
           "  -p, --processor <CPU> Select processor: 6502, 6502-undoc, 65c02, 65ce02, 45gs02\n"
           "  -I        Interactive mode\n"
           "  -J        JSON output mode (use with -I)\n"
           "  -l        List processors\n"
           "  -b <ADDR> Set breakpoint\n"
           "  -L, --limit <CYCLES> Maximum cycles to execute (default 1000000)\n"
           "  -S, --speed <SCALE>  Execution speed (default 1.0 = C64 PAL, 0.0 = unlimited)\n"
           "  -a <ADDR> Start address (default $0200)\n"
           "  -m <RANGE> Show memory dump (e.g., -m $0200:$0210)\n"
           "  --debug   Enable verbose instruction logging\n"
           "  -vv       Enable loader/init debug output\n"
           "  -vvv      Enable per-instruction trace (implies --debug)\n\n"
           "Testing:\n"
           "  ./sim6502 -a $0200 tests/6502_basic.asm\n"
           "  ./sim6502 -m $0200:$0210 tests/pseudoops_text_align.asm\n");
}

void print_opcode_info(cpu_type_t cpu_type, const char *mnemonic) {
    char mnem_upper[16]; int mi = 0;
    for (; mi < 15 && mnemonic[mi]; mi++) mnem_upper[mi] = (char)toupper((unsigned char)mnemonic[mi]);
    mnem_upper[mi] = '\0';
    if (!g_json_mode) cli_printf("%-6s  %-20s  %-12s  %-10s  %s\n", "MNEM", "MODE", "SYNTAX", "CYCLES", "OPCODE");

    struct { opcode_handler_t *h; int n; } tables[] = {
        { opcodes_6502,   OPCODES_6502_COUNT },
        { (cpu_type >= CPU_65C02)  ? opcodes_65c02   : nullptr, (cpu_type >= CPU_65C02)  ? OPCODES_65C02_COUNT  : 0 },
        { (cpu_type >= CPU_65CE02) ? opcodes_65ce02  : nullptr, (cpu_type >= CPU_65CE02) ? OPCODES_65CE02_COUNT : 0 },
        { (cpu_type >= CPU_45GS02) ? opcodes_45gs02  : nullptr, (cpu_type >= CPU_45GS02) ? OPCODES_45GS02_COUNT : 0 },
    };
    for (auto& t : tables) {
        if (!t.h) continue;
        for (int i = 0; i < t.n; i++) {
            if (strcasecmp(t.h[i].mnemonic, mnemonic) != 0 || t.h[i].opcode_len == 0) continue;
            char opbytes[16] = "";
            for (int j = 0; j < t.h[i].opcode_len; j++) { char tmp[8]; snprintf(tmp, sizeof(tmp), j > 0 ? " %02X" : "%02X", t.h[i].opcode_bytes[j]); strncat(opbytes, tmp, sizeof(opbytes) - strlen(opbytes) - 1); }
            if (g_json_mode) cli_printf("{\"mnemonic\":\"%s\",\"opcode\":\"%s\"}\n", mnem_upper, opbytes);
            else cli_printf("%-6s  %-20s  %-12s  %d      %s\n", mnem_upper, mode_name(t.h[i].mode), mnem_upper, t.h[i].cycles_6502, opbytes);
        }
    }
}
