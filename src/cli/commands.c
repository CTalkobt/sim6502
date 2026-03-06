#include "commands.h"
#include "cpu_engine.h"
#include "condition.h"
#include "vic2.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>

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
        /* Branched off: truncate "future" entries */
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
        if (enc < 0) { printf("       error: cannot encode '%s'\n", instr.op); continue; }
        *asm_pc += enc;
        printf("$%04X:", base_pc);
        int show = enc < 4 ? enc : 4;
        for (int i = 0; i < show; i++) printf(" %02X", mem->mem[base_pc + i]);
        if (enc > 4) printf(" ...");
        for (int i = show; i < 4; i++) printf("   ");
        printf("  %s\n", p);
    }
}

void run_interactive_mode(cpu_t *cpu, memory_t *mem, 
                                 opcode_handler_t **p_handlers, int *p_num_handlers,
                                 cpu_type_t *p_cpu_type, dispatch_table_t *dt,
                                 unsigned short start_addr, breakpoint_list_t *breakpoints,
                                 symbol_table_t *symbols) {
    char line[256]; char cmd[32];
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("6502 Simulator Interactive Mode\nType 'help' for commands.\n");
    while (1) {
        printf("> "); if (!fgets(line, sizeof(line), stdin)) break;
        if (sscanf(line, "%31s", cmd) != 1) {
            int tr = handle_trap_local(symbols, cpu, mem);
            if (tr == 0) { unsigned char opc = mem_read(mem, cpu->pc); if (opc != 0x00) execute_from_mem(cpu, mem, dt, *p_cpu_type); }
            printf("STOP %04X\n", cpu->pc); continue;
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
            printf("          vic2.info, vic2.savescreen [file], quit\n");
        } else if (strcmp(cmd, "break") == 0) {
            const char *p = line; SKIP_CMD(p); unsigned long addr;
            if (parse_mon_value(&p, &addr)) { while (*p && isspace((unsigned char)*p)) p++; breakpoint_add(breakpoints, (unsigned short)addr, *p ? p : NULL); }
            else printf("Usage: break <addr> [condition]\n");
        } else if (strcmp(cmd, "clear") == 0) {
            const char *p = line; SKIP_CMD(p); unsigned long addr;
            if (parse_mon_value(&p, &addr)) breakpoint_remove(breakpoints, (unsigned short)addr);
            else printf("Usage: clear <addr>\n");
        } else if (strcmp(cmd, "list") == 0) breakpoint_list(breakpoints);
        else if (strcmp(cmd, "jump") == 0) {
            const char *p = line; SKIP_CMD(p); unsigned long addr;
            if (parse_mon_value(&p, &addr)) { cpu->pc = (unsigned short)addr; printf("PC set to $%04X\n", cpu->pc); }
            else printf("Usage: jump <addr>\n");
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
                    else printf("Unknown register: %s\n", reg);
                }
            }
        } else if (strcmp(cmd, "run") == 0) {
            while (1) {
                mem->mem_writes = 0;
                int tr = handle_trap_local(symbols, cpu, mem); if (tr < 0) break; if (tr > 0) continue;
                unsigned char opc = mem_read(mem, cpu->pc); if (opc == 0x00) break;
                const dispatch_entry_t *te = peek_dispatch(cpu, mem, dt, *p_cpu_type);
                if (te->mnemonic && strcmp(te->mnemonic, "STP") == 0) break;
                if (breakpoint_hit(breakpoints, cpu)) break;
                cpu_t pre = *cpu;
                execute_from_mem(cpu, mem, dt, *p_cpu_type);
                cli_hist_push(&pre, mem);
            }
            printf("STOP at $%04X\n", cpu->pc);
        } else if (strcmp(cmd, "processors") == 0) list_processors();
        else if (strcmp(cmd, "info") == 0) { char mnem[16]; if (sscanf(line, "%*s %15s", mnem) == 1) print_opcode_info(*p_handlers, *p_num_handlers, mnem); }
        else if (strcmp(cmd, "processor") == 0) {
            char type[16]; if (sscanf(line, "%*s %15s", type) == 1) {
                if      (strcmp(type, "6502") == 0) { *p_handlers = opcodes_6502; *p_num_handlers = OPCODES_6502_COUNT; *p_cpu_type = CPU_6502; }
                else if (strcmp(type, "65c02") == 0) { *p_handlers = opcodes_65c02; *p_num_handlers = OPCODES_65C02_COUNT; *p_cpu_type = CPU_65C02; }
                else if (strcmp(type, "65ce02") == 0) { *p_handlers = opcodes_65ce02; *p_num_handlers = OPCODES_65CE02_COUNT; *p_cpu_type = CPU_65CE02; }
                else if (strcmp(type, "45gs02") == 0) { *p_handlers = opcodes_45gs02; *p_num_handlers = OPCODES_45GS02_COUNT; *p_cpu_type = CPU_45GS02; }
                dispatch_build(dt, *p_handlers, *p_num_handlers, *p_cpu_type);
            }
        } else if (strcmp(cmd, "regs") == 0) {
            printf("REGS A=%02X X=%02X Y=%02X S=%02X P=%02X PC=%04X Cycles=%lu\n", cpu->a, cpu->x, cpu->y, cpu->s, cpu->p, cpu->pc, cpu->cycles);
        } else if (strcmp(cmd, "mem") == 0) {
            const char *p = line; SKIP_CMD(p); unsigned long addr, len = 16, tmp;
            if (parse_mon_value(&p, &addr)) {
                if (parse_mon_value(&p, &tmp)) len = tmp;
                for (unsigned long i = 0; i < len; i++) { if (i % 16 == 0) printf("\n%04lX: ", addr + i); printf("%02X ", mem->mem[addr + i]); }
                printf("\n");
            }
        } else if (strcmp(cmd, "asm") == 0) {
            const char *p = line; SKIP_CMD(p); unsigned long tmp;
            int asm_pc = parse_mon_value(&p, &tmp) ? (int)tmp : (int)cpu->pc;
            run_asm_mode(mem, symbols, *p_handlers, *p_num_handlers, *p_cpu_type, &asm_pc);
        } else if (strcmp(cmd, "disasm") == 0) {
            const char *p = line; SKIP_CMD(p); unsigned long tmp;
            unsigned short daddr = parse_mon_value(&p, &tmp) ? (unsigned short)tmp : cpu->pc;
            int dcount = parse_mon_value(&p, &tmp) ? (int)tmp : 15;
            char dbuf[80]; for (int i = 0; i < dcount; i++) { int consumed = disasm_one(mem, dt, *p_cpu_type, daddr, dbuf, sizeof(dbuf)); printf("%s\n", dbuf); daddr += consumed; }
        } else if (strcmp(cmd, "reset") == 0) {
            cpu_init(cpu); cpu->pc = start_addr; if (*p_cpu_type == CPU_45GS02) set_flag(cpu, FLAG_E, 1);
        } else if (strcmp(cmd, "step") == 0) {
            const char *p = line; SKIP_CMD(p); unsigned long tmp; int steps = parse_mon_value(&p, &tmp) ? (int)tmp : 1;
            for (int i = 0; i < steps; i++) {
                mem->mem_writes = 0;
                int tr = handle_trap_local(symbols, cpu, mem); if (tr < 0) break; if (tr > 0) continue;
                unsigned char opc = mem_read(mem, cpu->pc); if (opc == 0x00) break;
                cpu_t pre = *cpu;
                execute_from_mem(cpu, mem, dt, *p_cpu_type);
                cli_hist_push(&pre, mem);
            }
            printf("STOP $%04X\n", cpu->pc);
        } else if (strcmp(cmd, "stepback") == 0 || strcmp(cmd, "sb") == 0) {
            if (cli_hist_step_back(cpu, mem)) printf("BACK $%04X\n", cpu->pc);
            else printf("No history to step back into.\n");
        } else if (strcmp(cmd, "stepfwd") == 0 || strcmp(cmd, "sf") == 0) {
            if (cli_hist_step_fwd(cpu, mem, dt, *p_cpu_type)) printf("FWD $%04X\n", cpu->pc);
            else printf("Already at the present.\n");
        } else if (strcmp(cmd, "bload") == 0) {
            /* bload "file.bin" <addr>   -- raw binary, address required
             * bload "file.prg" [addr]   -- PRG header used unless addr given */
            const char *p = line; SKIP_CMD(p);
            while (*p && isspace((unsigned char)*p)) p++;
            if (*p != '"') { printf("Usage: bload \"file\" [addr]\n"); }
            else {
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
            /* bsave "file" <start> <end>  -- saves [start, end) as .bin or .prg */
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
        } else if (strcmp(cmd, "vic2.info") == 0) {
            vic2_print_info(mem);
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
            if (vic2_render_ppm(mem, fbuf) == 0)
                printf("Saved %dx%d PPM to '%s'\n", VIC2_FRAME_W, VIC2_FRAME_H, fbuf);
            else
                printf("Error: cannot write '%s'\n", fbuf);
        }
#undef SKIP_CMD
    }
}

void print_help(const char *progname) {
	printf("6502 Simulator\nUsage: %s [options] <file.asm>\n\n", progname);
	printf("Options:\n  -p <CPU>  Select processor: 6502, 65c02, 65ce02, 45gs02\n  -I        Interactive mode\n  -l        List processors\n  -b <ADDR> Set breakpoint\n");
}

void list_processors(void) {
	printf("Available Processors: 6502, 65c02, 65ce02, 45gs02\n");
}

void list_opcodes(cpu_type_t type) {
	(void)type; printf("Opcode listing not implemented in CLI helpers yet.\n");
}

void print_opcode_info(opcode_handler_t *handlers, int num_handlers, const char *mnemonic) {
	(void)handlers; (void)num_handlers; (void)mnemonic; printf("Opcode info not implemented in CLI helpers yet.\n");
}
