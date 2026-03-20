#ifndef LIST_PARSER_H
#define LIST_PARSER_H

#include <stdbool.h>
#include <stdint.h>
#include "symbols.h"

#define MAX_SOURCE_LINES 65536
#define MAX_SOURCE_PATH 256

typedef struct {
    char source_path[MAX_SOURCE_PATH];
    int  line_number;
    uint32_t address;
} source_line_t;

typedef struct {
    source_line_t lines[MAX_SOURCE_LINES];
    int count;
} source_map_t;

#ifdef __cplusplus
extern "C" {
#endif

void source_map_init(source_map_t *sm);
bool source_map_load_acme_list(source_map_t *sm, symbol_table_t *st, const char *filename);
bool source_map_load_kickass_dbg(source_map_t *sm, const char *filename);
bool source_map_lookup_addr(const source_map_t *sm, uint32_t addr, char *path, int *line);
bool source_map_lookup_line(const source_map_t *sm, const char *path, int line, uint32_t *addr);

#ifdef __cplusplus
}
#endif

#endif
