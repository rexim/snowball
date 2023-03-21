// TODO: We need to explicitly support different encodings (ENC_UTF8, ENC_SINGLEBYTE, etc.)
//
// So far I've been using those stock dynamic arrays of symbols which are 2 bytes long.
// But after looking around I realized that this is not particularly what we want, because those
// arrays were designed to handle the source code of Snowball programs, not their input.
//
// We want to handle the input just like the generated programs. But unlike the generated
// programs we have too support all the encodings at once, because the user may switch between
// them at runtime.
//
// I do that after I make english.sbl work on words "linear" and "linearly".

#include <limits.h>  /* for INT_MAX */
#include <stdlib.h>  /* for free */
#include <stdio.h>   /* for printf */
#include <assert.h>  /* for assert */
#include <string.h>  /* for strcmp */
#include "header.h"

/// Debug Tools
///
/// Stuff that is useful while developing the interpreter but probably going to be removed after it's done.

static const char *printable_type_of_name(int type)
{
    switch (type) {
    case t_size:     return "t_size";
    case t_string:   return "t_string";
    case t_boolean:  return "t_boolean";
    case t_integer:  return "t_integer";
    case t_routine:  return "t_routine";
    case t_external: return "t_external";
    case t_grouping: return "t_grouping";
    }
    return "<unknown>";
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

/// Runtime.
/// Basically a mirroring of the runtime stuff that you are supposed to link the compiled stemmers with.
/// Primarily a copy-paste from ../runtime/

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

static int eq_s_b(struct SN_env * z, int s_size, const symbol * s) {
    if (z->c - z->lb < s_size || memcmp(z->p + z->c - s_size, s, s_size * sizeof(symbol)) != 0) return 0;
    z->c -= s_size; return 1;
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

/* find_among_b is for backwards processing. Same comments apply */

static int find_among_b(struct SN_env * z, const struct amongvec * v, int v_size) {

    int i = 0;
    int j = v_size;

    int c = z->c; int lb = z->lb;
    const symbol * q = z->p + c - 1;

    const struct amongvec * w;

    int common_i = 0;
    int common_j = 0;

    int first_key_inspected = 0;

    while (1) {
        int k = i + ((j - i) >> 1);
        int diff = 0;
        int common = common_i < common_j ? common_i : common_j;
        w = v + k;
        {
            int i2; for (i2 = w->size - 1 - common; i2 >= 0; i2--) {
                if (c - common == lb) { diff = -1; break; }
                diff = q[- common] - w->b[i2];
                if (diff != 0) break;
                common++;
            }
        }
        if (diff < 0) { j = k; common_j = common; }
                 else { i = k; common_i = common; }
        if (j - i <= 1) {
            if (i > 0) break;
            if (j == i) break;
            if (first_key_inspected) break;
            first_key_inspected = 1;
        }
    }
    while (1) {
        w = v + i;
        if (common_i >= w->size) {
            z->c = c - w->size;
            assert(w->function == 0 && "TODO: we don't really know what this function is used for yet");
            if (w->function == 0) return w->result;
#if 0
            {
                int res = w->function(z);
                z->c = c - w->s_size;
                if (res) return w->result;
            }
#endif
        }
        i = w->i;
        if (i < 0) return 0;
    }
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

static int out_grouping(struct SN_env * z, const symbol * s, int min, int max, int repeat) {
    do {
        int ch;
        if (z->c >= z->l) return -1;
        ch = z->p[z->c];
        if (!(ch > max || (ch -= min) < 0 || (s[ch >> 3] & (0X1 << (ch & 0X7))) == 0))
            return 1;
        z->c++;
    } while (repeat);
    return 0;
}

static int in_grouping(struct SN_env * z, const symbol * s, int min, int max, int repeat) {
    do {
        int ch;
        if (z->c >= z->l) return -1;
        ch = z->p[z->c];
        if (ch > max || (ch -= min) < 0 || (s[ch >> 3] & (0X1 << (ch & 0X7))) == 0)
            return 1;
        z->c++;
    } while (repeat);
    return 0;
}

/// Interpreter.
/// Everything related to traversing the Snowball AST and actually interpreting it.

static int interpret_command(struct generator *g, struct SN_env *z, struct node *p);

static int interpret_AE(struct generator *g, struct SN_env *z, struct node *p) {
    assert(p->mode == m_forward && "TODO: only forward mode is supported for now");

    switch (p->type) {
    case c_number: return p->number;

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

        default:
            tracef_linenumber(g, p, "%s: type is not interpreted yet.\n", printable_type_of_name(p->name->type));
            tracef_here("add another switch-case up there\n");
            exit(1);
        }
    } break;

    case c_limit: return z->l;
    }

    tracef_node(g, p, "AE is not interpreted yet\n");
    tracef_here("add another switch-case up there\n");
    exit(1);
    return 0;
}

static int interpret_repeat(struct generator *g, struct SN_env *z, struct node *p) {
    assert(p->mode == m_forward && "TODO: only forward mode is supported for now");

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

static int interpret_or(struct generator *g, struct SN_env *z, struct node *p) {
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

static int interpret_and(struct generator *g, struct SN_env *z, struct node *p) {
    assert(p->mode == m_forward && "TODO: only forward mode is supported for now");

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

static int interpret_bra(struct generator *g, struct SN_env *z, struct node *p) {
    p = p->left;
    while (p) {
        int s = interpret_command(g, z, p);
        if (!s) return 0;
        p = p->right;
    }
    return 1;
}

static int interpret_leftslice(struct SN_env *z, struct node *p) {
    if (p->mode == m_forward) {
        z->bra = z->c;
    } else {
        z->ket = z->c;
    }
    return 1;
}

static int interpret_rightslice(struct SN_env *z, struct node *p) {
    if (p->mode == m_forward) {
        z->ket = z->c;
    } else {
        z->bra = z->c;
    }
    return 1;
}

static int interpret_literalstring(struct SN_env *z, struct node *p) {
    // TODO: in the generator version there is also case when SIZE(b) == 1 for optimization reasons
    // Should we do the same in here too?
    symbol * b = p->literalstring;
    return p->mode == m_forward ? eq_s(z, SIZE(b), b) : eq_s_b(z, SIZE(b), b);
}

static int interpret_slicefrom(struct SN_env *z, struct node *p) {
    assert(p->mode == m_forward && "TODO: only forward mode is supported for now");
    assert(p->literalstring != NULL);
    slice_from_v(z, p->literalstring);
    return 1;
}

static int interpret_mathassign(struct generator *g, struct SN_env *z, struct node *p) {
    assert(p->mode == m_forward && "TODO: only forward mode is supported for now");
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

    default:
        tracef_linenumber(g, p, "%s: type is not interpreted yet.\n", printable_type_of_name(p->name->type));
        tracef_here("add another switch-case up there\n");
        exit(1);
    }

    return 1;
}

static int interpret_substring(struct SN_env *z, struct node *p) {
    // TODO: The original generate_substring() had some sort of optimization. Is it applicable here?
    struct among * x = p->among;
    int among_var;
    if (p->mode == m_forward) {
        among_var = find_among(z, x->b, x->literalstring_count);
    } else {
        among_var = find_among_b(z, x->b, x->literalstring_count);
    }
    // TODO: I'm not sure if this is the best solution since I have a limited understand of how amogus works
    if (x->amongvar_needed) z->among_var = among_var;
    return among_var;
}

static int interpret_among(struct generator *g, struct SN_env *z, struct node *p) {
    assert(p->mode == m_forward && "TODO: only forward mode is supported for now");
    struct among * x = p->among;
    if (x->substring == 0) {
        assert(x->command_count == 0);
        assert(x->starter == NULL);
        return interpret_substring(z, p);
    }
    if (x->starter != 0) {
        tracef_node(g, p, "interpret(x->starter)\n");
        assert(0 && "TODO: interpret(x->starter)");
        //generate(g, x->starter);
    }
    if (x->command_count == 1 && x->nocommand_count == 0) {
        /* Only one outcome ("no match" already handled). */
        tracef_node(g, p, "interpret(x->commands[0])\n");
        assert(0 && "TODO: interpret(x->commands[0])");
        //generate(g, x->commands[0]);
    } else if (x->command_count > 0) {
        assert(x->amongvar_needed);
        assert(z->among_var - 1 < x->command_count);
        return interpret_command(g, z, x->commands[z->among_var - 1]);
    }
    assert(0 && "TODO: c_among");
}

static int interpret_atlimit(struct SN_env *z, struct node *p) {
    assert(p->mode == m_forward && "TODO: only forward mode is supported for now");
    return !(z->c < z->l);
}

static int interpret_hop(struct generator *g, struct SN_env *z, struct node *p) {
    assert(p->mode == m_forward && "TODO: only forward mode is supported for now");
    // Based on the description from manual
    int ae = interpret_AE(g, z, p->AE);
    if (ae < 0) return 0;
    if (z->c + ae > z->l) return 0;
    z->c += ae;
    return 1;
}

static int interpret_do(struct generator *g, struct SN_env *z, struct node *p) {
    int c = z->c;
    interpret_command(g, z, p->left);
    z->c = c;
    return 1;
}

static int interpret_unset(struct generator *g, struct SN_env *z, struct node *p) {
    assert(p->mode == m_forward && "TODO: only forward mode is supported for now");
    assert(p->name->type == t_boolean);
    /* We use a single array for booleans and integers, with the
     * integers first.
     */
    int count = p->name->count + g->analyser->name_count[t_integer];
    z->I[count] = 0;
    return 1;
}

static int interpret_GO(struct generator *g, struct SN_env *z, struct node *p, int style) {
    assert(p->mode == m_forward && "TODO: only forward mode is support for now");
    // assert(!(p->left->type == c_grouping || p->left->type == c_non));
    // TODO: there is an optimization when (p->left->type == c_grouping || p->left->type == c_non)
    // See generate_GO and generate_GO_grouping() for more info.
    assert(g->options->encoding == ENC_SINGLEBYTE && "TODO: only single byte encoding supported for now");
    while (z->c < z->l) {
        int c = z->c;
        int ret = interpret_command(g, z, p->left);
        if (ret) {
            if (style) z->c = c;
            return 1;
        }

        z->c += 1;
    }
    return 0;
}

// NOTE: literally copy-pasted from generator.c
static void set_bit(symbol * b, int i) { b[i/8] |= 1 << i%8; }
static symbol *generate_grouping_table(struct grouping * q) {
    int range = q->largest_ch - q->smallest_ch + 1;
    int size = (range + 7)/ 8;  /* assume 8 bits per symbol */
    symbol * b = q->b;
    symbol * map = create_b(size);
    int i;
    for (i = 0; i < size; i++) map[i] = 0;

    for (i = 0; i < SIZE(b); i++) set_bit(map, b[i] - q->smallest_ch);

    return map;
}

static int interpret_grouping(struct generator *g, struct SN_env *z, struct node *p, int complement) {
    assert(p->mode == m_forward && "TODO: only forward mode is supported for now");
    struct grouping * q = p->name->grouping;
    assert(g->options->encoding == ENC_SINGLEBYTE && "TODO: only single byte encoding supported for now");
    // TODO: This is extremely slow. In generator.c these tables are pre-generated at compile time.
    //
    // Here we are rebuilding them every time at runtime. It's fine just advance the development.
    // But we need to go back here later and pre-generate it as well. Maybe put them in the SN_env.
    symbol *map = generate_grouping_table(q);
    int ret;
    if (complement) {
        ret = out_grouping(z, map, q->smallest_ch, q->largest_ch, 0);
    } else {
        ret = in_grouping(z, map, q->smallest_ch, q->largest_ch, 0);
    }
    lose_b(map);
    return ret;
}

static int interpret_setmark(struct generator *g, struct SN_env *z, struct node *p)
{
    assert(p->mode == m_forward && "TODO: only forward mode is supported for now");
    switch (p->name->type) {
    case t_integer: {
        int count = p->name->count;
        if (count < 0) {
            fprintf(stderr, "Reference to optimised out variable ");
            report_b(stderr, p->name->b);
            fprintf(stderr, " attempted\n");
            exit(1);
        }
        z->I[count] = z->c;
    } break;

    default:
        tracef_linenumber(g, p, "%s: type is not interpreted yet.\n", printable_type_of_name(p->name->type));
        tracef_here("add another switch-case up there\n");
        exit(1);
    }
    return 0;
}

static int interpret_backwards(struct generator *g, struct SN_env *z, struct node *p) {
    assert(p->mode == m_backward);
    assert(p->left->mode == m_backward);

    // // From the generator
    // writef(g, "~Mz->lb = z->c; z->c = z->l;~C~N", p);
    // generate(g, p->left);
    // w(g, "~Mz->c = z->lb;~N");

    z->lb = z->c;
    z->c = z->l;
    int ret = interpret_command(g, z, p->left);
    z->c = z->lb;
    return ret;
}

static int interpret_try(struct generator *g, struct SN_env *z, struct node *p)
{
    interpret_command(g, z, p->left);
    return 1;
}

static int interpret_booltest(struct generator *g, struct SN_env *z, struct node *p)
{
    assert(p->name->type == t_boolean);
    /* We use a single array for booleans and integers, with the
     * integers first.
     */
    int count = p->name->count + g->analyser->name_count[t_integer];
    return z->I[count];
}

static int interpret_command(struct generator *g, struct SN_env *z, struct node *p) {
    switch (p->type) {
    case c_repeat:        return interpret_repeat(g, z, p);
    case c_or:            return interpret_or(g, z, p);
    case c_and:           return interpret_and(g, z, p);
    case c_bra:           return interpret_bra(g, z, p);
    case c_leftslice:     return interpret_leftslice(z, p);
    case c_rightslice:    return interpret_rightslice(z, p);
    case c_literalstring: return interpret_literalstring(z, p);
    case c_slicefrom:     return interpret_slicefrom(z, p);
    case c_true:          return 1;
    case c_false:         return 0;
    case c_mathassign:    return interpret_mathassign(g, z, p);
    case c_eq:            return interpret_AE(g, z, p->left) == interpret_AE(g, z, p->AE);
    case c_call:          return interpret_command(g, z, p->name->definition);
    case c_substring:     return interpret_substring(z, p);
    case c_among:         return interpret_among(g, z, p);
    case c_atlimit:       return interpret_atlimit(z, p);
    case c_not:           return !interpret_command(g, z, p->left);
    case c_hop:           return interpret_hop(g, z, p);
    case c_do:            return interpret_do(g, z, p);
    case c_unset:         return interpret_unset(g, z, p);
    case c_grouping:      return interpret_grouping(g, z, p, false);
    case c_non:           return interpret_grouping(g, z, p, true);
    case c_goto:          return interpret_GO(g, z, p, 1);
    case c_gopast:        return interpret_GO(g, z, p, 0);
    case c_setmark:       return interpret_setmark(g, z, p);
    case c_backwards:     return interpret_backwards(g, z, p);
    case c_try:           return interpret_try(g, z, p);
    case c_booltest:      return interpret_booltest(g, z, p);
    }

    tracef_node(g, p, "command is not interpreted yet\n");
    tracef_here("add another switch-case up there\n");
    exit(1);
    return 0;
}

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

// TODO: passing `struct generator` everywhere is an overkill. We only need `struct analyzer` and `struct options`.
// Let's construct `struct interpreter` similar to `struct generator` that will contain both
// the analyser and the options plus maybe some interpreter-specific stuff if we ever need that.
extern void interpret(struct generator *g, struct SN_env *z)
{
    struct name *stem = find_stem(g);
    if (stem == NULL) {
        fprintf(stderr, "ERROR: Could not find an external called `stem`\n");
        exit(1);
    }

    interpret_command(g, z, stem->definition);
}
