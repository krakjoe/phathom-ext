/*
  +----------------------------------------------------------------------+
  | phathom                                                              |
  +----------------------------------------------------------------------+
  | Copyright (c) Joe Watkins 2026                                       |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Author: krakjoe                                                      |
  +----------------------------------------------------------------------+
 */
#include "phathom.h"

#include "chart.h"
#include "chart_arginfo.h"

#include "buffer.h"
#include "grammar.h"
#include "token.h"
#include "alternative.h"
#include "hash.h"

#include "automaton.h"
#include "reduction.h"
#include "worklist.h"

zend_class_entry* php_phathom_glr_chart_ce = NULL;
static zend_object_handlers php_phathom_glr_chart_handlers;

/* {{{ object lifecycle */
static zend_object* php_phathom_glr_chart_create(zend_class_entry* type) {
    php_phathom_glr_chart_t* chart = ecalloc(1,
        sizeof(php_phathom_glr_chart_t) + zend_object_properties_size(type));

    zend_object_std_init(&chart->std, type);
    object_properties_init(&chart->std, type);

    chart->std.handlers = &php_phathom_glr_chart_handlers;

    chart->arena = zend_arena_create(1024 * 64);

    zend_hash_init(&chart->tokens, 8, NULL, ZVAL_PTR_DTOR, 0);

    return &chart->std;
}

static void php_phathom_glr_chart_free(zend_object* std) {
    php_phathom_glr_chart_t* chart = php_phathom_glr_chart_fetch(std);

    if (chart->arena) {
        zend_arena_destroy(chart->arena);
    }

    zend_hash_destroy(&chart->tokens);

    php_phathom_grammar_free(&chart->grammar);
    php_phathom_buffer_free(&chart->buffer);

    zend_object_std_dtor(std);
} /* }}} */

/* {{{ resolve
 *
 * Mirrors Chart::resolve().  Returns:
 *   a    -- a is the winner
 *   b    -- b is the winner
 *   NULL -- unresolvable (both propagate, root detects ambiguity)
 */
static zend_always_inline php_phathom_glr_node_t*
php_phathom_glr_chart_resolve(
    php_phathom_t          *phathom,
    php_phathom_glr_node_t *a,
    php_phathom_glr_node_t *b)
{
    zend_long pri_a = php_phathom_alternative_priority(a->alternative);
    zend_long pri_b = php_phathom_alternative_priority(b->alternative);

    if (pri_a == ZEND_LONG_MIN || pri_b == ZEND_LONG_MIN) {
        return NULL;
    }
    if (pri_a != pri_b) {
        return pri_a > pri_b ? a : b;
    }

    zend_object *assoc =
        php_phathom_alternative_associativity(a->alternative);

    if (assoc == phathom->enumerated.associativity.left) {
        return php_phathom_glr_node_left_end(a) >=
               php_phathom_glr_node_left_end(b) ? a : b;
    }
    return php_phathom_glr_node_left_end(a) <=
           php_phathom_glr_node_left_end(b) ? a : b;
} /* }}} */

/* {{{ reduce
 *
 * Apply all reductions to fixed point with local ambiguity resolution.
 *
 * best_node: php_phathom_hash_t keyed by binary (base_states, goto, rule, pos)
 *            -> php_phathom_glr_node_t*
 *
 * node_key:  php_phathom_hash_t keyed by (uintptr_t)node
 *            -> arena-allocated key blob {size_t ksize, uint64_t data[]}
 *            used to check if a thread's top node has been superseded.
 */
#define PHP_PHATHOM_GLR_KEY_MAX_DEPTH 1024

static void
php_phathom_glr_chart_reduce(
    php_phathom_t            *phathom,
    php_phathom_glr_chart_t  *chart,
    zend_object              *automaton,
    php_phathom_glr_thread_t **threads,
    zend_long                  nthreads,
    php_phathom_glr_worklist_t *result)
{
    zend_array *reduces = php_phathom_glr_automaton_reduces(automaton);
    zend_array *goto_t  = php_phathom_glr_automaton_goto(automaton);

    zend_arena *arena = zend_arena_create(65536);

    php_phathom_hash_t best_node;
    php_phathom_hash_init(&best_node, &arena, 16);

    php_phathom_hash_t node_key;
    php_phathom_hash_init(&node_key, &arena, 16);

    php_phathom_glr_worklist_t wl = {0};
    for (zend_long i = 0; i < nthreads; i++) {
        php_phathom_glr_worklist_push(&wl, threads[i]);
    }

    zend_long cursor = 0;

    while (cursor < wl.used) {
        php_phathom_glr_thread_t *thread = wl.data[cursor++];

        /* ---- Supersede check ------------------------------------------ */
        if (thread->depth > 0 &&
            thread->nodes[thread->depth - 1].kind == PHP_PHATHOM_GLR_SLOT_NODE) {

            php_phathom_glr_node_t *top_node =
                thread->nodes[thread->depth - 1].node;

            void *kblob = php_phathom_hash_find_index(
                &node_key, (uint64_t)(uintptr_t) top_node);

            if (kblob) {
                size_t  ksize = *(size_t *) kblob;
                void   *kdata = (char *) kblob + sizeof(size_t);
                php_phathom_glr_node_t *best =
                    (php_phathom_glr_node_t *)
                        php_phathom_hash_find_binary(&best_node, kdata, ksize);
                if (best != top_node) {
                    continue; /* superseded */
                }
            }
        }
        /* --------------------------------------------------------------- */

        php_phathom_glr_worklist_push(result, thread);

        zend_long top_state = thread->states[thread->depth - 1];

        zval *red_list_v = zend_hash_index_find(reduces, (zend_ulong) top_state);
        if (!red_list_v) {
            continue;
        }
        zval *red_arr = Z_UNWRAP_P(red_list_v);
        if (Z_TYPE_P(red_arr) != IS_ARRAY) {
            continue;
        }

        zval *red_v;
        ZEND_HASH_FOREACH_VAL(Z_ARR_P(red_arr), red_v) {
            zend_object *reduction = Z_OBJ_P(Z_UNWRAP_P(red_v));

            zend_string *red_rule    = php_phathom_glr_reduction_rule(reduction);
            zend_long    red_alt     = php_phathom_glr_reduction_alt(reduction);
            zend_long    red_len     = php_phathom_glr_reduction_length(reduction);
            zend_object *red_alt_obj = php_phathom_glr_reduction_alternative(reduction);

            php_phathom_glr_slot_t   *children;
            php_phathom_glr_thread_t *base =
                php_phathom_glr_thread_pop(
                    &chart->arena, thread, red_len, &children);

            zend_long base_top = base->states[base->depth - 1];

            zval *goto_row_v = zend_hash_index_find(goto_t, (zend_ulong) base_top);
            if (!goto_row_v) {
                efree(children);
                continue;
            }
            zval *goto_row = Z_UNWRAP_P(goto_row_v);
            if (Z_TYPE_P(goto_row) != IS_ARRAY) {
                efree(children);
                continue;
            }
            zval *goto_v = zend_hash_find(Z_ARR_P(goto_row), red_rule);
            if (!goto_v) {
                efree(children);
                continue;
            }
            zend_long goto_state = Z_LVAL_P(Z_UNWRAP_P(goto_v));

            /* children ownership transfers to node_new */
            php_phathom_glr_node_t *node =
                php_phathom_glr_node_new(
                    &chart->arena,
                    red_rule, red_alt, red_alt_obj,
                    children, red_len,
                    thread->pos);

            /* Build binary key for best_node lookup */
            ZEND_ASSERT(base->depth <= PHP_PHATHOM_GLR_KEY_MAX_DEPTH);
            size_t   ksize = (size_t)(base->depth + 4) * sizeof(uint64_t);
            uint64_t kbuf[PHP_PHATHOM_GLR_KEY_MAX_DEPTH + 4];
            kbuf[0] = (uint64_t) base->depth;
            for (zend_long ki = 0; ki < base->depth; ki++) {
                kbuf[ki + 1] = (uint64_t) base->states[ki];
            }
            kbuf[base->depth + 1] = (uint64_t) goto_state;
            kbuf[base->depth + 2] = (uint64_t)(uintptr_t) red_rule;
            kbuf[base->depth + 3] = (uint64_t) thread->pos;

            bool inserted;
            void **slot = php_phathom_hash_slot(
                &best_node,
                php_phathom_hash_key_binary(kbuf, ksize),
                &inserted);

            if (!inserted) {
                php_phathom_glr_node_t *existing =
                    (php_phathom_glr_node_t *) *slot;
                php_phathom_glr_node_t *winner =
                    php_phathom_glr_chart_resolve(phathom, existing, node);

                if (winner == existing) {
                    /* node loses */
                    continue;
                }
                if (winner == NULL) {
                    /* Unresolvable -- let both propagate */
                    php_phathom_glr_thread_t *nt =
                        php_phathom_glr_thread_push(
                            &chart->arena, base, goto_state, node);
                    php_phathom_glr_worklist_push(&wl, nt);
                    continue;
                }
                /* winner == node: replace in best_node */
                *slot = node;
            } else {
                *slot = node;
            }

            /* node_key[node] = arena-allocated {ksize, kbuf} for supersede detection */
            void *kblob = zend_arena_alloc(&arena, sizeof(size_t) + ksize);
            *(size_t *) kblob = ksize;
            memcpy((char *) kblob + sizeof(size_t), kbuf, ksize);
            bool nk_inserted;
            void **nk_slot = php_phathom_hash_slot(
                &node_key,
                php_phathom_hash_key_index((uint64_t)(uintptr_t) node),
                &nk_inserted);
            *nk_slot = kblob;

            php_phathom_glr_thread_t *nt =
                php_phathom_glr_thread_push(
                    &chart->arena, base, goto_state, node);
            php_phathom_glr_worklist_push(&wl, nt);

        } ZEND_HASH_FOREACH_END();
    }

    php_phathom_glr_worklist_free(&wl);
    zend_arena_destroy(arena);
} /* }}} */

/* {{{ expected */
static void
php_phathom_glr_chart_expected(
    zend_object              *automaton,
    php_phathom_glr_thread_t **threads,
    zend_long                  nthreads,
    zval                      *expected)
{
    array_init(expected);

    zend_array *shifts = php_phathom_glr_automaton_shifts(automaton);

    for (zend_long i = 0; i < nthreads; i++) {
        zend_long top = threads[i]->states[threads[i]->depth - 1];

        zval *state_shifts = zend_hash_index_find(shifts, (zend_ulong) top);
        if (!state_shifts) {
            continue;
        }
        zval *sa = Z_UNWRAP_P(state_shifts);
        if (Z_TYPE_P(sa) != IS_ARRAY) {
            continue;
        }

        zend_ulong tid;
        zval      *_unused;
        ZEND_HASH_FOREACH_NUM_KEY_VAL(Z_ARR_P(sa), tid, _unused) {
            add_index_bool(expected, tid, true);
        } ZEND_HASH_FOREACH_END();
    }
} /* }}} */

/* {{{ shift
 *
 * Deduplicate shifted threads by new state-stack binary content.
 */
static void
php_phathom_glr_chart_shift(
    php_phathom_glr_chart_t   *chart,
    zend_object               *automaton,
    php_phathom_glr_thread_t **threads,
    zend_long                  nthreads,
    zend_long                  type,
    zend_long                  ti,
    php_phathom_glr_worklist_t *shifted)
{
    zend_array *shifts = php_phathom_glr_automaton_shifts(automaton);
    zend_arena *arena  = zend_arena_create(4096);

    php_phathom_hash_t seen;
    php_phathom_hash_init(&seen, &arena, 16);

    for (zend_long i = 0; i < nthreads; i++) {
        php_phathom_glr_thread_t *t   = threads[i];
        zend_long                 top = t->states[t->depth - 1];

        zval *state_shifts = zend_hash_index_find(shifts, (zend_ulong) top);
        if (!state_shifts) {
            continue;
        }
        zval *sa = Z_UNWRAP_P(state_shifts);
        if (Z_TYPE_P(sa) != IS_ARRAY) {
            continue;
        }
        zval *next_v = zend_hash_index_find(Z_ARR_P(sa), (zend_ulong) type);
        if (!next_v) {
            continue;
        }
        zend_long next_state = Z_LVAL_P(Z_UNWRAP_P(next_v));

        php_phathom_glr_thread_t *nt =
            php_phathom_glr_thread_shift(&chart->arena, t, next_state, ti);

        size_t ksize = (size_t) nt->depth * sizeof(zend_long);
        bool inserted;
        php_phathom_hash_slot(
            &seen,
            php_phathom_hash_key_binary(nt->states, ksize),
            &inserted);
        if (inserted) {
            php_phathom_glr_worklist_push(shifted, nt);
        }
    }
    zend_arena_destroy(arena);
} /* }}} */

/* {{{ scan */
static bool
php_phathom_glr_chart_scan(
    php_phathom_glr_chart_t *chart,
    zval                    *expected,
    zend_long               *ti_out)
{
    zval args[5], retval;

    ZVAL_OBJ(&args[0],  chart->buffer.object);
    ZVAL_LONG(&args[1], chart->position);
    ZVAL_COPY(&args[2], expected);
    ZVAL_STR(&args[3],  chart->grammar.token);
    ZVAL_ARR(&args[4],  chart->grammar.literals);

    ZVAL_NEW_REF(&args[1], &args[1]);
    {
        ZVAL_UNDEF(&retval);
        zend_call_known_function(
            chart->grammar.scanner,
            chart->grammar.lexer,
            chart->grammar.lexer->ce,
            &retval,
                chart->grammar.literals == &zend_empty_array ?
                    4 : 5,
            args, NULL);
        chart->position =
            Z_LVAL_P(Z_UNWRAP_P(&args[1]));
    }
    zval_ptr_dtor(&args[2]);
    zval_ptr_dtor(&args[1]);

    if (Z_TYPE(retval) == IS_NULL || Z_TYPE(retval) == IS_UNDEF) {
        zval_ptr_dtor(&retval);
        return false;
    }

    *ti_out = (zend_long) zend_hash_num_elements(&chart->tokens);
    zend_hash_next_index_insert(&chart->tokens, &retval);
    return true;
} /* }}} */

/* {{{ construct */
static void
php_phathom_glr_chart_construct(
    php_phathom_t           *phathom,
    php_phathom_glr_chart_t *chart)
{
    php_phathom_grammar_fetch(phathom, &chart->grammar);
    php_phathom_buffer_fetch(phathom, &chart->buffer);

    chart->start = chart->grammar.start;

    zend_object *automaton = php_phathom_engine_automaton(chart->grammar.engine);

    zend_long accept = php_phathom_glr_automaton_accept(automaton);

    /* Initial thread: states=[INITIAL=0], nodes=[NULL sentinel], pos=0 */
    zend_long               init_state = 0; /* Automaton::INITIAL */
    php_phathom_glr_slot_t  init_slot  = {
        .kind = PHP_PHATHOM_GLR_SLOT_NULL 
    };
    php_phathom_glr_thread_t *init =
        php_phathom_glr_thread_new(
            &chart->arena, &init_state, &init_slot, 1, 0);

    php_phathom_glr_worklist_t cur = {0};
    php_phathom_glr_worklist_push(&cur, init);

    while (true) {
        /* reduce to fixed point */
        php_phathom_glr_worklist_t reduced = {0};
        php_phathom_glr_chart_reduce(
            phathom, chart, automaton,
            cur.data, cur.used, &reduced);
        php_phathom_glr_worklist_free(&cur);
        cur = reduced;

        if (cur.used == 0) {
            break;
        }

        /* collect expected terminals */
        zval expected;
        php_phathom_glr_chart_expected(
            automaton, cur.data, cur.used, &expected);

        if (!zend_hash_num_elements(Z_ARR(expected))) {
            zval_ptr_dtor(&expected);
            break;
        }

        /* scan next token */
        zend_long ti;
        bool scanned =
            php_phathom_glr_chart_scan(chart, &expected, &ti);
        zval_ptr_dtor(&expected);

        if (!scanned) {
            break;
        }

        zval *tok = zend_hash_index_find(&chart->tokens, (zend_ulong) ti);
        zend_long type = php_phathom_token_type(Z_OBJ_P(tok));

        /* shift */
        php_phathom_glr_worklist_t shifted = {0};
        php_phathom_glr_chart_shift(
            chart, automaton, cur.data, cur.used, type, ti, &shifted);
        php_phathom_glr_worklist_free(&cur);
        cur = shifted;
    }

    /* Final scanner drain call with empty expected */
    {
        zval args[5], retval;
        ZVAL_OBJ(&args[0], chart->buffer.object);
        ZVAL_LONG(&args[1], chart->position);
        array_init(&args[2]); /* [] */
        ZVAL_STR(&args[3],  chart->grammar.token);
        ZVAL_ARR(&args[4],  chart->grammar.literals);

        ZVAL_NEW_REF(&args[1], &args[1]);
        {
            ZVAL_UNDEF(&retval);
            zend_call_known_function(
                chart->grammar.scanner,
                chart->grammar.lexer,
                chart->grammar.lexer->ce,
                &retval,
                    chart->grammar.literals == &zend_empty_array ?
                        4 : 5,
                args, NULL);
            zval_ptr_dtor(&retval);
        }
        zval_ptr_dtor(&args[1]);
        zval_ptr_dtor(&args[2]);
    }

    /* Collect accepting threads */
    php_phathom_glr_worklist_t accepting = {0};
    for (zend_long i = 0; i < cur.used; i++) {
        php_phathom_glr_thread_t *t = cur.data[i];
        if (t->states[t->depth - 1] == accept) {
            php_phathom_glr_worklist_push(&accepting, t);
        }
    }
    php_phathom_glr_worklist_free(&cur);

    chart->nthreads = accepting.used;
    if (accepting.used > 0) {
        chart->threads = zend_arena_alloc(
            &chart->arena,
            (size_t) accepting.used * sizeof(*chart->threads));
        memcpy(chart->threads, accepting.data,
            (size_t) accepting.used * sizeof(*chart->threads));
    }
    php_phathom_glr_worklist_free(&accepting);

    chart->limit = (zend_long) zend_hash_num_elements(&chart->tokens);
} /* }}} */

PHP_FUNCTION(php_phathom_glr_chart___construct) {
    php_phathom_t phathom =
        php_phathom_fetch();
    php_phathom_glr_chart_t* chart =
        php_phathom_glr_chart_fetch(Z_OBJ(EX(This)));

    /* LCOV_EXCL_START */
    ZEND_PARSE_PARAMETERS_START(2, 2)
        Z_PARAM_OBJ_OF_CLASS(
            chart->grammar.object, phathom.class.grammar)
        Z_PARAM_OBJ_OF_CLASS(
            chart->buffer.object,  phathom.class.buffer)
    ZEND_PARSE_PARAMETERS_END();
    /* LCOV_EXCL_STOP */

    php_phathom_glr_chart_construct(&phathom, chart);
}

static zend_function_entry php_phathom_glr_chart_methods[] = {
    ZEND_NAMED_ME(__construct, PHP_FN(php_phathom_glr_chart___construct), arginfo_class_pharos_phathom_GLR_Chart___construct, ZEND_ACC_PUBLIC)
    PHP_FE_END
};

PHP_MINIT_FUNCTION(PHATHOM_GLR_CHART) {
    zend_class_entry ce;

    INIT_NS_CLASS_ENTRY(ce,
        "pharos\\phathom\\GLR", "Chart",
        php_phathom_glr_chart_methods);

    php_phathom_glr_chart_ce =
        zend_register_internal_class(&ce);
    php_phathom_glr_chart_ce->create_object =
        php_phathom_glr_chart_create;

    memcpy(
        &php_phathom_glr_chart_handlers,
        zend_get_std_object_handlers(),
        sizeof(zend_object_handlers));
    php_phathom_glr_chart_handlers.offset =
        XtOffsetOf(php_phathom_glr_chart_t, std);
    php_phathom_glr_chart_handlers.free_obj =
        php_phathom_glr_chart_free;

    return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(PHATHOM_GLR_CHART) {
    return SUCCESS;
}
