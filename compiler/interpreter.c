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

static const char *printable_type_of_node(int type)
{
    switch (type) {
    case c_among:          return "c_among";
    case c_and:            return "c_and";
    case c_as:             return "c_as";
    case c_assign:         return "c_assign";
    case c_assignto:       return "c_assignto";
    case c_atleast:        return "c_atleast";
    case c_atlimit:        return "c_atlimit";
    case c_atmark:         return "c_atmark";
    case c_attach:         return "c_attach";
    case c_backwardmode:   return "c_backwardmode";
    case c_backwards:      return "c_backwards";
    case c_booleans:       return "c_booleans";
    case c_bra:            return "c_bra";
    case c_comment1:       return "c_comment1";
    case c_comment2:       return "c_comment2";
    case c_cursor:         return "c_cursor";
    case c_debug:          return "c_debug";
    case c_decimal:        return "c_decimal";
    case c_define:         return "c_define";
    case c_delete:         return "c_delete";
    case c_divide:         return "c_divide";
    case c_divideassign:   return "c_divideassign";
    case c_do:             return "c_do";
    case c_dollar:         return "c_dollar";
    case c_eq:             return "c_eq";
    case c_externals:      return "c_externals";
    case c_fail:           return "c_fail";
    case c_false:          return "c_false";
    case c_for:            return "c_for";
    case c_ge:             return "c_ge";
    case c_get:            return "c_get";
    case c_gopast:         return "c_gopast";
    case c_goto:           return "c_goto";
    case c_gr:             return "c_gr";
    case c_groupings:      return "c_groupings";
    case c_hex:            return "c_hex";
    case c_hop:            return "c_hop";
    case c_insert:         return "c_insert";
    case c_integers:       return "c_integers";
    case c_ket:            return "c_ket";
    case c_le:             return "c_le";
    case c_leftslice:      return "c_leftslice";
    case c_len:            return "c_len";
    case c_lenof:          return "c_lenof";
    case c_limit:          return "c_limit";
    case c_loop:           return "c_loop";
    case c_ls:             return "c_ls";
    case c_maxint:         return "c_maxint";
    case c_minint:         return "c_minint";
    case c_minus:          return "c_minus";
    case c_minusassign:    return "c_minusassign";
    case c_multiply:       return "c_multiply";
    case c_multiplyassign: return "c_multiplyassign";
    case c_ne:             return "c_ne";
    case c_next:           return "c_next";
    case c_non:            return "c_non";
    case c_not:            return "c_not";
    case c_or:             return "c_or";
    case c_plus:           return "c_plus";
    case c_plusassign:     return "c_plusassign";
    case c_repeat:         return "c_repeat";
    case c_reverse:        return "c_reverse";
    case c_rightslice:     return "c_rightslice";
    case c_routines:       return "c_routines";
    case c_set:            return "c_set";
    case c_setlimit:       return "c_setlimit";
    case c_setmark:        return "c_setmark";
    case c_size:           return "c_size";
    case c_sizeof:         return "c_sizeof";
    case c_slicefrom:      return "c_slicefrom";
    case c_sliceto:        return "c_sliceto";
    case c_stringdef:      return "c_stringdef";
    case c_stringescapes:  return "c_stringescapes";
    case c_strings:        return "c_strings";
    case c_substring:      return "c_substring";
    case c_test:           return "c_test";
    case c_tolimit:        return "c_tolimit";
    case c_tomark:         return "c_tomark";
    case c_true:           return "c_true";
    case c_try:            return "c_try";
    case c_unset:          return "c_unset";
    case c_mathassign:     return "c_mathassign";
    case c_name:           return "c_name";
    case c_number:         return "c_number";
    case c_literalstring:  return "c_literalstring";
    case c_neg:            return "c_neg";
    case c_call:           return "c_call";
    case c_grouping:       return "c_grouping";
    case c_booltest:       return "c_booltest";
    }
    return "<unknown>";
}

static int eq_s(struct SN_env * z, int s_size, const symbol * s) {
    if (z->l - z->c < s_size || memcmp(z->p + z->c, s, s_size * sizeof(symbol)) != 0) return 0;
    z->c += s_size; return 1;
}

/* Increase the size of the buffer pointed to by p to at least n symbols.
 * If insufficient memory, returns NULL and frees the old buffer.
 */
static symbol * increase_size(symbol * p, int n) {
    symbol * q;
    int new_size = n + 20;
    void * mem = realloc((char *) p - HEAD,
                         HEAD + (new_size + 1) * sizeof(symbol));
    if (mem == NULL) {
        lose_b(p);
        return NULL;
    }
    q = (symbol *) (HEAD + (char *)mem);
    CAPACITY(q) = new_size;
    return q;
}

/* to replace symbols between c_bra and c_ket in z->p by the
   s_size symbols at s.
   Returns 0 on success, -1 on error.
   Also, frees z->p (and sets it to NULL) on error.
*/
static int replace_s(struct SN_env * z, int c_bra, int c_ket, int s_size, const symbol * s, int * adjptr)
{
    int adjustment;
    int len;
    if (z->p == NULL) {
        z->p = create_b(0);
        if (z->p == NULL) return -1;
    }
    adjustment = s_size - (c_ket - c_bra);
    len = SIZE(z->p);
    if (adjustment != 0) {
        if (adjustment + len > CAPACITY(z->p)) {
            z->p = increase_size(z->p, adjustment + len);
            if (z->p == NULL) return -1;
        }
        memmove(z->p + c_ket + adjustment,
                z->p + c_ket,
                (len - c_ket) * sizeof(symbol));
        SET_SIZE(z->p, adjustment + len);
        z->l += adjustment;
        if (z->c >= c_ket)
            z->c += adjustment;
        else if (z->c > c_bra)
            z->c = c_bra;
    }
    if (s_size) memmove(z->p + c_bra, s, s_size * sizeof(symbol));
    if (adjptr != NULL)
        *adjptr = adjustment;
    return 0;
}

static int slice_check(struct SN_env * z) {

    if (z->bra < 0 ||
        z->bra > z->ket ||
        z->ket > z->l ||
        z->p == NULL ||
        z->l > SIZE(z->p)) /* this line could be removed */
    {
#if 0
        fprintf(stderr, "faulty slice operation:\n");
        debug(z, -1, 0);
#endif
        return -1;
    }
    return 0;
}

static int slice_from_s(struct SN_env * z, int s_size, const symbol * s) {
    if (slice_check(z)) return -1;
    return replace_s(z, z->bra, z->ket, s_size, s, NULL);
}

static int slice_from_v(struct SN_env * z, const symbol * p) {
    return slice_from_s(z, SIZE(p), p);
}

static int interpret_AE(struct generator *g, struct node *p)
{
    switch (p->type) {
    case c_number: {
        return p->number;
    } break;
    }

    fprintf(stderr, "%s:%d: AE %s is not interpreted yet\n",
            g->analyser->tokeniser->file,
            p->line_number, printable_type_of_node(p->type));
    fprintf(stderr, "%s:%d: add another switch-case up there\n", __FILE__, __LINE__);
    exit(1);
    return 0;
}

static int interpret_node(struct generator *g, struct SN_env *z, struct node *p)
{
    switch (p->type) {
    case c_repeat: {
        while (1) {
            int c = z->c;
            int s = interpret_node(g, z, p->left);
            if (!s) {
                z->c = c;
                return 1;
            }
        }
        assert(0 && "unreachable");
    }
    break;

    case c_or: {
        p = p->left;
        while (p) {
            int c = z->c;
            int s = interpret_node(g, z, p);
            if (s) return 1;
            z->c = c;
            p = p->right;
        }
        return 0;
    }
    break;

    case c_and: {
        p = p->left;
        while (p) {
            int c = z->c;
            int s = interpret_node(g, z, p);
            if (!s) return 0;
            z->c = c;
            p = p->right;
        }
        return 1;
    }
    break;

    case c_bra: {
        p = p->left;
        while (p) {
            int s = interpret_node(g, z, p);
            if (!s) return 0;
            p = p->right;
        }
        return 1;
    }
    break;

    case c_leftslice: {
        assert(p->mode == m_forward);
        z->bra = z->c;
        return 1;
    } break;

    case c_rightslice: {
        assert(p->mode == m_forward);
        z->ket = z->c;
        return 1;
    } break;

    case c_literalstring: {
        symbol * b = p->literalstring;
        return eq_s(z, SIZE(b), b);
    } break;

    case c_slicefrom: {
        assert(p->literalstring != NULL);
        slice_from_v(z, p->literalstring);
        return 1;
    } break;

    case c_true: {
        return 1;
    } break;

    case c_false: {
        return 0;
    } break;

    case c_mathassign: {
        switch (p->name->type) {
        case t_integer: {
            int count = p->name->count;
            if (count < 0) {
                fprintf(stderr, "Reference to optimised out variable ");
                report_b(stderr, p->name->b);
                fprintf(stderr, " attempted\n");
                exit(1);
            }
            z->I[count] = interpret_AE(g, p->AE);
        } break;

        default: assert(0 && "TODO: assigning this kind of variables is not implemented yet");
        }

        return 1;
    } break;
    }

    fprintf(stderr, "%s:%d: command %s is not interpreted yet\n",
            g->analyser->tokeniser->file,
            p->line_number, printable_type_of_node(p->type));
    fprintf(stderr, "%s:%d: add another switch-case up there\n", __FILE__, __LINE__);
    exit(1);
    return 0;
}

extern struct SN_env *SN_create_env(struct generator *g)
{
    int * p = g->analyser->name_count;
    int S_size = p[t_string];
    int I_size = p[t_integer] + p[t_boolean];

    struct SN_env * z = (struct SN_env *) calloc(1, sizeof(struct SN_env));
    if (z == NULL) return NULL;
    z->p = create_b(0);
    if (z->p == NULL) goto error;
    if (S_size)
    {
        int i;
        z->S = (symbol * *) calloc(S_size, sizeof(symbol *));
        if (z->S == NULL) goto error;

        for (i = 0; i < S_size; i++)
        {
            z->S[i] = create_b(0);
            if (z->S[i] == NULL) goto error;
        }
    }

    if (I_size)
    {
        z->I = (int *) calloc(I_size, sizeof(int));
        if (z->I == NULL) goto error;
    }

    return z;
error:
    SN_close_env(z, g);
    return NULL;
}

extern void SN_close_env(struct SN_env *z, struct generator *g)
{
    int * p = g->analyser->name_count;
    int S_size = p[t_string];
    if (z == NULL) return;
    if (S_size)
    {
        int i;
        for (i = 0; i < S_size; i++)
        {
            lose_b(z->S[i]);
        }
        free(z->S);
    }
    free(z->I);
    if (z->p) lose_b(z->p);
    free(z);
}

extern void interpret(struct generator *g, struct SN_env *z)
{
    struct name *stem = find_stem(g);
    if (stem == NULL) {
        fprintf(stderr, "ERROR: Could not find an external called `stem`\n");
        exit(1);
    }

    interpret_node(g, z, stem->definition);
}
