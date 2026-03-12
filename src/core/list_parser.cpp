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
