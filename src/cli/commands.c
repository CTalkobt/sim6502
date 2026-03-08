#include "commands.h"
#include "cpu_engine.h"
#include "condition.h"
#include "vic2.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <time.h>

/* Speed throttle: 0.0 = unlimited, 1.0 = C64 PAL (~985 kHz) */
static float  g_cli_speed   = 0.0f;
static const double CLI_C64_HZ = 985248.0;

/* JSON mode flag: 0 = plain text, 1 = JSON output */
static int g_json_mode = 0;

void cli_set_json_mode(int v) { g_json_mode = v; }

/* --------------------------------------------------------------------------
 * CLI execution history (local ring buffer, 4096 entries)
 * -------------------------------------------------------------------------- */
#define CLI_HIST_CAP 4096

typedef struct {
    cpu_t    pre_cpu;
    uint8_t  delta_count;
    uint16_t delta_addr[16];
    uint8_t  delta_old[16];
} cli_hist_entry_t;

static cli_hist_entry_t s_cli_hist[CLI_HIST_CAP];
static int              s_cli_hist_write = 0;
static int              s_cli_hist_count = 0;
static int              s_cli_hist_pos   = 0;

static void cli_hist_push(const cpu_t *pre, const memory_t *mem) {
    if (s_cli_hist_pos > 0) {
        s_cli_hist_write = ((s_cli_hist_write - s_cli_hist_pos) % CLI_HIST_CAP + CLI_HIST_CAP) % CLI_HIST_CAP;
        s_cli_hist_count -= s_cli_hist_pos;
        if (s_cli_hist_count < 0) s_cli_hist_count = 0;
        s_cli_hist_pos = 0;
    }
    cli_hist_entry_t *e = &s_cli_hist[s_cli_hist_write % CLI_HIST_CAP];
    e->pre_cpu = *pre;
    int dc = mem->mem_writes < 16 ? mem->mem_writes : 16;
    e->delta_count = (uint8_t)dc;
    for (int i = 0; i < dc; i++) {
        e->delta_addr[i] = mem->mem_addr[i];
        e->delta_old[i]  = mem->mem_old_val[i];
    }
    s_cli_hist_write = (s_cli_hist_write + 1) % CLI_HIST_CAP;
    if (s_cli_hist_count < CLI_HIST_CAP) s_cli_hist_count++;
}

static int cli_hist_step_back(cpu_t *cpu, memory_t *mem) {
    if (s_cli_hist_pos >= s_cli_hist_count) return 0;
    int idx = ((s_cli_hist_write - 1 - s_cli_hist_pos) % CLI_HIST_CAP + CLI_HIST_CAP) % CLI_HIST_CAP;
    cli_hist_entry_t *e = &s_cli_hist[idx];
    *cpu = e->pre_cpu;
    for (int i = 0; i < e->delta_count; i++)
        mem->mem[e->delta_addr[i]] = e->delta_old[i];
    s_cli_hist_pos++;
    return 1;
}

static int cli_hist_step_fwd(cpu_t *cpu, memory_t *mem, dispatch_table_t *dt, cpu_type_t cpu_type) {
    if (s_cli_hist_pos == 0) return 0;
    int idx = ((s_cli_hist_write - s_cli_hist_pos) % CLI_HIST_CAP + CLI_HIST_CAP) % CLI_HIST_CAP;
    cli_hist_entry_t *e = &s_cli_hist[idx];
    *cpu = e->pre_cpu;
    mem->mem_writes = 0;
    execute_from_mem(cpu, mem, dt, cpu_type);
    s_cli_hist_pos--;
    return 1;
}

static int handle_trap_local(const symbol_table_t *st, cpu_t *cpu, memory_t *mem) {
    for (int i = 0; i < st->count; i++) {
        if (st->symbols[i].type != SYM_TRAP) continue;
        if (st->symbols[i].address != cpu->pc) continue;
        printf("[TRAP] %-20s $%04X  A=%02X X=%02X Y=%02X",
            st->symbols[i].name, cpu->pc, cpu->a, cpu->x, cpu->y);
        if (cpu->pc > 0) printf(" Z=%02X B=%02X", cpu->z, cpu->b);
        printf(" S=%02X P=%02X", cpu->s, cpu->p);
        if (st->symbols[i].comment[0]) printf("  ; %s", st->symbols[i].comment);
        printf("\n");
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

/* Emit flat register fields (no surrounding braces) — used inside data objects */
static void json_reg_fields(const cpu_t *cpu) {
    printf("\"a\":%d,\"x\":%d,\"y\":%d,\"z\":%d,\"b\":%d,"
           "\"sp\":%d,\"pc\":%d,\"p\":%d,\"cycles\":%lu,"
           "\"flags\":{\"N\":%d,\"V\":%d,\"U\":%d,\"B\":%d,"
                      "\"D\":%d,\"I\":%d,\"Z\":%d,\"C\":%d}",
           cpu->a, cpu->x, cpu->y, cpu->z, cpu->b,
           cpu->s, cpu->pc, cpu->p, cpu->cycles,
           (cpu->p>>7)&1, (cpu->p>>6)&1, (cpu->p>>5)&1, (cpu->p>>4)&1,
           (cpu->p>>3)&1, (cpu->p>>2)&1, (cpu->p>>1)&1, cpu->p&1);
}

/* Emit a full JSON response for execution commands */
static void json_exec_result(const char *cmd, const char *stop_reason, const cpu_t *cpu) {
    printf("{\"cmd\":\"%s\",\"ok\":true,\"data\":{\"stop_reason\":\"%s\",", cmd, stop_reason);
    json_reg_fields(cpu);
    printf("}}\n");
}

/* Emit a simple ok/error JSON response (for silent-success commands) */
static void json_ok(const char *cmd) {
    printf("{\"cmd\":\"%s\",\"ok\":true,\"data\":{}}\n", cmd);
}
static void json_err(const char *cmd, const char *msg) {
    printf("{\"cmd\":\"%s\",\"ok\":false,\"error\":\"%s\"}\n", cmd, msg);
}

/* --------------------------------------------------------------------------
 * Mode name and operand syntax helpers (used by print_opcode_info)
 * -------------------------------------------------------------------------- */

static const char *mode_name_str(unsigned char mode) {
    switch (mode) {
    case MODE_IMPLIED:            return "implied";
    case MODE_IMMEDIATE:          return "immediate";
    case MODE_IMMEDIATE_WORD:     return "immediate_word";
    case MODE_ZP:                 return "zp";
    case MODE_ZP_X:               return "zp_x";
    case MODE_ZP_Y:               return "zp_y";
    case MODE_ABSOLUTE:           return "absolute";
    case MODE_ABSOLUTE_X:         return "absolute_x";
    case MODE_ABSOLUTE_Y:         return "absolute_y";
    case MODE_INDIRECT:           return "indirect";
    case MODE_INDIRECT_X:         return "indirect_x";
    case MODE_INDIRECT_Y:         return "indirect_y";
    case MODE_ZP_INDIRECT:        return "zp_indirect";
    case MODE_ABS_INDIRECT_Y:     return "abs_indirect_y";
    case MODE_ZP_INDIRECT_Z:      return "zp_indirect_z";
    case MODE_SP_INDIRECT_Y:      return "sp_indirect_y";
    case MODE_ABS_INDIRECT_X:     return "abs_indirect_x";
    case MODE_RELATIVE:           return "relative";
    case MODE_RELATIVE_LONG:      return "relative_long";
    case MODE_ZP_INDIRECT_FLAT:   return "zp_indirect_flat";
    case MODE_ZP_INDIRECT_Z_FLAT: return "zp_indirect_z_flat";
    default:                      return "unknown";
    }
}

static const char *mode_operand_template(unsigned char mode) {
    switch (mode) {
    case MODE_IMPLIED:            return "";
    case MODE_IMMEDIATE:          return " #$nn";
    case MODE_IMMEDIATE_WORD:     return " #$nnnn";
    case MODE_ZP:                 return " $nn";
    case MODE_ZP_X:               return " $nn,X";
    case MODE_ZP_Y:               return " $nn,Y";
    case MODE_ABSOLUTE:           return " $nnnn";
    case MODE_ABSOLUTE_X:         return " $nnnn,X";
    case MODE_ABSOLUTE_Y:         return " $nnnn,Y";
    case MODE_INDIRECT:           return " ($nnnn)";
    case MODE_INDIRECT_X:         return " ($nn,X)";
    case MODE_INDIRECT_Y:         return " ($nn),Y";
    case MODE_ZP_INDIRECT:        return " ($nn)";
    case MODE_ABS_INDIRECT_Y:     return " ($nnnn),Y";
    case MODE_ZP_INDIRECT_Z:      return " ($nn),Z";
    case MODE_SP_INDIRECT_Y:      return " ($nn,SP),Y";
    case MODE_ABS_INDIRECT_X:     return " ($nnnn,X)";
    case MODE_RELATIVE:           return " $nnnn";
    case MODE_RELATIVE_LONG:      return " $nnnn";
    case MODE_ZP_INDIRECT_FLAT:   return " [$nn]";
    case MODE_ZP_INDIRECT_Z_FLAT: return " [$nn],Z";
    default:                      return " ?";
    }
}

/* --------------------------------------------------------------------------
 * run_asm_mode
 * -------------------------------------------------------------------------- */

void run_asm_mode(memory_t *mem, symbol_table_t *symbols,
                  opcode_handler_t *handlers, int num_handlers,
                  cpu_type_t cpu_type, int *asm_pc) {
    char buf[512];
    printf("Assembling from $%04X  (enter '.' on a blank line to finish)\n", (unsigned int)*asm_pc);
    for (;;) {
        printf("$%04X> ", (unsigned int)*asm_pc); fflush(stdout);
        if (!fgets(buf, sizeof(buf), stdin)) break;
        size_t blen = strlen(buf);
        while (blen > 0 && (buf[blen-1] == '\n' || buf[blen-1] == '\r')) buf[--blen] = '\0';
        const char *p = buf; while (*p && isspace((unsigned char)*p)) p++;
        if (p[0] == '.' && p[1] == '\0') break;
        if (!*p || *p == ';') continue;
        int base_pc = *asm_pc;
        if (*p == '.') {
            handle_pseudo_op(p, &cpu_type, asm_pc, mem, symbols, NULL);
            if (*asm_pc - base_pc > 0) {
                printf("$%04X:", base_pc);
                int show = (*asm_pc - base_pc) < 4 ? (*asm_pc - base_pc) : 4;
                for (int i = 0; i < show; i++) printf(" %02X", mem->mem[base_pc + i]);
                if (*asm_pc - base_pc > 4) printf(" ...");
                printf("\n");
            } else printf("       -> PC=$%04X\n", (unsigned int)*asm_pc);
            continue;
        }
        const char *colon = strchr(p, ':');
        if (colon) {
            char lname[64]; int llen = (int)(colon - p); if (llen > 63) llen = 63;
            memcpy(lname, p, (size_t)llen); lname[llen] = '\0';
            while (llen > 0 && isspace((unsigned char)lname[llen-1])) lname[--llen] = '\0';
            symbol_add(symbols, lname, (unsigned short)*asm_pc, SYM_LABEL, "asm");
            printf("       %s = $%04X\n", lname, (unsigned int)*asm_pc);
            const char *after = colon + 1; while (*after && isspace((unsigned char)*after)) after++;
            if (*after == '.') { handle_pseudo_op(after, &cpu_type, asm_pc, mem, symbols, NULL); continue; }
            if (!*after || *after == ';') continue;
        }
        instruction_t instr; parse_line(buf, &instr, symbols, *asm_pc);
        if (!instr.op[0]) continue;
        int enc = encode_to_mem(mem, *asm_pc, &instr, handlers, num_handlers, cpu_type);
        if (enc < 0) { printf("       error: cannot assemble: %s\n", p); continue; }
        *asm_pc += enc;
        printf("$%04X:", base_pc);
        int show = enc < 4 ? enc : 4;
        for (int i = 0; i < show; i++) printf(" %02X", mem->mem[base_pc + i]);
        if (enc > 4) printf(" ...");
        for (int i = show; i < 4; i++) printf("   ");
        printf("  %s\n", p);
    }
}

/* --------------------------------------------------------------------------
 * run_interactive_mode
 * -------------------------------------------------------------------------- */

void run_interactive_mode(cpu_t *cpu, memory_t *mem,
                                 opcode_handler_t **p_handlers, int *p_num_handlers,
                                 cpu_type_t *p_cpu_type, dispatch_table_t *dt,
                                 unsigned short start_addr, breakpoint_list_t *breakpoints,
                                 symbol_table_t *symbols) {
    char line[256]; char cmd[32];
    setvbuf(stdout, NULL, _IONBF, 0);
    if (!g_json_mode)
        printf("6502 Simulator Interactive Mode\nType 'help' for commands.\n");
    while (1) {
        printf("> "); if (!fgets(line, sizeof(line), stdin)) break;
        if (sscanf(line, "%31s", cmd) != 1) {
            int tr = handle_trap_local(symbols, cpu, mem);
            if (tr == 0) { unsigned char opc = mem_read(mem, cpu->pc); if (opc != 0x00) execute_from_mem(cpu, mem, dt, *p_cpu_type); }
            if (g_json_mode) json_exec_result("step", "step", cpu);
            else printf("STOP %04X\n", cpu->pc);
            continue;
        }
#define SKIP_CMD(lp) do { while (*(lp) && !isspace((unsigned char)*(lp))) (lp)++; } while (0)
        if (strcmp(cmd, "quit") == 0 || strcmp(cmd, "exit") == 0) break;
        else if (strcmp(cmd, "help") == 0) {
            printf("Commands: step [n], run, stepback (sb), stepfwd (sf),\n");
            printf("          break <addr>, clear <addr>, list, regs,\n");
            printf("          mem <addr> [len], write <addr> <val>, reset,\n");
            printf("          processors, processor <type>, info <opcode>,\n");
            printf("          jump <addr>, set <reg> <val>, flag <flag> <0|1>,\n");
            printf("          bload \"file\" [addr], bsave \"file\" <start> <end>,\n");
            printf("          asm [addr], disasm [addr [count]],\n");
            printf("          vic2.info, vic2.regs, vic2.sprites,\n");
            printf("          vic2.savescreen [file], vic2.savebitmap [file],\n");
            printf("          speed [scale]  (1.0=C64, 0=unlimited), quit\n");
        } else if (strcmp(cmd, "break") == 0) {
            const char *p = line; SKIP_CMD(p); unsigned long addr;
            if (parse_mon_value(&p, &addr)) {
                while (*p && isspace((unsigned char)*p)) p++;
                breakpoint_add(breakpoints, (unsigned short)addr, *p ? p : NULL);
                if (g_json_mode) printf("{\"cmd\":\"break\",\"ok\":true,\"data\":{\"address\":%lu}}\n", addr & 0xFFFF);
            } else {
                if (g_json_mode) json_err("break", "Usage: break <addr> [condition]");
                else printf("Usage: break <addr> [condition]\n");
            }
        } else if (strcmp(cmd, "clear") == 0) {
            const char *p = line; SKIP_CMD(p); unsigned long addr;
            if (parse_mon_value(&p, &addr)) {
                breakpoint_remove(breakpoints, (unsigned short)addr);
                if (g_json_mode) printf("{\"cmd\":\"clear\",\"ok\":true,\"data\":{\"address\":%lu}}\n", addr & 0xFFFF);
            } else {
                if (g_json_mode) json_err("clear", "Usage: clear <addr>");
                else printf("Usage: clear <addr>\n");
            }
        } else if (strcmp(cmd, "list") == 0) {
            if (g_json_mode) {
                printf("{\"cmd\":\"list\",\"ok\":true,\"data\":{\"breakpoints\":[");
                for (int i = 0; i < breakpoints->count; i++) {
                    if (i > 0) printf(",");
                    printf("{\"index\":%d,\"address\":%d,\"enabled\":%d,\"condition\":\"%s\"}",
                           i, breakpoints->breakpoints[i].address,
                           breakpoints->breakpoints[i].enabled,
                           breakpoints->breakpoints[i].condition);
                }
                printf("]}}\n");
            } else {
                breakpoint_list(breakpoints);
            }
        } else if (strcmp(cmd, "jump") == 0) {
            const char *p = line; SKIP_CMD(p); unsigned long addr;
            if (parse_mon_value(&p, &addr)) {
                cpu->pc = (unsigned short)addr;
                if (g_json_mode) printf("{\"cmd\":\"jump\",\"ok\":true,\"data\":{\"pc\":%d}}\n", cpu->pc);
                else printf("PC set to $%04X\n", cpu->pc);
            } else {
                if (g_json_mode) json_err("jump", "Usage: jump <addr>");
                else printf("Usage: jump <addr>\n");
            }
        } else if (strcmp(cmd, "set") == 0) {
            char reg[16]; if (sscanf(line, "%*s %15s", reg) == 1) {
                const char *p = line; SKIP_CMD(p); while (*p && isspace((unsigned char)*p)) p++; SKIP_CMD(p);
                unsigned long val; if (parse_mon_value(&p, &val)) {
                    if      (strcmp(reg, "A") == 0 || strcmp(reg, "a") == 0) cpu->a = (unsigned char)val;
                    else if (strcmp(reg, "X") == 0 || strcmp(reg, "x") == 0) cpu->x = (unsigned char)val;
                    else if (strcmp(reg, "Y") == 0 || strcmp(reg, "y") == 0) cpu->y = (unsigned char)val;
                    else if (strcmp(reg, "Z") == 0 || strcmp(reg, "z") == 0) cpu->z = (unsigned char)val;
                    else if (strcmp(reg, "B") == 0 || strcmp(reg, "b") == 0) cpu->b = (unsigned char)val;
                    else if (strcmp(reg, "S") == 0 || strcmp(reg, "s") == 0 || strcmp(reg, "SP") == 0) cpu->s = (unsigned short)val;
                    else if (strcmp(reg, "P") == 0 || strcmp(reg, "p") == 0) cpu->p = (unsigned char)val;
                    else if (strcmp(reg, "PC") == 0 || strcmp(reg, "pc") == 0) cpu->pc = (unsigned short)val;
                    else {
                        if (g_json_mode) { char buf[64]; snprintf(buf, sizeof(buf), "Unknown register: %s", reg); json_err("set", buf); }
                        else printf("Unknown register: %s\n", reg);
                    }
                    if (g_json_mode) json_ok("set");
                }
            }
        } else if (strcmp(cmd, "flag") == 0) {
            char fname[8]; int fval;
            if (sscanf(line, "%*s %7s %d", fname, &fval) == 2) {
                unsigned char fbit = 0;
                if      (fname[0]=='C'||fname[0]=='c') fbit = FLAG_C;
                else if (fname[0]=='Z'||fname[0]=='z') fbit = FLAG_Z;
                else if (fname[0]=='I'||fname[0]=='i') fbit = FLAG_I;
                else if (fname[0]=='D'||fname[0]=='d') fbit = FLAG_D;
                else if (fname[0]=='B'||fname[0]=='b') fbit = FLAG_B;
                else if (fname[0]=='V'||fname[0]=='v') fbit = FLAG_V;
                else if (fname[0]=='N'||fname[0]=='n') fbit = FLAG_N;
                if (fbit) {
                    set_flag(cpu, fbit, fval);
                    if (g_json_mode) json_ok("flag");
                } else {
                    if (g_json_mode) { char buf[64]; snprintf(buf, sizeof(buf), "Unknown flag: %s", fname); json_err("flag", buf); }
                    else printf("Unknown flag: %s\n", fname);
                }
            } else {
                if (g_json_mode) json_err("flag", "Usage: flag <C|Z|I|D|B|V|N> <0|1>");
                else printf("Usage: flag <C|Z|I|D|B|V|N> <0|1>\n");
            }
        } else if (strcmp(cmd, "write") == 0) {
            const char *p = line; SKIP_CMD(p); unsigned long addr, val;
            if (parse_mon_value(&p, &addr) && parse_mon_value(&p, &val)) {
                mem->mem[addr & 0xFFFF] = (unsigned char)(val & 0xFF);
                if (g_json_mode) printf("{\"cmd\":\"write\",\"ok\":true,\"data\":{\"address\":%lu,\"value\":%lu}}\n",
                                        addr & 0xFFFF, val & 0xFF);
                else printf("OK\n");
            } else {
                if (g_json_mode) json_err("write", "Usage: write <addr> <val>");
                else printf("Usage: write <addr> <val>\n");
            }
        } else if (strcmp(cmd, "run") == 0) {
            const char *stop_reason = "brk";
            struct timespec t0; clock_gettime(CLOCK_MONOTONIC, &t0);
            unsigned long cyc0 = cpu->cycles;
            while (1) {
                mem->mem_writes = 0;
                int tr = handle_trap_local(symbols, cpu, mem); if (tr < 0) { stop_reason = "trap"; break; } if (tr > 0) continue;
                unsigned char opc = mem_read(mem, cpu->pc); if (opc == 0x00) { stop_reason = "brk"; break; }
                const dispatch_entry_t *te = peek_dispatch(cpu, mem, dt, *p_cpu_type);
                if (te->mnemonic && strcmp(te->mnemonic, "STP") == 0) { stop_reason = "stp"; break; }
                if (breakpoint_hit(breakpoints, cpu)) { stop_reason = "breakpoint"; break; }
                cpu_t pre = *cpu;
                execute_from_mem(cpu, mem, dt, *p_cpu_type);
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
            else printf("STOP at $%04X\n", cpu->pc);
        } else if (strcmp(cmd, "processors") == 0) {
            if (g_json_mode) printf("{\"cmd\":\"processors\",\"ok\":true,\"data\":{\"processors\":[\"6502\",\"65c02\",\"65ce02\",\"45gs02\"]}}\n");
            else list_processors();
        } else if (strcmp(cmd, "info") == 0) {
            char mnem[16]; if (sscanf(line, "%*s %15s", mnem) == 1)
                print_opcode_info(*p_handlers, *p_num_handlers, mnem);
            else {
                if (g_json_mode) json_err("info", "Usage: info <mnemonic>");
                else printf("Usage: info <mnemonic>\n");
            }
        } else if (strcmp(cmd, "processor") == 0) {
            char type[16]; if (sscanf(line, "%*s %15s", type) == 1) {
                if      (strcmp(type, "6502") == 0)   { *p_handlers = opcodes_6502;   *p_num_handlers = OPCODES_6502_COUNT;   *p_cpu_type = CPU_6502; }
                else if (strcmp(type, "65c02") == 0)  { *p_handlers = opcodes_65c02;  *p_num_handlers = OPCODES_65C02_COUNT;  *p_cpu_type = CPU_65C02; }
                else if (strcmp(type, "65ce02") == 0) { *p_handlers = opcodes_65ce02; *p_num_handlers = OPCODES_65CE02_COUNT; *p_cpu_type = CPU_65CE02; }
                else if (strcmp(type, "45gs02") == 0) { *p_handlers = opcodes_45gs02; *p_num_handlers = OPCODES_45GS02_COUNT; *p_cpu_type = CPU_45GS02; }
                dispatch_build(dt, *p_handlers, *p_num_handlers, *p_cpu_type);
                if (g_json_mode) printf("{\"cmd\":\"processor\",\"ok\":true,\"data\":{\"type\":\"%s\"}}\n", type);
            }
        } else if (strcmp(cmd, "regs") == 0) {
            if (g_json_mode) {
                printf("{\"cmd\":\"regs\",\"ok\":true,\"data\":{");
                json_reg_fields(cpu);
                printf("}}\n");
            } else {
                printf("REGS A=%02X X=%02X Y=%02X S=%02X P=%02X PC=%04X Cycles=%lu\n",
                       cpu->a, cpu->x, cpu->y, cpu->s, cpu->p, cpu->pc, cpu->cycles);
            }
        } else if (strcmp(cmd, "mem") == 0) {
            const char *p = line; SKIP_CMD(p); unsigned long addr, len = 16, tmp;
            if (parse_mon_value(&p, &addr)) {
                if (parse_mon_value(&p, &tmp)) len = tmp;
                if (g_json_mode) {
                    printf("{\"cmd\":\"mem\",\"ok\":true,\"data\":{\"address\":%lu,\"length\":%lu,\"bytes\":[",
                           addr & 0xFFFF, len);
                    for (unsigned long i = 0; i < len; i++) {
                        if (i > 0) printf(",");
                        printf("%d", mem->mem[(addr + i) & 0xFFFF]);
                    }
                    printf("]}}\n");
                } else {
                    for (unsigned long i = 0; i < len; i++) {
                        if (i % 16 == 0) printf("\n%04lX: ", addr + i);
                        printf("%02X ", mem->mem[addr + i]);
                    }
                    printf("\n");
                }
            } else {
                if (g_json_mode) json_err("mem", "Usage: mem <addr> [len]");
                else printf("Usage: mem <addr> [len]\n");
            }
        } else if (strcmp(cmd, "asm") == 0) {
            const char *p = line; SKIP_CMD(p); unsigned long tmp;
            int asm_pc = parse_mon_value(&p, &tmp) ? (int)tmp : (int)cpu->pc;
            run_asm_mode(mem, symbols, *p_handlers, *p_num_handlers, *p_cpu_type, &asm_pc);
        } else if (strcmp(cmd, "disasm") == 0) {
            const char *p = line; SKIP_CMD(p); unsigned long tmp;
            unsigned short daddr = parse_mon_value(&p, &tmp) ? (unsigned short)tmp : cpu->pc;
            int dcount = parse_mon_value(&p, &tmp) ? (int)tmp : 15;
            if (g_json_mode) {
                printf("{\"cmd\":\"disasm\",\"ok\":true,\"data\":{\"instructions\":[");
                for (int i = 0; i < dcount; i++) {
                    disasm_entry_t entry;
                    int consumed = disasm_one_entry(mem, dt, *p_cpu_type, daddr, &entry);
                    if (i > 0) printf(",");
                    printf("{\"address\":%d,\"size\":%d,\"bytes\":\"%s\","
                           "\"mnemonic\":\"%s\",\"operand\":\"%s\",\"cycles\":%d}",
                           entry.address, entry.size, entry.bytes,
                           entry.mnemonic, entry.operand, entry.cycles);
                    daddr = (unsigned short)(daddr + consumed);
                }
                printf("]}}\n");
            } else {
                char dbuf[80];
                for (int i = 0; i < dcount; i++) {
                    int consumed = disasm_one(mem, dt, *p_cpu_type, daddr, dbuf, sizeof(dbuf));
                    printf("%s\n", dbuf);
                    daddr = (unsigned short)(daddr + consumed);
                }
            }
        } else if (strcmp(cmd, "reset") == 0) {
            cpu_init(cpu); cpu->pc = start_addr;
            if (*p_cpu_type == CPU_45GS02) set_flag(cpu, FLAG_E, 1);
            if (g_json_mode) json_ok("reset");
        } else if (strcmp(cmd, "step") == 0) {
            const char *p = line; SKIP_CMD(p); unsigned long tmp;
            int steps = parse_mon_value(&p, &tmp) ? (int)tmp : 1;
            const char *stop_reason = "step";
            for (int i = 0; i < steps; i++) {
                mem->mem_writes = 0;
                int tr = handle_trap_local(symbols, cpu, mem); if (tr < 0) { stop_reason = "trap"; break; } if (tr > 0) continue;
                unsigned char opc = mem_read(mem, cpu->pc); if (opc == 0x00) { stop_reason = "brk"; break; }
                const dispatch_entry_t *te = peek_dispatch(cpu, mem, dt, *p_cpu_type);
                if (te->mnemonic && strcmp(te->mnemonic, "STP") == 0) { stop_reason = "stp"; break; }
                cpu_t pre = *cpu;
                execute_from_mem(cpu, mem, dt, *p_cpu_type);
                cli_hist_push(&pre, mem);
            }
            if (g_json_mode) json_exec_result("step", stop_reason, cpu);
            else printf("STOP $%04X\n", cpu->pc);
        } else if (strcmp(cmd, "stepback") == 0 || strcmp(cmd, "sb") == 0) {
            if (cli_hist_step_back(cpu, mem)) {
                if (g_json_mode) json_exec_result("stepback", "back", cpu);
                else printf("BACK $%04X\n", cpu->pc);
            } else {
                if (g_json_mode) json_err("stepback", "No history to step back into");
                else printf("No history to step back into.\n");
            }
        } else if (strcmp(cmd, "stepfwd") == 0 || strcmp(cmd, "sf") == 0) {
            if (cli_hist_step_fwd(cpu, mem, dt, *p_cpu_type)) {
                if (g_json_mode) json_exec_result("stepfwd", "forward", cpu);
                else printf("FWD $%04X\n", cpu->pc);
            } else {
                if (g_json_mode) json_err("stepfwd", "Already at the present");
                else printf("Already at the present.\n");
            }
        } else if (strcmp(cmd, "bload") == 0) {
            const char *p = line; SKIP_CMD(p);
            while (*p && isspace((unsigned char)*p)) p++;
            if (*p != '"') {
                if (g_json_mode) json_err("bload", "Usage: bload \\\"file\\\" [addr]");
                else printf("Usage: bload \"file\" [addr]\n");
            } else {
                p++;
                char fname[512]; int fi = 0;
                while (*p && *p != '"' && fi < 511) fname[fi++] = *p++;
                fname[fi] = '\0';
                if (*p == '"') p++;
                while (*p && isspace((unsigned char)*p)) p++;
                const char *ext = strrchr(fname, '.');
                int is_prg = ext && (ext[1]=='p'||ext[1]=='P') &&
                                    (ext[2]=='r'||ext[2]=='R') &&
                                    (ext[3]=='g'||ext[3]=='G') && !ext[4];
                if (is_prg) {
                    unsigned long override_val = 0;
                    int has_ovr = parse_mon_value(&p, &override_val);
                    FILE *f = fopen(fname, "rb");
                    if (!f) { printf("Error: cannot open '%s'\n", fname); }
                    else {
                        int lo = fgetc(f), hi = fgetc(f);
                        if (lo == EOF || hi == EOF) { printf("Error: file too short\n"); fclose(f); }
                        else {
                            unsigned short load_addr = has_ovr
                                ? (unsigned short)override_val
                                : (unsigned short)((unsigned)lo | ((unsigned)hi << 8));
                            int n = 0, c;
                            while ((c = fgetc(f)) != EOF) {
                                unsigned int dst = (unsigned int)load_addr + (unsigned int)n;
                                if (dst < 65536) mem->mem[dst] = (unsigned char)c;
                                n++;
                            }
                            fclose(f);
                            cpu->pc = load_addr;
                            printf("bload: %d bytes at $%04X (PRG)\n", n, (unsigned)load_addr);
                        }
                    }
                } else {
                    unsigned long addr = 0;
                    if (!parse_mon_value(&p, &addr)) { printf("Usage: bload \"file.bin\" <addr>\n"); }
                    else {
                        int n = load_binary_to_mem(mem, (int)addr, fname);
                        if (n < 0) printf("Error: cannot open '%s'\n", fname);
                        else { cpu->pc = (unsigned short)addr; printf("bload: %d bytes at $%04X\n", n, (unsigned)addr); }
                    }
                }
            }
        } else if (strcmp(cmd, "bsave") == 0) {
            const char *p = line; SKIP_CMD(p);
            while (*p && isspace((unsigned char)*p)) p++;
            if (*p != '"') { printf("Usage: bsave \"file\" <start> <end>\n"); }
            else {
                p++;
                char fname[512]; int fi = 0;
                while (*p && *p != '"' && fi < 511) fname[fi++] = *p++;
                fname[fi] = '\0';
                if (*p == '"') p++;
                while (*p && isspace((unsigned char)*p)) p++;
                unsigned long start_a = 0, end_a = 0;
                if (!parse_mon_value(&p, &start_a) || !parse_mon_value(&p, &end_a)) {
                    printf("Usage: bsave \"file\" <start> <end>\n");
                } else if (end_a <= start_a || end_a > 0x10000) {
                    printf("Error: invalid range $%04lX-$%04lX\n", start_a, end_a);
                } else {
                    const char *ext = strrchr(fname, '.');
                    int is_prg = ext && (ext[1]=='p'||ext[1]=='P') &&
                                        (ext[2]=='r'||ext[2]=='R') &&
                                        (ext[3]=='g'||ext[3]=='G') && !ext[4];
                    unsigned long count = end_a - start_a;
                    FILE *f = fopen(fname, "wb");
                    if (!f) { printf("Error: cannot write '%s'\n", fname); }
                    else {
                        if (is_prg) {
                            fputc((int)(start_a & 0xFF),        f);
                            fputc((int)((start_a >> 8) & 0xFF), f);
                        }
                        for (unsigned long i = start_a; i < end_a; i++)
                            fputc(mem->mem[i], f);
                        fclose(f);
                        printf("bsave: %lu bytes at $%04lX saved to '%s'%s\n",
                               count, start_a, fname, is_prg ? " (PRG)" : "");
                    }
                }
            }
        } else if (strcmp(cmd, "speed") == 0) {
            float s = 0.0f;
            const char *p = line; while (*p && !isspace((unsigned char)*p)) p++;
            if (sscanf(p, " %f", &s) == 1) {
                if (s < 0.0f) s = 0.0f;
                g_cli_speed = s;
                if (g_json_mode) {
                    if (g_cli_speed == 0.0f)
                        printf("{\"cmd\":\"speed\",\"ok\":true,\"data\":{\"scale\":0.0,\"unlimited\":true,\"hz\":0}}\n");
                    else
                        printf("{\"cmd\":\"speed\",\"ok\":true,\"data\":{\"scale\":%.4f,\"unlimited\":false,\"hz\":%.0f}}\n",
                               g_cli_speed, CLI_C64_HZ * g_cli_speed);
                } else {
                    if (g_cli_speed == 0.0f) printf("Speed: unlimited\n");
                    else printf("Speed: %.4fx C64 (%.0f Hz)\n", g_cli_speed, CLI_C64_HZ * g_cli_speed);
                }
            } else {
                if (g_json_mode) {
                    if (g_cli_speed == 0.0f)
                        printf("{\"cmd\":\"speed\",\"ok\":true,\"data\":{\"scale\":0.0,\"unlimited\":true,\"hz\":0}}\n");
                    else
                        printf("{\"cmd\":\"speed\",\"ok\":true,\"data\":{\"scale\":%.4f,\"unlimited\":false,\"hz\":%.0f}}\n",
                               g_cli_speed, CLI_C64_HZ * g_cli_speed);
                } else {
                    if (g_cli_speed == 0.0f) printf("Speed: unlimited (use 'speed 1.0' for C64 speed)\n");
                    else printf("Speed: %.4fx C64 (%.0f Hz)\n", g_cli_speed, CLI_C64_HZ * g_cli_speed);
                }
            }
        } else if (strcmp(cmd, "vic2.info") == 0) {
            if (g_json_mode) {
                printf("{\"cmd\":\"vic2.info\",\"ok\":true,\"data\":");
                vic2_json_info(mem);
                printf("}\n");
            } else {
                vic2_print_info(mem);
            }
        } else if (strcmp(cmd, "vic2.regs") == 0) {
            if (g_json_mode) {
                printf("{\"cmd\":\"vic2.regs\",\"ok\":true,\"data\":");
                vic2_json_regs(mem);
                printf("}\n");
            } else {
                vic2_print_regs(mem);
            }
        } else if (strcmp(cmd, "vic2.sprites") == 0) {
            if (g_json_mode) {
                printf("{\"cmd\":\"vic2.sprites\",\"ok\":true,\"data\":");
                vic2_json_sprites(mem);
                printf("}\n");
            } else {
                vic2_print_sprites(mem);
            }
        } else if (strcmp(cmd, "vic2.savescreen") == 0) {
            const char *p = line; SKIP_CMD(p);
            while (*p && isspace((unsigned char)*p)) p++;
            char fbuf[512];
            if (*p && *p != '\n' && *p != '\r') {
                int fi = 0;
                while (*p && *p != '\n' && *p != '\r' && fi < 511) fbuf[fi++] = *p++;
                fbuf[fi] = '\0';
            } else {
                strcpy(fbuf, "vic2screen.ppm");
            }
            if (vic2_render_ppm(mem, fbuf) == 0) {
                if (g_json_mode) printf("{\"cmd\":\"vic2.savescreen\",\"ok\":true,\"data\":{\"path\":\"%s\",\"width\":%d,\"height\":%d}}\n",
                                        fbuf, VIC2_FRAME_W, VIC2_FRAME_H);
                else printf("Saved %dx%d PPM to '%s'\n", VIC2_FRAME_W, VIC2_FRAME_H, fbuf);
            } else {
                if (g_json_mode) { char buf[576]; snprintf(buf, sizeof(buf), "cannot write %s", fbuf); json_err("vic2.savescreen", buf); }
                else printf("Error: cannot write '%s'\n", fbuf);
            }
        } else if (strcmp(cmd, "vic2.savebitmap") == 0) {
            const char *p = line; SKIP_CMD(p);
            while (*p && isspace((unsigned char)*p)) p++;
            char fbuf[512];
            if (*p && *p != '\n' && *p != '\r') {
                int fi = 0;
                while (*p && *p != '\n' && *p != '\r' && fi < 511) fbuf[fi++] = *p++;
                fbuf[fi] = '\0';
            } else {
                strcpy(fbuf, "vic2bitmap.ppm");
            }
            if (vic2_render_ppm_active(mem, fbuf) == 0) {
                if (g_json_mode) printf("{\"cmd\":\"vic2.savebitmap\",\"ok\":true,\"data\":{\"path\":\"%s\",\"width\":%d,\"height\":%d}}\n",
                                        fbuf, VIC2_ACTIVE_W, VIC2_ACTIVE_H);
                else printf("Saved %dx%d active-area PPM to '%s'\n", VIC2_ACTIVE_W, VIC2_ACTIVE_H, fbuf);
            } else {
                if (g_json_mode) { char buf[576]; snprintf(buf, sizeof(buf), "cannot write %s", fbuf); json_err("vic2.savebitmap", buf); }
                else printf("Error: cannot write '%s'\n", fbuf);
            }
        }
#undef SKIP_CMD
    }
}

/* --------------------------------------------------------------------------
 * Information Display
 * -------------------------------------------------------------------------- */

void print_help(const char *progname) {
    printf("6502 Simulator\nUsage: %s [options] <file.asm>\n\n", progname);
    printf("Options:\n"
           "  -p <CPU>  Select processor: 6502, 65c02, 65ce02, 45gs02\n"
           "  -I        Interactive mode\n"
           "  -J        JSON output mode (use with -I)\n"
           "  -l        List processors\n"
           "  -b <ADDR> Set breakpoint\n");
}

void list_processors(void) {
    printf("Available Processors: 6502, 65c02, 65ce02, 45gs02\n");
}

void list_opcodes(cpu_type_t type) {
    (void)type; printf("Opcode listing not implemented in CLI helpers yet.\n");
}

void print_opcode_info(opcode_handler_t *handlers, int num_handlers, const char *mnemonic) {
    /* Convert to uppercase for comparison and output */
    char mnem_upper[16];
    int mi = 0;
    for (; mi < 15 && mnemonic[mi]; mi++)
        mnem_upper[mi] = (char)toupper((unsigned char)mnemonic[mi]);
    mnem_upper[mi] = '\0';

    int found = 0;
    int first = 1;

    if (g_json_mode)
        printf("{\"cmd\":\"info\",\"ok\":true,\"data\":{\"mnemonic\":\"%s\",\"modes\":[", mnem_upper);
    else
        printf("%-6s  %-20s  %-12s  %-10s  %s\n",
               "MNEM", "MODE", "SYNTAX", "CYCLES", "OPCODE");

    for (int i = 0; i < num_handlers; i++) {
        if (strcasecmp(handlers[i].mnemonic, mnemonic) != 0) continue;
        if (handlers[i].opcode_len == 0) continue;
        found++;

        /* Build opcode bytes hex string */
        char opbytes[16] = "";
        for (int j = 0; j < handlers[i].opcode_len; j++) {
            char tmp[8];
            snprintf(tmp, sizeof(tmp), j > 0 ? " %02X" : "%02X", handlers[i].opcode_bytes[j]);
            strncat(opbytes, tmp, sizeof(opbytes) - strlen(opbytes) - 1);
        }

        /* Total instruction size = prefix bytes + instruction bytes */
        int instr_bytes = get_instruction_length(handlers[i].mode);
        int total_size  = (int)handlers[i].opcode_len - 1 + instr_bytes;

        /* Syntax: mnemonic + operand template */
        char syntax[40];
        snprintf(syntax, sizeof(syntax), "%s%s", mnem_upper,
                 mode_operand_template(handlers[i].mode));

        if (g_json_mode) {
            if (!first) printf(",");
            first = 0;
            printf("{\"mode\":\"%s\",\"syntax\":\"%s\","
                   "\"cycles_6502\":%d,\"cycles_65c02\":%d,"
                   "\"cycles_65ce02\":%d,\"cycles_45gs02\":%d,"
                   "\"opcode\":\"%s\",\"size\":%d}",
                   mode_name_str(handlers[i].mode), syntax,
                   handlers[i].cycles_6502, handlers[i].cycles_65c02,
                   handlers[i].cycles_65ce02, handlers[i].cycles_45gs02,
                   opbytes, total_size);
        } else {
            printf("%-6s  %-20s  %-12s  %d/%d/%d/%d      %s\n",
                   mnem_upper,
                   mode_name_str(handlers[i].mode),
                   syntax,
                   handlers[i].cycles_6502, handlers[i].cycles_65c02,
                   handlers[i].cycles_65ce02, handlers[i].cycles_45gs02,
                   opbytes);
        }
    }

    if (g_json_mode) {
        if (!found)
            printf("{\"cmd\":\"info\",\"ok\":false,\"error\":\"Unknown mnemonic: %s\"}\n", mnem_upper);
        else
            printf("]}}\n");
    } else {
        if (!found) printf("Unknown mnemonic: %s\n", mnem_upper);
    }
}
