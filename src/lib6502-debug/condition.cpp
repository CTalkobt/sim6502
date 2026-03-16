#include "condition.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

int parse_mon_value(const char **pp, unsigned long *out) {
    const char *p = *pp;
    while (*p && isspace((unsigned char)*p)) p++;
    if (!*p) return 0;
    char *end;
    unsigned long val;
    if (*p == '$') {
        if (!isxdigit((unsigned char)p[1])) return 0;
        val = strtoul(p + 1, &end, 16);
    } else if (*p == '0' && (p[1] == 'x' || p[1] == 'X')) {
        val = strtoul(p, &end, 16);
    } else if (*p == '%') {
        if (p[1] != '0' && p[1] != '1') return 0;
        val = strtoul(p + 1, &end, 2);
    } else if (*p == '#') {
        if (!isdigit((unsigned char)p[1])) return 0;
        val = strtoul(p + 1, &end, 10);
    } else if (isxdigit((unsigned char)*p)) {
        // Default to hex for monitor compatibility if it starts with a hex digit.
        // For conditions, this still allows registers like A-F if they are not
        // handled as numbers first (see next_tok).
        val = strtoul(p, &end, 16);
    } else {
        return 0;
    }
    *pp = end;
    *out = val;
    return 1;
}

unsigned long get_reg_val(const char *name, cpu_t *cpu) {
    if      (strcasecmp(name, "A")  == 0) return cpu->a;
    else if (strcasecmp(name, "X")  == 0) return cpu->x;
    else if (strcasecmp(name, "Y")  == 0) return cpu->y;
    else if (strcasecmp(name, "Z")  == 0) return cpu->z;
    else if (strcasecmp(name, "B")  == 0) return cpu->b;
    else if (strcasecmp(name, "S")  == 0 || strcasecmp(name, "SP") == 0) return cpu->s;
    else if (strcasecmp(name, "P")  == 0) return cpu->p;
    else if (strcasecmp(name, "PC") == 0) return cpu->pc;
    else if (strcasecmp(name, ".C") == 0) return (cpu->p & FLAG_C) ? 1 : 0;
    else if (strcasecmp(name, ".Z") == 0) return (cpu->p & FLAG_Z) ? 1 : 0;
    else if (strcasecmp(name, ".I") == 0) return (cpu->p & FLAG_I) ? 1 : 0;
    else if (strcasecmp(name, ".D") == 0) return (cpu->p & FLAG_D) ? 1 : 0;
    else if (strcasecmp(name, ".B") == 0) return (cpu->p & FLAG_B) ? 1 : 0;
    else if (strcasecmp(name, ".V") == 0) return (cpu->p & FLAG_V) ? 1 : 0;
    else if (strcasecmp(name, ".N") == 0) return (cpu->p & FLAG_N) ? 1 : 0;
    return 0;
}

typedef enum {
    TOK_END, TOK_NUM, TOK_REG, TOK_OP, TOK_LPAREN, TOK_RPAREN
} tok_type_t;

typedef struct {
    tok_type_t type;
    unsigned long val;
    char op[4];
} token_t;

static const char *s_p;
static token_t s_tok;

static void next_tok(cpu_t *cpu) {
    while (*s_p && isspace((unsigned char)*s_p)) s_p++;
    if (!*s_p) { s_tok.type = TOK_END; return; }
    
    if (*s_p == '(') { s_tok.type = TOK_LPAREN; s_p++; return; }
    if (*s_p == ')') { s_tok.type = TOK_RPAREN; s_p++; return; }
    if (*s_p == '~') { s_tok.type = TOK_OP; s_tok.op[0] = '~'; s_tok.op[1] = '\0'; s_p++; return; }
    
    if (*s_p == '$' || *s_p == '%' || *s_p == '#' || isdigit((unsigned char)*s_p)) {
        s_tok.type = TOK_NUM;
        parse_mon_value(&s_p, &s_tok.val);
        return;
    }
    
    if (*s_p == '.' || isalpha((unsigned char)*s_p)) {
        char name[32];
        int i = 0;
        if (*s_p == '.') name[i++] = *s_p++;
        while (isalnum((unsigned char)*s_p) && i < 31) name[i++] = *s_p++;
        name[i] = '\0';
        s_tok.type = TOK_REG;
        s_tok.val = get_reg_val(name, cpu);
        return;
    }
    
    s_tok.type = TOK_OP;
    int i = 0;
    while (*s_p && strchr("=!<>|&^", *s_p) && i < 3) s_tok.op[i++] = *s_p++;
    s_tok.op[i] = '\0';
}

static unsigned long eval_or(cpu_t *cpu);

static unsigned long eval_primary(cpu_t *cpu) {
    unsigned long val = 0;
    if (s_tok.type == TOK_OP && s_tok.op[0] == '~') {
        next_tok(cpu);
        val = ~eval_primary(cpu);
    } else if (s_tok.type == TOK_NUM || s_tok.type == TOK_REG) {
        val = s_tok.val;
        next_tok(cpu);
    } else if (s_tok.type == TOK_LPAREN) {
        next_tok(cpu);
        val = eval_or(cpu);
        if (s_tok.type == TOK_RPAREN) next_tok(cpu);
    }
    return val;
}

static unsigned long eval_shift(cpu_t *cpu) {
    unsigned long val = eval_primary(cpu);
    while (s_tok.type == TOK_OP && (strcmp(s_tok.op, "<<") == 0 || strcmp(s_tok.op, ">>") == 0)) {
        char op[4]; strcpy(op, s_tok.op);
        next_tok(cpu);
        unsigned long rhs = eval_primary(cpu);
        if (strcmp(op, "<<") == 0) val <<= rhs;
        else val >>= rhs;
    }
    return val;
}

static unsigned long eval_bit_and(cpu_t *cpu) {
    unsigned long val = eval_shift(cpu);
    while (s_tok.type == TOK_OP && strcmp(s_tok.op, "&") == 0) {
        next_tok(cpu);
        val &= eval_shift(cpu);
    }
    return val;
}

static unsigned long eval_bit_xor(cpu_t *cpu) {
    unsigned long val = eval_bit_and(cpu);
    while (s_tok.type == TOK_OP && strcmp(s_tok.op, "^") == 0) {
        next_tok(cpu);
        val ^= eval_bit_and(cpu);
    }
    return val;
}

static unsigned long eval_bit_or(cpu_t *cpu) {
    unsigned long val = eval_bit_xor(cpu);
    while (s_tok.type == TOK_OP && strcmp(s_tok.op, "|") == 0) {
        next_tok(cpu);
        val |= eval_bit_xor(cpu);
    }
    return val;
}

static unsigned long eval_comp(cpu_t *cpu) {
    unsigned long val = eval_bit_or(cpu);
    while (s_tok.type == TOK_OP && (strcmp(s_tok.op, "==") == 0 || strcmp(s_tok.op, "!=") == 0 ||
                                   strcmp(s_tok.op, "<")  == 0 || strcmp(s_tok.op, ">")  == 0 ||
                                   strcmp(s_tok.op, "<=") == 0 || strcmp(s_tok.op, ">=") == 0)) {
        char op[4]; strcpy(op, s_tok.op);
        next_tok(cpu);
        unsigned long rhs = eval_bit_or(cpu);
        if      (strcmp(op, "==") == 0) val = (val == rhs);
        else if (strcmp(op, "!=") == 0) val = (val != rhs);
        else if (strcmp(op, "<")  == 0) val = (val < rhs);
        else if (strcmp(op, ">")  == 0) val = (val > rhs);
        else if (strcmp(op, "<=") == 0) val = (val <= rhs);
        else if (strcmp(op, ">=") == 0) val = (val >= rhs);
    }
    return val;
}

static unsigned long eval_and(cpu_t *cpu) {
    unsigned long val = eval_comp(cpu);
    while (s_tok.type == TOK_OP && strcmp(s_tok.op, "&&") == 0) {
        next_tok(cpu);
        unsigned long rhs = eval_comp(cpu);
        val = (val && rhs);
    }
    return val;
}

static unsigned long eval_or(cpu_t *cpu) {
    unsigned long val = eval_and(cpu);
    while (s_tok.type == TOK_OP && strcmp(s_tok.op, "||") == 0) {
        next_tok(cpu);
        unsigned long rhs = eval_and(cpu);
        val = (val || rhs);
    }
    return val;
}

int evaluate_condition(const char *cond, cpu_t *cpu) {
    if (!cond || *cond == '\0') return 1;
    s_p = cond;
    next_tok(cpu);
    return (int)eval_or(cpu);
}
