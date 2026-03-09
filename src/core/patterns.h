#ifndef PATTERNS_H
#define PATTERNS_H

/* -------------------------------------------------------------------------
 * Snippet Library — common 6502/65c02/45gs02 idioms
 * ------------------------------------------------------------------------- */

typedef struct {
    const char *name;       /* identifier, e.g. "add16"          */
    const char *category;   /* "math", "memory", "io", "time"    */
    const char *summary;    /* one-line description               */
    const char *processor;  /* "6502", "65c02", "45gs02"         */
    const char *body;       /* full documented assembly source    */
} snippet_t;

extern const snippet_t   g_snippets[];
extern const int         g_snippet_count;

const snippet_t *snippet_find(const char *name);

#endif /* PATTERNS_H */
