#include "metadata.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

int g_verbose = 0;
#define LOG_V2(...) if (g_verbose >= 2) fprintf(stderr, __VA_ARGS__)
#define LOG_V3(...) if (g_verbose >= 3) fprintf(stderr, __VA_ARGS__)

/* Strip surrounding quotes and trailing whitespace: "SID #1"\n -> SID #1 */
static void strip_arg_quotes(const char *in, char *out, int outlen) {
    const char *p = in;
    while (*p == ' ' || *p == '\t') p++;
    const char *start = (*p == '"') ? p + 1 : p;
    strncpy(out, start, (size_t)outlen - 1);
    out[outlen - 1] = '\0';
    /* Trim trailing whitespace first */
    int len = (int)strlen(out);
    while (len > 0 && (out[len-1] == ' ' || out[len-1] == '\t' ||
                       out[len-1] == '\r' || out[len-1] == '\n'))
        out[--len] = '\0';
    /* Then remove closing quote if present */
    if (len > 0 && out[len-1] == '"') out[--len] = '\0';
}

/* Map a cpu name string (sim form: "45gs02", "65ce02", "6502-undoc", etc.) to cpu_type_t. */
static cpu_type_t cpu_type_from_name(const char *s) {
    if (!s) return CPU_6502;
    if (strcmp(s, "45gs02")    == 0) return CPU_45GS02;
    if (strcmp(s, "65ce02")    == 0) return CPU_65CE02;
    if (strcmp(s, "65c02")     == 0) return CPU_65C02;
    if (strcmp(s, "6502-undoc")== 0) return CPU_6502_UNDOCUMENTED;
    return CPU_6502;
}

/* Map a machine name string ("mega65", "c64", etc.) to machine_type_t. */
static machine_type_t machine_type_from_name(const char *s) {
    if (!s) return MACHINE_C64;
    if (strcmp(s, "mega65")  == 0) return MACHINE_MEGA65;
    if (strcmp(s, "c128")    == 0) return MACHINE_C128;
    if (strcmp(s, "x16")     == 0) return MACHINE_X16;
    if (strcmp(s, "raw6502") == 0) return MACHINE_RAW6502;
    return MACHINE_C64;
}

cpu_type_t detect_asm_cpu_type(const char *asm_path) {
    LOG_V2("DEBUG: detect_asm_cpu_type probe: %s\n", asm_path);
    FILE *f = fopen(asm_path, "r");
    if (!f) {
        LOG_V2("DEBUG: detect_asm_cpu_type: could not open %s\n", asm_path);
        return CPU_6502;
    }
    char line[256];
    cpu_type_t result = CPU_6502;
    while (fgets(line, sizeof(line), f)) {
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;

        /* Native KickAssembler directive: .cpu _45gs02 */
        if (strncasecmp(p, ".cpu", 4) == 0 && (p[4] == ' ' || p[4] == '\t')) {
            char *q = p + 4;
            while (*q == ' ' || *q == '\t') q++;
            if      (strncmp(q, "_45gs02", 7) == 0) result = CPU_45GS02;
            else if (strncmp(q, "_65ce02", 7) == 0) result = CPU_65CE02;
            else if (strncmp(q, "_65c02",  6) == 0) result = CPU_65C02;
            else if (strncmp(q, "_6502_undoc", 11) == 0) result = CPU_6502_UNDOCUMENTED;
            else if (strncmp(q, "_6502",   5) == 0) result = CPU_6502;
            continue;
        }

        /* Comment pseudo-op: //.cpu "45gs02" */
        if (p[0] == '/' && p[1] == '/') {
            char *q = p + 2;
            while (*q == ' ' || *q == '\t') q++;
            if (strncmp(q, ".cpu", 4) == 0 && (q[4] == ' ' || q[4] == '\t')) {
                char clean[64];
                strip_arg_quotes(q + 4, clean, sizeof(clean));
                result = cpu_type_from_name(clean);
                LOG_V2("DEBUG: detect_asm_cpu_type: found pseudo-op, result=%d\n", (int)result);
                break;
            }
        }
    }
    fclose(f);
    LOG_V2("DEBUG: detect_asm_cpu_type final: %d\n", (int)result);
    return result;
}

cpu_type_t cpu_type_from_symbols(const symbol_table_t *st) {
    if (!st) return CPU_6502;
    for (int i = 0; i < st->count; i++) {
        const symbol_t *sym = &st->symbols[i];
        if (sym->type == SYM_PROCESSOR && strcmp(sym->name, "sim_cpu") == 0)
            return cpu_type_from_name(sym->comment);
    }
    return CPU_6502;
}

machine_type_t machine_type_from_symbols(const symbol_table_t *st) {
    if (!st) return MACHINE_C64;
    for (int i = 0; i < st->count; i++) {
        const symbol_t *sym = &st->symbols[i];
        if (sym->type == SYM_PROCESSOR && strcmp(sym->name, "sim_machine") == 0)
            return machine_type_from_name(sym->comment);
    }
    return MACHINE_C64;
}

int load_binary(memory_t *mem, int addr, const char *filename) {
	FILE *bf = fopen(filename, "rb");
	if (!bf) {
		fprintf(stderr, "Warning: cannot open binary '%s': %s\n", filename, strerror(errno));
		return -1;
	}
	fseek(bf, 0, SEEK_END);
	long size = ftell(bf);
	rewind(bf);
	if (mem) {
		for (long i = 0; i < size; i++) {
			int c = fgetc(bf);
			if (c == EOF) { size = i; break; }
			if (addr + (int)i < 65536)
				mem_write(mem, addr + i, (unsigned char)c);
		}
	}
	fclose(bf);
	return (int)size;
}

int load_prg(memory_t *mem, const char *filename, int *out_load_addr) {
    LOG_V2("DEBUG: ENTER load_prg\n");
    LOG_V2("DEBUG: load_prg filename=%s mem=%p\n", filename ? filename : "NULL", (void*)mem);
    if (!filename) return -1;
    FILE *bf = fopen(filename, "rb");
    if (!bf) {
        fprintf(stderr, "Warning: cannot open PRG '%s': %s\n", filename, strerror(errno));
        return -1;
    }
    LOG_V2("DEBUG: PRG opened\n");

    unsigned char lo = fgetc(bf);
    unsigned char hi = fgetc(bf);
    int addr = (hi << 8) | lo;
    if (out_load_addr) *out_load_addr = addr;
    LOG_V2("DEBUG: PRG load address: $%04X\n", addr);

    fseek(bf, 0, SEEK_END);
    long size = ftell(bf) - 2;
    fseek(bf, 2, SEEK_SET);
    LOG_V2("DEBUG: PRG size: %ld\n", size);

    if (mem) {
        for (long i = 0; i < size; i++) {
            int c = fgetc(bf);
            if (c == EOF) { size = i; break; }
            if (addr + (int)i < 65536) {
                mem_write(mem, (unsigned short)(addr + i), (unsigned char)c);
            }
        }
    }
    LOG_V2("DEBUG: PRG loaded\n");
    fclose(bf);
    return (int)size;
}

/* Silently probe whether a file exists without printing warnings. */
static int file_exists(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    fclose(f);
    return 1;
}

/*
 * Preprocess a .asm source file: replace simulator pseudo-ops (.inspect, .trap)
 * with KickAssembler .print statements that embed the current PC in their output.
 * This allows recovering the assembled address for each pseudo-op by parsing
 * KickAssembler's stdout after assembly.
 *
 * Returns the number of pseudo-ops replaced, or 0 if none found (in which case
 * tmp_path is not written).
 */
/*
 * Detect a simulator pseudo-op embedded in a comment: "// .inspect ..." or "//.trap ..."
 * p must point to the start of the non-whitespace portion of the line.
 * If matched, sets *keyword to the keyword start and *kw_len to its length, returns true.
 */
/* Detect a simulator pseudo-op embedded in a comment: "//.inspect", "//.trap",
 * "//.cpu", "//.machine".  p must point to the non-whitespace start of the line.
 * Returns 1 if matched; sets *keyword and *kw_len. */
static int detect_pseudoop(char *p, const char **keyword, int *kw_len) {
    if (p[0] != '/' || p[1] != '/') return 0;
    char *q = p + 2;
    while (*q == ' ' || *q == '\t') q++;
#define MATCH_KW(str, n) (strncmp(q, str, n) == 0 && (!q[n] || q[n]==' ' || q[n]=='\t' || q[n]=='"'))
    if (MATCH_KW(".inspect", 8)) { *keyword = q; *kw_len = 8; return 1; }
    if (MATCH_KW(".trap",    5)) { *keyword = q; *kw_len = 5; return 1; }
    if (MATCH_KW(".cpu",     4)) { *keyword = q; *kw_len = 4; return 1; }
    if (MATCH_KW(".machine", 8)) { *keyword = q; *kw_len = 8; return 1; }
#undef MATCH_KW
    return 0;
}

/* Detect a native KickAssembler / ACME .cpu or .processor directive (not a comment).
 * p must point to the non-whitespace start of the line.
 * If matched, writes the sim-format cpu name (e.g. "45gs02") into cpu_out (size outlen)
 * and returns 1.  The original line should be KEPT and a SIM_CPU print injected after it. */
static int detect_native_cpu_directive(const char *p, char *cpu_out, int outlen) {
    const char *directive = NULL;
    int dlen = 0;
    if (strncasecmp(p, ".cpu",       4) == 0 && (p[4]==' '||p[4]=='\t')) { directive=p+4; dlen=4; }
    else if (strncasecmp(p, ".processor", 10) == 0 && (p[10]==' '||p[10]=='\t')) { directive=p+10; dlen=10; }
    if (!directive) return 0;
    (void)dlen;
    /* Skip whitespace after directive */
    while (*directive == ' ' || *directive == '\t') directive++;
    /* Strip leading underscore (KickAssembler uses _45gs02, ACME may not) */
    if (*directive == '_') directive++;
    /* Copy the cpu name, stopping at whitespace/newline */
    int i = 0;
    while (i < outlen - 1 && directive[i] && directive[i] != ' ' && directive[i] != '\t'
           && directive[i] != '\r' && directive[i] != '\n') {
        cpu_out[i] = directive[i];
        i++;
    }
    cpu_out[i] = '\0';
    
    /* Handle specific KickAss naming -> sim naming mapping if needed */
    if (strcmp(cpu_out, "6502_undoc") == 0) strcpy(cpu_out, "6502-undoc");

    return (i > 0) ? 1 : 0;
}

/*
 * Preprocess a .asm source file: simulator pseudo-ops embedded in comments
 * ("//.inspect ..." or "// .trap ...") are replaced with KickAssembler .print
 * statements that emit the current PC to stdout.  This lets files assemble
 * directly with KickAssembler (the comment is ignored) while our pipeline
 * recovers the assembled addresses by parsing stdout after assembly.
 *
 * Returns the number of pseudo-ops replaced, or 0 if none (tmp_path not written).
 */
static int preprocess_asm_pseudoops(const char *asm_path, const char *tmp_path) {
    FILE *in = fopen(asm_path, "r");
    if (!in) return 0;

    /* First pass: count lines that need transformation */
    int count = 0;
    char line[4096];
    while (fgets(line, sizeof(line), in)) {
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        const char *kw; int kw_len;
        if (detect_pseudoop(p, &kw, &kw_len)) count++;
    }
    if (count == 0) { fclose(in); return 0; }

    /* Second pass: rewrite */
    rewind(in);
    FILE *out = fopen(tmp_path, "w");
    if (!out) { fclose(in); return 0; }

    while (fgets(line, sizeof(line), in)) {
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        const char *kw; int kw_len;

        if (detect_pseudoop(p, &kw, &kw_len)) {
            char *arg = (char *)kw + kw_len;
            while (*arg == ' ' || *arg == '\t') arg++;
            char clean[256];
            strip_arg_quotes(arg, clean, sizeof(clean));
            /* Map keyword length to output tag:
             *   8 = .inspect → SIM_INSPECT (needs PC address)
             *   5 = .trap    → SIM_TRAP    (needs PC address)
             *   4 = .cpu     → SIM_CPU     (no address — metadata only)
             *   8 = .machine → SIM_MACHINE (no address — metadata only)
             * .inspect and .machine both have kw_len==8; disambiguate by keyword text. */
            if (strncmp(kw, ".inspect", 8) == 0) {
                fprintf(out, "    .print \"SIM_INSPECT:\"+toHexString(*)+\":%s\"\n", clean);
            } else if (strncmp(kw, ".trap", 5) == 0) {
                fprintf(out, "    .print \"SIM_TRAP:\"+toHexString(*)+\":%s\"\n", clean);
            } else if (strncmp(kw, ".cpu", 4) == 0) {
                fprintf(out, "    .print \"SIM_CPU:%s\"\n", clean);
            } else if (strncmp(kw, ".machine", 8) == 0) {
                fprintf(out, "    .print \"SIM_MACHINE:%s\"\n", clean);
            }
        } else {
            fputs(line, out);
        }
    }
    fclose(in);
    fclose(out);
    return count;
}

/*
 * Parse KickAssembler stdout (captured to a file) for SIM_INSPECT:/SIM_TRAP:
 * markers emitted by .print statements injected during preprocessing.
 * Adds matching SYM_INSPECT/SYM_TRAP symbols to the symbol table.
 */
static void apply_pseudoop_output(symbol_table_t *st, const char *output_file) {
    if (!st) return;
    FILE *f = fopen(output_file, "r");
    if (!f) return;
    char line[4096];
    while (fgets(line, sizeof(line), f)) {
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        unsigned int addr;
        char device[128];
        if (strncmp(p, "SIM_INSPECT:", 12) == 0 &&
                sscanf(p + 12, "%x:%127[^\n\r]", &addr, device) == 2) {
            char name[64];
            snprintf(name, sizeof(name), "inspect_%04X", addr);
            symbol_add(st, name, (unsigned short)addr, SYM_INSPECT, device);
        } else if (strncmp(p, "SIM_TRAP:", 9) == 0 &&
                   sscanf(p + 9, "%x:%127[^\n\r]", &addr, device) == 2) {
            char name[64];
            snprintf(name, sizeof(name), "trap_%04X", addr);
            symbol_add(st, name, (unsigned short)addr, SYM_TRAP, device);
        } else if (strncmp(p, "SIM_CPU:", 8) == 0) {
            char type[32];
            if (sscanf(p + 8, "%31[^\n\r]", type) == 1)
                symbol_add(st, "sim_cpu", 0, SYM_PROCESSOR, type);
        } else if (strncmp(p, "SIM_MACHINE:", 12) == 0) {
            char type[32];
            if (sscanf(p + 12, "%31[^\n\r]", type) == 1)
                symbol_add(st, "sim_machine", 0, SYM_PROCESSOR, type);
        }
    }
    fclose(f);
}

bool load_toolchain_bundle(memory_t *mem, symbol_table_t *st, source_map_t *sm, const char *base_path, int *out_load_addr, char *err_msg, int err_sz) {
    LOG_V2("DEBUG: ENTER load_toolchain_bundle mem=%p base=%s\n", (void*)mem, base_path);
    char path[512];

    if (err_msg && err_sz > 0) err_msg[0] = '\0';

    int load_addr = 0x0801;
    int n = -1;

    char asm_path[512], prg_path[512], dbg_path[512];
    snprintf(asm_path, sizeof(asm_path), "%s.asm", base_path);
    snprintf(prg_path, sizeof(prg_path), "%s.prg", base_path);
    snprintf(dbg_path, sizeof(dbg_path), "%s.dbg", base_path);

    /* Try .prg first (silently — not finding it is expected when only .asm exists) */
    LOG_V2("DEBUG: Attempting to load PRG: %s\n", prg_path);
    if (file_exists(prg_path)) {
        n = load_prg(mem, prg_path, &load_addr);
        LOG_V2("DEBUG: load_prg returned %d\n", n);
    }
    if (out_load_addr) *out_load_addr = load_addr;

    if (n < 0) {
        /* Try .bin */
        snprintf(path, sizeof(path), "%s.bin", base_path);
        if (file_exists(path))
            n = load_binary(mem, 0x0801, path);
    }

    /* Assemble when: (a) no binary was found yet, OR (b) .asm exists but .dbg is
     * missing (debug info needed for source-level navigation even if .prg is cached).
     * The jar is resolved relative to CWD (project root when running sim6502/sim6502-gui).
     *
     * Simulator pseudo-ops (.inspect, .trap) are preprocessed: each is replaced with a
     * KickAssembler .print statement that emits the current PC to stdout.  After assembly
     * we parse that output to register the pseudo-op addresses as SYM_INSPECT/SYM_TRAP
     * symbols, without KickAssembler ever needing to understand them. */
    bool need_assemble = (n < 0) || (file_exists(asm_path) && !file_exists(dbg_path));

    if (need_assemble && file_exists(asm_path)) {
        /* Preprocess for simulator pseudo-ops */
        char tmp_asm[512], tmp_out[512], tmp_sym[512];
        snprintf(tmp_asm, sizeof(tmp_asm), "%s.asm.tmp",     base_path);
        snprintf(tmp_out, sizeof(tmp_out), "%s.asm.tmp.out", base_path);
        /* KickAssembler derives the .sym name by stripping the last extension
         * from its input path, so "base.asm.tmp" → sym at "base.asm.sym". */
        snprintf(tmp_sym, sizeof(tmp_sym), "%s.asm.sym", base_path);

        int has_pseudoops = preprocess_asm_pseudoops(asm_path, tmp_asm);
        const char *src = has_pseudoops ? tmp_asm : asm_path;

        char cmd[2048];
        snprintf(cmd, sizeof(cmd),
                 "java -jar tools/KickAss65CE02.jar \"%s\" -symbolfile -debugdump -o \"%s\" > \"%s\" 2>&1",
                 src, prg_path, tmp_out);
        LOG_V2("DEBUG: Invoking assembler: %s\n", cmd);
        int rc = system(cmd);

        if (rc == 0 && file_exists(prg_path)) {
            if (n < 0) {
                n = load_prg(mem, prg_path, &load_addr);
                if (out_load_addr) *out_load_addr = load_addr;
            }
            if (has_pseudoops && st) apply_pseudoop_output(st, tmp_out);
            /* KickAssembler writes .sym next to its input file.  When using a temp
             * file rename the result so normal .sym loading below finds it. */
            if (has_pseudoops && file_exists(tmp_sym)) {
                char sym_dest[512];
                snprintf(sym_dest, sizeof(sym_dest), "%s.sym", base_path);
                rename(tmp_sym, sym_dest);
            }
        } else if (n < 0) {
            /* Assembly failed and we have no pre-loaded binary to fall back on. */
            fprintf(stderr, "Error: assembly of '%s' failed (rc=%d)\n", asm_path, rc);
            FILE *ef = fopen(tmp_out, "r");
            if (ef) {
                char ebuf[512];
                while (fgets(ebuf, sizeof(ebuf), ef)) {
                    fprintf(stderr, "  %s", ebuf);
                    if (err_msg && err_sz > 0) {
                        int len = (int)strlen(err_msg);
                        if (len + (int)strlen(ebuf) < err_sz - 1)
                            strcat(err_msg, ebuf);
                    }
                }
                fclose(ef);
            }
        }
        if (has_pseudoops) remove(tmp_asm);
        remove(tmp_out);
    }

    if (n < 0) return false;

    /* Detect CPU / machine type directly from the .asm source (avoids the need to
     * inject extra .print lines into the preprocessed temp file, which would shift
     * .dbg line numbers).  We only need the FIRST .cpu/.processor directive. */
    if (st && file_exists(asm_path)) {
        FILE *af = fopen(asm_path, "r");
        if (af) {
            char aline[512];
            char cpu_name[32];
            while (fgets(aline, sizeof(aline), af)) {
                char *p = aline;
                while (*p == ' ' || *p == '\t') p++;
                if (detect_native_cpu_directive(p, cpu_name, sizeof(cpu_name))) {
                    symbol_add(st, "sim_cpu", 0, SYM_PROCESSOR, cpu_name);
                    break;
                }
            }
            fclose(af);
        }
    }

    load_companion_files(st, sm, base_path);
    return true;
}

void load_companion_files(symbol_table_t *st, source_map_t *sm, const char *base_path) {
    char path[512];

    /* Try .dbg (KickAssembler C64debugger debug dump - address/line/file map) */
    snprintf(path, sizeof(path), "%s.dbg", base_path);
    if (sm) source_map_load_kickass_dbg(sm, path);

    /* Try .list (ACME-format source map) */
    snprintf(path, sizeof(path), "%s.list", base_path);
    if (sm) source_map_load_acme_list(sm, st, path);

    /* Try .sym (KickAssembler symbol file) */
    snprintf(path, sizeof(path), "%s.sym", base_path);
    if (st) symbol_load_file(st, path);

    /* Try .sym_add — supplemental annotations file that accepts both KickAssembler
     * symbol format (.label name=$addr) and simulator pseudo-op markers
     * (SIM_INSPECT:<hex>:<name>, SIM_TRAP:<hex>:<name>).  Both parsers run on the
     * same file; each handles the lines it understands and ignores the rest.
     * Useful for .prg/.bin files built by any assembler that need .inspect/.trap
     * annotations.  Produce one by redirecting KickAssembler stdout:
     *   java -jar tools/KickAss65CE02.jar prog.asm -symbolfile -o prog.prg > prog.sym_add */
    snprintf(path, sizeof(path), "%s.sym_add", base_path);
    if (st && file_exists(path)) {
        symbol_load_file(st, path);        /* picks up .label name=$addr entries */
        apply_pseudoop_output(st, path);   /* picks up SIM_INSPECT/SIM_TRAP lines */
    }
}
