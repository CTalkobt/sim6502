#include "sim_api.h"
#include "symbols.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Initialize symbol table */
void symbol_table_init(symbol_table_t *st, const char *arch) {
	st->count = 0;
	strncpy(st->arch_name, arch, sizeof(st->arch_name) - 1);
	st->arch_name[sizeof(st->arch_name) - 1] = 0;
}

/* Add symbol to table */
int symbol_add(symbol_table_t *st, const char *name, 
	unsigned short addr, symbol_type_t type, const char *comment) {
	
	if (st->count >= MAX_SYMBOLS) {
		return 0;
	}
	
	/* Check for duplicate */
	for (int i = 0; i < st->count; i++) {
		if (strcmp(st->symbols[i].name, name) == 0) {
			return 0;  /* Duplicate */
		}
	}
	
	strncpy(st->symbols[st->count].name, name, MAX_SYMBOL_NAME);
	st->symbols[st->count].name[MAX_SYMBOL_NAME - 1] = 0;
	st->symbols[st->count].address = addr;
	st->symbols[st->count].type = type;
	st->symbols[st->count].size = 1;
	
	if (comment) {
		strncpy(st->symbols[st->count].comment, comment, MAX_SYMBOL_COMMENT);
		st->symbols[st->count].comment[MAX_SYMBOL_COMMENT - 1] = 0;
	} else {
		st->symbols[st->count].comment[0] = 0;
	}
	
	st->count++;
	return 1;
}

/* Look up symbol by name */
int symbol_lookup_name(const symbol_table_t *st, const char *name, 
	unsigned short *addr) {
	
	for (int i = 0; i < st->count; i++) {
		if (strcmp(st->symbols[i].name, name) == 0) {
			*addr = st->symbols[i].address;
			return 1;
		}
	}
	return 0;
}

/* Look up symbol by address */
int symbol_lookup_addr(const symbol_table_t *st, unsigned short addr,
	char *name) {
	
	for (int i = 0; i < st->count; i++) {
		if (st->symbols[i].address == addr) {
			strcpy(name, st->symbols[i].name);
			return 1;
		}
	}
	return 0;
}

/* Look up symbol by address and return name pointer */
const char *symbol_lookup_addr_name(const symbol_table_t *st, unsigned short addr) {
	for (int i = 0; i < st->count; i++) {
		if (st->symbols[i].address == addr) {
			return st->symbols[i].name;
		}
	}
	return NULL;
}

/* Get symbol info */
symbol_t *symbol_get(const symbol_table_t *st, const char *name) {
	for (int i = 0; i < st->count; i++) {
		if (strcmp(st->symbols[i].name, name) == 0) {
			return (symbol_t *)&st->symbols[i];
		}
	}
	return NULL;
}

/* Remove symbol by index */
int symbol_remove_idx(symbol_table_t *st, int idx) {
	if (idx < 0 || idx >= st->count) return 0;
	for (int i = idx; i < st->count - 1; i++) {
		st->symbols[i] = st->symbols[i + 1];
	}
	st->count--;
	return 1;
}

/* Rename symbol at index */
int symbol_rename(symbol_table_t *st, int idx, const char *new_name) {
	if (idx < 0 || idx >= st->count) return 0;
	if (!new_name || !new_name[0]) return 0;
	/* Check for duplicate name */
	for (int i = 0; i < st->count; i++) {
		if (i != idx && strcmp(st->symbols[i].name, new_name) == 0) return 0;
	}
	strncpy(st->symbols[idx].name, new_name, MAX_SYMBOL_NAME);
	st->symbols[idx].name[MAX_SYMBOL_NAME - 1] = 0;
	return 1;
}

/* Set address for symbol at index */
int symbol_set_addr(symbol_table_t *st, int idx, unsigned short addr) {
	if (idx < 0 || idx >= st->count) return 0;
	st->symbols[idx].address = addr;
	return 1;
}

/* Display symbol table */
void symbol_display(const symbol_table_t *st) {
	cli_printf("\n╔════════════════════════════════════════════════════════╗\n");
	cli_printf("║  Symbol Table: %-40s║\n", st->arch_name);
	cli_printf("╠════════════════════════════════════════════════════════╣\n");
	cli_printf("║ Address  | Name                | Type          | Comment       ║\n");
	cli_printf("╠══════════╪═════════════════════╪═══════════════╪═══════════════╣\n");
	
	for (int i = 0; i < st->count; i++) {
		const char *type_str;
		switch (st->symbols[i].type) {
		case SYM_LABEL: type_str = "Label"; break;
		case SYM_VARIABLE: type_str = "Variable"; break;
		case SYM_CONSTANT: type_str = "Constant"; break;
		case SYM_FUNCTION: type_str = "Function"; break;
		case SYM_IO_PORT: type_str = "I/O Port"; break;
		case SYM_MEMORY_REGION: type_str = "Region"; break;
		case SYM_TRAP: type_str = "Trap"; break;
		case SYM_INSPECT: type_str = "Inspect"; break;
		default: type_str = "Unknown"; break;
		}
		
		cli_printf("║ $%04X    │ %-19s │ %-13s │ %-13s ║\n",
			st->symbols[i].address,
			st->symbols[i].name,
			type_str,
			st->symbols[i].comment);
	}
	
	cli_printf("╠════════════════════════════════════════════════════════╣\n");
	cli_printf("║ Total Symbols: %-46d║\n", st->count);
	cli_printf("╚════════════════════════════════════════════════════════╝\n");
}

/* Load symbol table from file */
int symbol_load_file(symbol_table_t *st, const char *filename) {
	FILE *f = fopen(filename, "r");
	if (!f) {
		return 0;
	}
	
	char line[256];
	while (fgets(line, sizeof(line), f)) {
		/* Skip comments and empty lines */
		if (line[0] == ';' || line[0] == '#' || line[0] == '\n' || (line[0] == '/' && line[1] == '/')) {
			continue;
		}
		
		unsigned short addr = 0;
		char name[MAX_SYMBOL_NAME];
		char type_str[32] = "LABEL";
		char comment[MAX_SYMBOL_COMMENT] = "";
		int parsed = 0;

		/* Try KickAssembler format: .label name=$addr */
		if (strncmp(line, ".label ", 7) == 0) {
			char *eq = strchr(line, '=');
			if (eq) {
				*eq = ' ';
				char *dollar = strchr(eq + 1, '$');
				if (dollar) *dollar = ' ';
				parsed = sscanf(line + 7, "%63s %hx", name, &addr);
				if (parsed == 2) {
					symbol_add(st, name, addr, SYM_LABEL, "");
					continue;
				}
			}
		}

		/* Parse standard format: ADDRESS NAME [TYPE] [COMMENT] */
		parsed = sscanf(line, "%hx %63s %31s %127[^\n]", &addr, name, type_str, comment);
		if (parsed < 2) {
			continue;
		}
		
		symbol_type_t type = SYM_LABEL;
		if (strcmp(type_str, "VAR") == 0 || strcmp(type_str, "VARIABLE") == 0) {
			type = SYM_VARIABLE;
		} else if (strcmp(type_str, "CONST") == 0 || strcmp(type_str, "CONSTANT") == 0) {
			type = SYM_CONSTANT;
		} else if (strcmp(type_str, "FUNC") == 0 || strcmp(type_str, "FUNCTION") == 0) {
			type = SYM_FUNCTION;
		} else if (strcmp(type_str, "IO") == 0 || strcmp(type_str, "PORT") == 0) {
			type = SYM_IO_PORT;
		} else if (strcmp(type_str, "REGION") == 0 || strcmp(type_str, "MEMORY") == 0) {
			type = SYM_MEMORY_REGION;
		} else if (strcmp(type_str, "TRAP") == 0) {
			type = SYM_TRAP;
		} else if (strcmp(type_str, "INSPECT") == 0) {
			type = SYM_INSPECT;
		}
		
		symbol_add(st, name, addr, type, comment);
	}
	
	fclose(f);
	return 1;
}

/* Save symbol table to file */
int symbol_save_file(const symbol_table_t *st, const char *filename) {
	FILE *f = fopen(filename, "w");
	if (!f) {
		return 0;
	}
	
	fprintf(f, "; Symbol Table for %s\n", st->arch_name);
	fprintf(f, "; Format: ADDRESS NAME [TYPE] [COMMENT]\n");
	fprintf(f, "; Types: LABEL, VAR, CONST, FUNC, IO, REGION\n\n");
	
	for (int i = 0; i < st->count; i++) {
		const char *type_str;
		switch (st->symbols[i].type) {
		case SYM_LABEL: type_str = "LABEL"; break;
		case SYM_VARIABLE: type_str = "VAR"; break;
		case SYM_CONSTANT: type_str = "CONST"; break;
		case SYM_FUNCTION: type_str = "FUNC"; break;
		case SYM_IO_PORT: type_str = "IO"; break;
		case SYM_MEMORY_REGION: type_str = "REGION"; break;
		case SYM_TRAP: type_str = "TRAP"; break;
		case SYM_INSPECT: type_str = "INSPECT"; break;
		default: type_str = "LABEL"; break;
		}
		
		fprintf(f, "%04X %s %s %s\n",
			st->symbols[i].address,
			st->symbols[i].name,
			type_str,
			st->symbols[i].comment);
	}
	
	fclose(f);
	return 1;
}
