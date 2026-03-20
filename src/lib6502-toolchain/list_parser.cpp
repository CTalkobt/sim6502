#include "list_parser.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

void source_map_init(source_map_t *sm) {
    sm->count = 0;
}

static char *trim_whitespace(char *str) {
    while (isspace((unsigned char)*str)) str++;
    if (*str == 0) return str;
    char *end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    end[1] = 0;
    return str;
}

bool source_map_load_acme_list(source_map_t *sm, symbol_table_t *st, const char *filename) {
    FILE *f = fopen(filename, "r");
    if (!f) return false;

    char line[512];
    char source_path[MAX_SOURCE_PATH] = "";
    
    /* ACME list format often includes the source file being processed. */
    while (fgets(line, sizeof(line), f)) {
        /* ACME format: 
         * [line] [addr] [bytes] [source]
         * example: 
         *    2  0000  a9 01       lda #$01
         */
        
        /* Skip empty lines */
        if (strlen(line) < 10) continue;
        
        int line_num;
        unsigned int addr;
        char part[256];
        
        /* ACME list often starts with spaces */
        if (sscanf(line, "%d %x", &line_num, &addr) == 2) {
            /* We found a line with address. */
            if (sm->count < MAX_SOURCE_LINES) {
                sm->lines[sm->count].line_number = line_num;
                sm->lines[sm->count].address = addr;
                strncpy(sm->lines[sm->count].source_path, source_path, MAX_SOURCE_PATH - 1);
                sm->count++;
            }
            
            /* Check for labels or metadata in the source part of the line */
            /* The source part starts after about column 16-20 in ACME lists */
            char *comment = strchr(line, ';');
            if (comment) {
                comment++;
                comment = trim_whitespace(comment);
                
                if (strncmp(comment, "@inspect", 8) == 0) {
                    char name[64];
                    snprintf(name, sizeof(name), "inspect_%04X", addr);
                    symbol_add(st, name, addr, SYM_INSPECT, comment + 8);
                } else if (strncmp(comment, "@trap", 5) == 0) {
                    char name[64];
                    snprintf(name, sizeof(name), "trap_%04X", addr);
                    symbol_add(st, name, addr, SYM_TRAP, comment + 5);
                }
            }
        }
    }

    fclose(f);
    return true;
}

/*
 * Parse a KickAssembler C64debugger XML dump (generated with -debugdump).
 * Format excerpt:
 *   <Sources values="INDEX,FILE">
 *       0,KickAss.jar:/include/autoinclude.asm
 *       1,/path/to/source.asm
 *   </Sources>
 *   <Segment ...>
 *     <Block ...>
 *       $START,$END,FILE_IDX,LINE1,COL1,LINE2,COL2
 *     </Block>
 *   </Segment>
 *
 * We record one entry per block line using (START, FILE_IDX→path, LINE1).
 * Paths ending in ".asm.tmp" are rewritten to ".asm" so preprocessed sources
 * resolve back to the user-visible file.
 */
bool source_map_load_kickass_dbg(source_map_t *sm, const char *filename) {
    FILE *f = fopen(filename, "r");
    if (!f) return false;

#define KA_MAX_SOURCES 64
    char sources[KA_MAX_SOURCES][MAX_SOURCE_PATH];
    int  num_sources = 0;
    memset(sources, 0, sizeof(sources));

    char line[1024];
    bool in_sources = false;
    bool in_block   = false;

    while (fgets(line, sizeof(line), f)) {
        /* Trim leading whitespace for simpler comparisons */
        char *p = line;
        while (isspace((unsigned char)*p)) p++;

        if (strstr(p, "<Sources")) { in_sources = true;  continue; }
        if (strstr(p, "</Sources")){ in_sources = false; continue; }

        if (in_sources) {
            int idx;
            char path[MAX_SOURCE_PATH];
            if (sscanf(p, "%d,%255s", &idx, path) == 2) {
                if (idx >= 0 && idx < KA_MAX_SOURCES) {
                    /* Rewrite .asm.tmp → .asm (pseudoop preprocessing artifact) */
                    int plen = (int)strlen(path);
                    if (plen > 8 && strcmp(path + plen - 8, ".asm.tmp") == 0)
                        path[plen - 4] = '\0'; /* strip ".tmp" */
                    strncpy(sources[idx], path, MAX_SOURCE_PATH - 1);
                    if (idx + 1 > num_sources) num_sources = idx + 1;
                }
            }
            continue;
        }

        if (strstr(p, "<Block"))  { in_block = true;  continue; }
        if (strstr(p, "</Block")) { in_block = false; continue; }

        if (in_block) {
            unsigned int start_addr, end_addr;
            int file_idx, line1, col1, line2, col2;
            if (sscanf(p, "$%x,$%x,%d,%d,%d,%d,%d",
                       &start_addr, &end_addr,
                       &file_idx, &line1, &col1, &line2, &col2) == 7) {
                if (sm->count < MAX_SOURCE_LINES &&
                    file_idx >= 0 && file_idx < num_sources &&
                    sources[file_idx][0] != '\0') {
                    sm->lines[sm->count].address     = start_addr;
                    sm->lines[sm->count].line_number = line1;
                    strncpy(sm->lines[sm->count].source_path,
                            sources[file_idx], MAX_SOURCE_PATH - 1);
                    sm->count++;
                }
            }
        }
    }

    fclose(f);
    return sm->count > 0;
#undef KA_MAX_SOURCES
}

bool source_map_lookup_addr(const source_map_t *sm, uint32_t addr, char *path, int *line) {
    /* Simple linear search for now, could be optimized with binary search if sm->lines is sorted by address */
    for (int i = 0; i < sm->count; i++) {
        if (sm->lines[i].address == addr) {
            if (path) strcpy(path, sm->lines[i].source_path);
            if (line) *line = sm->lines[i].line_number;
            return true;
        }
    }
    return false;
}

bool source_map_lookup_line(const source_map_t *sm, const char *path, int line, uint32_t *addr) {
    for (int i = 0; i < sm->count; i++) {
        if (sm->lines[i].line_number == line) {
            /* If path is specified, match it too */
            if (path && sm->lines[i].source_path[0] != 0) {
                if (strcmp(sm->lines[i].source_path, path) != 0) continue;
            }
            if (addr) *addr = sm->lines[i].address;
            return true;
        }
    }
    return false;
}
