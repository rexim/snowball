#include <stdlib.h> /* for free */
#include <stdio.h>  /* for printf */
#include <assert.h> /* for assert */
#include <string.h> /* for strcmp */
#include "header.h"

struct system_word {
    int s_size;      /* size of system word */
    const byte * s;  /* pointer to the system word */
    int code;        /* its internal code */
};

#include "syswords.h"

static struct name *find_stem(struct generator * g)
{
    struct name * q;
    for (q = g->analyser->names; q; q = q->next) {
        if (q->type == t_external) {
            char *s = b_to_s(q->b);
            int is_stem = strcmp(s, "stem") == 0;
            free(s);
            if (is_stem) return q;
        }
    }
    return NULL;
}

static const unsigned char *printable_type_of_node(int type)
{
    size_t vocab_len = sizeof(vocab)/sizeof(vocab[0]);
    for (size_t i = 0; i < vocab_len; ++i) {
        if (vocab[i].code == type) {
            return vocab[i].s;
        }
    }
    return (const unsigned char*)"<unknown>";
}

static int interpret_node(struct generator *g, struct node *node)
{
    switch (node->type) {
    case c_repeat: {
        while (interpret_node(g, node->left)) {}
        return 0;
    }
    break;

    case c_or: {
        assert(0 && "c_or is not implemented yet");
    } break;

    default: {
        fprintf(stderr, "%s:%d: %s is not interpreted yet\n",
                g->analyser->tokeniser->file,
                node->line_number, printable_type_of_node(node->type));
        exit(1);
    }
    }
    return 0;
}

extern void interpret(struct generator *g)
{
    struct name *stem = find_stem(g);
    if (stem == NULL) {
        fprintf(stderr, "ERROR: Could not find an external `stem`\n");
        exit(1);
    }

    interpret_node(g, stem->definition);

    printf("%s:%d: `stem` declared in here\n", g->analyser->tokeniser->file, stem->declaration_line_number);
    struct node *def = stem->definition;
    printf("%s:%d: `stem` defined in here\n", g->analyser->tokeniser->file, def->line_number);

}
