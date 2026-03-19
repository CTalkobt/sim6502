#ifndef SYMBOL_TABLE_H
#define SYMBOL_TABLE_H

#include <stdbool.h>

#define MAX_SYMBOLS 2048
#define MAX_SYMBOL_NAME 64
#define MAX_SYMBOL_COMMENT 128

typedef enum {
	SYM_LABEL = 0,
	SYM_VARIABLE = 1,
	SYM_CONSTANT = 2,
	SYM_FUNCTION = 3,
	SYM_IO_PORT = 4,
	SYM_MEMORY_REGION = 5,
	SYM_TRAP = 6,		/* Intercept JSR/JMP/RTS/RTN to this address; dump state and simulate RTS */
	SYM_INSPECT = 7,	/* Print device/memory state when PC hits this address */
	SYM_PROCESSOR = 8	/* Metadata: cpu/machine type detected from source (.cpu / .machine directives).
	                       address=0, name="sim_cpu" or "sim_machine", comment=type string */
} symbol_type_t;

typedef struct {
	char name[MAX_SYMBOL_NAME];
	unsigned short address;
	symbol_type_t type;
	char comment[MAX_SYMBOL_COMMENT];
	int size;  /* For regions, size in bytes */
} symbol_t;

typedef struct {
	symbol_t symbols[MAX_SYMBOLS];
	int count;
	char arch_name[64];
} symbol_table_t;

#ifdef __cplusplus
extern "C" {
#endif

/* Initialize symbol table */
void symbol_table_init(symbol_table_t *st, const char *arch);

/* Add symbol to table */
int symbol_add(symbol_table_t *st, const char *name, 
	unsigned short addr, symbol_type_t type, const char *comment);

/* Look up symbol by name */
int symbol_lookup_name(const symbol_table_t *st, const char *name, 
	unsigned short *addr);

/* Look up symbol by address */
int symbol_lookup_addr(const symbol_table_t *st, unsigned short addr,
	char *name);

/* Look up symbol by address and return name pointer */
const char *symbol_lookup_addr_name(const symbol_table_t *st, unsigned short addr);

/* Get symbol info */
symbol_t *symbol_get(const symbol_table_t *st, const char *name);

/* Remove symbol by index */
int symbol_remove_idx(symbol_table_t *st, int idx);

/* Rename symbol at index */
int symbol_rename(symbol_table_t *st, int idx, const char *new_name);

/* Set address for symbol at index */
int symbol_set_addr(symbol_table_t *st, int idx, unsigned short addr);

/* Display symbol table */
void symbol_display(const symbol_table_t *st);

/* Load symbol table from file */
int symbol_load_file(symbol_table_t *st, const char *filename);

/* Save symbol table to file */
int symbol_save_file(const symbol_table_t *st, const char *filename);

#ifdef __cplusplus
}
#endif

#endif
