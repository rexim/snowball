#include <limits.h>  /* for INT_MAX */
#include <stdlib.h>  /* for free */
#include <stdio.h>   /* for printf */
#include <assert.h>  /* for assert */
#include <string.h>  /* for strcmp */
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

#define tracef_linenumber(g, p, ...) \
    do { \
        fprintf(stderr, "%s:%d: ", (g)->analyser->tokeniser->file, (p)->line_number); \
        fprintf(stderr, __VA_ARGS__); \
    } while (0)

#define tracef_node(g, p, ...) \
    do { \
        tracef_linenumber((g), (p), "%s: ", printable_type_of_node((p)->type)); \
        fprintf(stderr, __VA_ARGS__); \
    } while (0)

#define tracef_here(...) \
    do { \
        fprintf(stderr, "%s:%d: ", __FILE__, __LINE__); \
        fprintf(stderr, __VA_ARGS__); \
    } while (0)

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

static int find_among(struct SN_env * z, const struct amongvec * v, int v_size) {

    int i = 0;
    int j = v_size;

    int c = z->c; int l = z->l;
    const symbol * q = z->p + c;

    const struct amongvec * w;

    int common_i = 0;
    int common_j = 0;

    int first_key_inspected = 0;

    while (1) {
        int k = i + ((j - i) >> 1);
        int diff = 0;
        int common = common_i < common_j ? common_i : common_j; /* smaller */
        w = v + k;
        {
            int i2; for (i2 = common; i2 < w->size; i2++) {
                if (c + common == l) { diff = -1; break; }
                diff = q[common] - w->b[i2];
                if (diff != 0) break;
                common++;
            }
        }
        if (diff < 0) {
            j = k;
            common_j = common;
        } else {
            i = k;
            common_i = common;
        }
        if (j - i <= 1) {
            if (i > 0) break; /* v->s has been inspected */
            if (j == i) break; /* only one item in v */

            /* - but now we need to go round once more to get
               v->s inspected. This looks messy, but is actually
               the optimal approach.  */

            if (first_key_inspected) break;
            first_key_inspected = 1;
        }
    }
    while (1) {
        w = v + i;
        if (common_i >= w->size) {
            z->c = c + w->size;
            assert(w->function == 0 && "TODO: we don't really know what this function is used for yet");
            if (w->function == 0) return w->result;
#if 0
            {
                int res = w->function(z);
                z->c = c + w->s_size;
                if (res) return w->result;
            }
#endif
        }
        i = w->i;
        if (i < 0) return 0;
    }
}

static int interpret_AE(struct generator *g, struct SN_env *z, struct node *p)
{
    switch (p->type) {
    case c_number: {
        return p->number;
    } break;

    case c_name: {
        switch (p->name->type) {
        case t_integer: {
            int count = p->name->count;
            if (count < 0) {
                fprintf(stderr, "Reference to optimised out variable ");
                report_b(stderr, p->name->b);
                fprintf(stderr, " attempted\n");
                exit(1);
            }
            return z->I[count];
        } break;

        default: assert(0 && "TODO: evaluting this kind of variables is not implemented yet");
        }
    } break;
    }

    tracef_node(g, p, "AE is not interpreted yet\n");
    tracef_here("add another switch-case up there\n");
    exit(1);
    return 0;
}

static int interpret_command(struct generator *g, struct SN_env *z, struct node *p)
{
    switch (p->type) {
    case c_repeat: {
        while (1) {
            int c = z->c;
            int s = interpret_command(g, z, p->left);
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
            int s = interpret_command(g, z, p);
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
            int s = interpret_command(g, z, p);
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
            int s = interpret_command(g, z, p);
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
        // TODO: in the generator there is also case when SIZE(b) == 1 for optimization reasons
        // Should we do the same in here too?
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
            z->I[count] = interpret_AE(g, z, p->AE);
        } break;

        default: assert(0 && "TODO: assigning this kind of variables is not implemented yet");
        }

        return 1;
    } break;

    case c_eq: {
        return interpret_AE(g, z, p->left) == interpret_AE(g, z, p->AE);
    } break;

    case c_call: {
        return interpret_command(g, z, p->name->definition);
    } break;

    case c_substring: {
        assert(p->mode == m_forward && "TODO: only forward mode is supported for now");
        // TODO: The original generate_substring() had some sort of optimization. Is it applicable here?
        struct among * x = p->among;
        int among_var = find_among(z, x->b, x->literalstring_count);
        // TODO: I'm not sure if this is the best solution since I have a limited understand of how amogus works
        if (x->amongvar_needed) z->among_var = among_var;
        return among_var;
    } break;

    case c_among: {
        struct among * x = p->among;
        if (x->substring == 0) {
            assert(0 && "TODO: interpret_substring()");
            //generate_substring(g, p);
        }
        if (x->starter != 0) {
            assert(0 && "TODO: interpret(x->starter)");
            //generate(g, x->starter);
        }
        if (x->command_count == 1 && x->nocommand_count == 0) {
            /* Only one outcome ("no match" already handled). */
            assert(0 && "TODO: interpret(x->commands[0])");
            //generate(g, x->commands[0]);
        } else if (x->command_count > 0) {
            assert(x->amongvar_needed);
            assert(z->among_var - 1 < x->command_count);
            return interpret_command(g, z, x->commands[z->among_var - 1]);
        }
        assert(0 && "TODO: c_among");
    } break;

    case c_atlimit: {
        assert(p->mode == m_forward && "TODO: only forward mode i supported for now");
        return !(z->c < z->l);
    } break;
    }

    tracef_node(g, p, "command is not interpreted yet\n");
    tracef_here("add another switch-case up there\n");
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

    interpret_command(g, z, stem->definition);
}
