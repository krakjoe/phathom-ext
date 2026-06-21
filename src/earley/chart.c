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
#include "symbol.h"
#include "alternative.h"
#include "token.h"

zend_class_entry* php_phathom_earley_chart_ce = NULL;
static zend_object_handlers php_phathom_earley_chart_handlers;

/* {{{ forward */
static void php_phathom_earley_chart_add(php_phathom_earley_chart_t*, zend_long, php_phathom_earley_item_t*);
static void php_phathom_earley_chart_drain(php_phathom_earley_chart_t*, zend_long, zend_object*, php_phathom_earley_item_t*);
static void php_phathom_earley_chart_complete(php_phathom_earley_chart_t*, zend_long, php_phathom_earley_item_t*);
static void php_phathom_earley_chart_predict(php_phathom_earley_chart_t*, zend_long, zend_object*);
static bool php_phathom_earley_chart_scan(php_phathom_earley_chart_t*, zend_long, zval*); /* }}} */

/* {{{ table */
static zend_always_inline php_phathom_hash_t *php_phathom_earley_chart_table(
    php_phathom_earley_chart_t *chart, php_phathom_hash_t *root, php_phathom_hash_key_t key, uint32_t size) {
    bool inserted;
    php_phathom_hash_t **slot = (php_phathom_hash_t **)
        php_phathom_hash_slot(root, key, &inserted);

    if (EXPECTED(!inserted)) {
        return *slot;
    }

    *slot = zend_arena_alloc(&chart->arena, sizeof(php_phathom_hash_t));
    php_phathom_hash_init(*slot, &chart->arena, size);
    return *slot;
} /* }}} */

/* {{{ internals */
zend_object* php_phathom_earley_chart_create(zend_class_entry* type) {
    php_phathom_earley_chart_t* chart = ecalloc(1,
        sizeof(php_phathom_earley_chart_t) + zend_object_properties_size(type));

    zend_object_std_init(&chart->std, type);
    object_properties_init(&chart->std, type);

    chart->std.handlers =
        &php_phathom_earley_chart_handlers;

    chart->arena =
        zend_arena_create(1024 * 64);

    php_phathom_hash_init(&chart->index,    &chart->arena, 2048);
    php_phathom_hash_init(&chart->waiting,  &chart->arena, 8);
    php_phathom_hash_init(&chart->nullable, &chart->arena, 8);
    php_phathom_hash_init(&chart->path,     &chart->arena, 8);

    zend_hash_init(&chart->tokens,   8, NULL, ZVAL_PTR_DTOR, 0);

    return &chart->std;
}

void php_phathom_earley_chart_free(zend_object* std) {
    php_phathom_earley_chart_t* chart =
        php_phathom_earley_chart_fetch(std);

    if (chart->arena) {
        zend_arena_destroy(chart->arena);
    }

    zend_hash_destroy(&chart->tokens);

    php_phathom_grammar_free(&chart->grammar);
    php_phathom_buffer_free(&chart->buffer);

    zend_object_std_dtor(std);
} /* }}} */

/* {{{ index */
typedef struct _php_phathom_earley_chart_index_key_t {
    uint64_t position;
    uint64_t rule;
    uint64_t alt;
    uint64_t dot;
    uint64_t origin;
} php_phathom_earley_chart_index_key_t;

static zend_always_inline php_phathom_earley_item_t* php_phathom_earley_chart_index(
    php_phathom_earley_chart_t* chart, zend_long position, php_phathom_earley_item_t *item, bool *isset) {
    php_phathom_earley_chart_index_key_t key = {
        .position = (uint64_t) position,
        .rule     = (uint64_t)(uintptr_t) item->rule,
        .alt      = (uint64_t) item->alt,
        .dot      = (uint64_t) item->dot,
        .origin   = (uint64_t) item->origin,
    };

    bool inserted;
    php_phathom_earley_item_t **slot = (php_phathom_earley_item_t **)
        php_phathom_hash_slot(
            &chart->index,
            php_phathom_hash_key_binary(&key, sizeof(key)),
            &inserted);

    if (!inserted) {
        *isset = true;
        return *slot;
    }

    *isset = false;
    *slot  = zend_arena_alloc(
        &chart->arena,
        sizeof(php_phathom_earley_item_t));
    return *slot;
} /* }}}*/

/* {{{ path */
static zend_always_inline void php_phathom_earley_chart_path(php_phathom_earley_chart_t *chart, zend_long position, php_phathom_earley_item_t *item) {
    php_phathom_hash_t *path =
        php_phathom_earley_chart_table(
            chart,
            &chart->path,
            php_phathom_hash_key_index(
                (uint64_t) position),
            32);
    /* path[position][] = item; */
    php_phathom_hash_append(path, item);
} /* }}} */

/* {{{ nullable */
typedef struct _php_phathom_earley_chart_nullable_key_t {
    uint64_t position;
    uint64_t rule;
} php_phathom_earley_chart_nullable_key_t;

static zend_always_inline void php_phathom_earley_chart_nullable(
    php_phathom_earley_chart_t *chart, zend_long position, php_phathom_earley_item_t *item) {
    php_phathom_earley_chart_nullable_key_t key = {
        .position = (uint64_t) position,
        .rule     = (uint64_t)(uintptr_t) item->rule,
    };
    php_phathom_hash_t *nullable =
        php_phathom_earley_chart_table(
            chart,
            &chart->nullable,
            php_phathom_hash_key_binary(
                &key, sizeof(key)),
            8);
    /* nullable[{position, rule}][] = item */
    php_phathom_hash_append(nullable, item);
} /* }}} */

/* {{{ waiting */
typedef struct _php_phathom_earley_chart_waiting_key_t {
    uint64_t position;
    uint64_t rule;
} php_phathom_earley_chart_waiting_key_t;

static zend_always_inline void php_phathom_earley_chart_waiting(
    php_phathom_earley_chart_t *chart, zend_long position, zend_string *name, php_phathom_earley_item_t *item) {
    php_phathom_earley_chart_waiting_key_t key = {
        .position = (uint64_t) position,
        .rule     = (uint64_t)(uintptr_t) name,
    };
    php_phathom_hash_t *waiting =
        php_phathom_earley_chart_table(
            chart,
            &chart->waiting,
            php_phathom_hash_key_binary(
                &key, sizeof(key)),
            16);
    /* waiting[{position, name}][] = item; */
    php_phathom_hash_append(waiting, item);
} /* }}} */

/* {{{ add */
static zend_always_inline void
    php_phathom_earley_chart_append(
        php_phathom_earley_chart_t *chart,
        php_phathom_earley_backs_t *backs,
        php_phathom_earley_back_t  *back) {
    if (backs->limit == 0) {
        /* initial slot unused */
        backs->limit = 1;
        backs->one   = *back;
        backs->used  = 1;
        return;
    }
    if (backs->used == backs->limit) {
        /* at limit (re)allocate */
        uint64_t limit =
            backs->limit > 1 ?
                backs->limit * 2 : /* double slots */
                4; /* initially allocate 4 slots */
        php_phathom_earley_back_t *grown = zend_arena_alloc(
            &chart->arena,
            sizeof(php_phathom_earley_back_t) * limit);
        if (backs->used) {
            memcpy(grown,
                php_phathom_earley_chart_back_fetch(backs, 0),
                sizeof(php_phathom_earley_back_t) * backs->used);
        }
        backs->many  = grown;
        backs->limit = limit;
    }
    backs->many[backs->used++] = *back;
}

static void php_phathom_earley_chart_add(
    php_phathom_earley_chart_t* chart, zend_long position, php_phathom_earley_item_t *item) {
    bool isset;
    php_phathom_earley_item_t *slot =
        php_phathom_earley_chart_index(
            chart, position, item, &isset);
    if (isset) {
        if (item->backs.used == 0) {
            return;
        }
        for (uint64_t b = 0; b < item->backs.used; b++) {
            php_phathom_earley_chart_append(
                chart,
                &slot->backs,
                php_phathom_earley_chart_back_fetch(
                    &item->backs, b));
        }
        return;
    }

    *slot     = *item;
    slot->pos = position;

    php_phathom_earley_chart_path(chart, position, slot);

    zend_object *dotted =
        php_phathom_alternative_symbol(
            slot->alternative, slot->dot);

    if (!dotted) {
        if (slot->origin == position) {
            php_phathom_earley_chart_nullable(chart, position, slot);
            php_phathom_earley_chart_complete(chart, position, slot);
        }
        return;
    }

    php_phathom_earley_chart_waiting(chart, position,
        php_phathom_symbol_name(dotted), slot);
    php_phathom_earley_chart_drain(
        chart, position, dotted, slot);
} /* }}} */

/* {{{ drain */
static void php_phathom_earley_chart_drain(
    php_phathom_earley_chart_t* chart, zend_long position, zend_object *dotted, php_phathom_earley_item_t* item) {

    php_phathom_earley_chart_nullable_key_t key = {
        .position = (uint64_t) position,
        .rule     = (uint64_t)(uintptr_t)
            php_phathom_symbol_name(dotted),
    };

    /* nullable[{position, rule}] */
    php_phathom_hash_t *nullable =
        php_phathom_hash_find_binary(
            &chart->nullable, &key, sizeof(key));

    if (!nullable) {
        return;
    }

    PHP_PHATHOM_HASH_FOREACH_CURRENT(nullable, php_phathom_earley_item_t, completed) {
        php_phathom_earley_item_t draining = {
            .rule        = item->rule,
            .alt         = item->alt,
            .dot         = item->dot + 1,
            .origin      = item->origin,
            .backs       = { 
                .used  = 1,
                .limit = 1,
                .one = {
                    .prev = item,
                    .child = completed,
                    .token = -1,
                },
            },
            .alternative = item->alternative,
        };
        php_phathom_earley_chart_add(chart, position, &draining);
    } PHP_PHATHOM_HASH_FOREACH_END();
} /* }}} */

/* {{{ complete */
static void php_phathom_earley_chart_complete(
    php_phathom_earley_chart_t* chart, zend_long position, php_phathom_earley_item_t *item) {

    php_phathom_earley_chart_waiting_key_t wkey = {
        .position = (uint64_t) item->origin,
        .rule     = (uint64_t)(uintptr_t) item->rule,
    };

    /* waiting[{position, rule}] */
    php_phathom_hash_t *waiting =
        php_phathom_hash_find_binary(
            &chart->waiting, &wkey, sizeof(wkey));
    if (!waiting) {
        return;
    }

    PHP_PHATHOM_HASH_FOREACH_CURRENT(waiting, php_phathom_earley_item_t, complete) {
        php_phathom_earley_item_t add = {
            .rule        = complete->rule,
            .alt         = complete->alt,
            .dot         = complete->dot + 1,
            .origin      = complete->origin,
            .backs       = { 
                .used  = 1,
                .limit = 1,
                .one = {
                    .prev = complete,
                    .child = item,
                    .token = -1,
                },
            },
            .alternative = complete->alternative,
        };
        php_phathom_earley_chart_add(chart, position, &add);
    } PHP_PHATHOM_HASH_FOREACH_END();
} /* }}} */

/* {{{ predict */
static void php_phathom_earley_chart_predict(
    php_phathom_earley_chart_t* chart, zend_long position, zend_object *dotted) {

    zend_string *name = php_phathom_symbol_name(dotted);
    zval        *rule = zend_hash_find(chart->grammar.rules, name);

    if (!rule) {
        return;
    }

    zend_long    aid;
    zval        *alt;
    ZEND_HASH_FOREACH_NUM_KEY_VAL(Z_ARRVAL_P(Z_UNWRAP_P(rule)), aid, alt) {
        php_phathom_earley_item_t item = {
            .rule        = name,
            .alt         = aid,
            .dot         = 0,
            .origin      = position,
            .backs       = { 0 },
            .alternative = Z_OBJ_P(Z_UNWRAP_P(alt)),
        };
        php_phathom_earley_chart_add(chart, position, &item);
    } ZEND_HASH_FOREACH_END();
} /* }}} */

/* {{{ scan */
static bool php_phathom_earley_chart_scan(
    php_phathom_earley_chart_t* chart, zend_long position, zval *expected) {
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
                    4 : 5
            , args, NULL);
        chart->position =
            Z_LVAL_P(Z_UNWRAP_P(&args[1]));
    }
    zval_ptr_dtor(&args[2]);
    zval_ptr_dtor(&args[1]);

    if (Z_TYPE(retval) == IS_NULL || Z_TYPE(retval) == IS_UNDEF) {
        zval_ptr_dtor(&retval);
        return false;
    }

    zend_long ti =
        (zend_long)
            zend_hash_num_elements(
                &chart->tokens);
    zend_hash_next_index_insert(
        &chart->tokens, &retval);

    /* ensure path[position+1] exists */
    php_phathom_earley_chart_table(chart,
        &chart->path,
        php_phathom_hash_key_index((uint64_t) (position + 1)),
        32);

    /* advance items in path[position] whose terminal matches scanned token type */
    php_phathom_hash_t *path =
        php_phathom_hash_find_index(
            &chart->path, (uint64_t) position);
    zend_long type =
        php_phathom_token_type(Z_OBJ(retval));

    if (path) {
        PHP_PHATHOM_HASH_FOREACH_CURRENT(path, php_phathom_earley_item_t, item) {
            zend_object *dotted =
                php_phathom_alternative_symbol(
                    item->alternative, item->dot);

            if (!dotted) {
                continue;
            }

            if (type != php_phathom_symbol_terminal(dotted)) {
                continue;
            }

            php_phathom_earley_item_t add = {
                .rule        = item->rule,
                .alt         = item->alt,
                .dot         = item->dot + 1,
                .origin      = item->origin,
                .backs       = {
                    .used  = 1,
                    .limit = 1,
                    .one = {
                        .prev = item,
                        .child = NULL,
                        .token = ti,
                    },
                },
                .alternative = item->alternative,
            };
            php_phathom_earley_chart_add(chart, position + 1, &add);
        } PHP_PHATHOM_HASH_FOREACH_END();
    }

    return true;
} /* }}} */

/* {{{ start */
static zend_always_inline void php_phathom_earley_chart_start(php_phathom_t* phathom, php_phathom_earley_chart_t* chart) {
    zval *alts = zend_hash_find(chart->grammar.rules, chart->grammar.start);
    if (!alts) {
        return;
    }

    zend_long    aid;
    zend_string *unused;
    zval        *alt;
    ZEND_HASH_FOREACH_KEY_VAL(Z_ARRVAL_P(Z_UNWRAP_P(alts)), aid, unused, alt) {
        php_phathom_earley_item_t item = {
            .alt         = aid,
            .dot         = 0,
            .origin      = 0,
            .pos         = 0,
            .backs       = { 0 },
            .rule        = chart->grammar.start,
            .alternative = Z_OBJ_P(Z_UNWRAP_P(alt)),
        };
        php_phathom_earley_chart_add(chart, 0, &item);
    } ZEND_HASH_FOREACH_END();
} /* }}} */

/* {{{ construct */
static zend_always_inline void php_phathom_earley_chart_construct(php_phathom_t* phathom, php_phathom_earley_chart_t* chart) {
    php_phathom_grammar_fetch(phathom, &chart->grammar);
    php_phathom_buffer_fetch(phathom, &chart->buffer);

    php_phathom_earley_chart_start(phathom, chart);

    for (zend_long i = 0; ; i++) {
        php_phathom_hash_t *path;

        if (!(path = php_phathom_hash_find_index(&chart->path,  (uint64_t) i))) {
            break;
        }

        zval expected;
        array_init(&expected);

        PHP_PHATHOM_HASH_FOREACH_CONCURRENT(path, php_phathom_earley_item_t, item) {
            zend_object *dotted =
                php_phathom_alternative_symbol(
                    item->alternative, item->dot);

            if (!dotted) {
                if (i == item->origin) {
                    continue; /* skip nullable */
                }
                php_phathom_earley_chart_complete(chart, i, item);
            } else {
                zend_long  terminal = php_phathom_symbol_terminal(dotted);
                if (terminal != -1) {
                    add_index_bool(&expected, terminal, true);
                } else {
                    php_phathom_earley_chart_predict(chart, i, dotted);
                }
            }
        } PHP_PHATHOM_HASH_FOREACH_END();

        if (!zend_hash_num_elements(Z_ARRVAL(expected))) {
            zval_ptr_dtor(&expected);
            break;
        }

        if (!php_phathom_earley_chart_scan(chart, i, &expected)) {
            zval_ptr_dtor(&expected);
            break;
        }
        zval_ptr_dtor(&expected);
    }

    {
        zval args[5], retval;
        ZVAL_OBJ(&args[0],  chart->buffer.object);
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

    chart->limit = (zend_long) zend_hash_num_elements(&chart->tokens);
}

PHP_METHOD(Chart, __construct) {
    php_phathom_t phathom =
        php_phathom_fetch();
    php_phathom_earley_chart_t* chart =
        php_phathom_earley_chart_fetch(Z_OBJ(EX(This)));

    /* LCOV_EXCL_START */
    ZEND_PARSE_PARAMETERS_START(2, 2)
        Z_PARAM_OBJ_OF_CLASS(
            chart->grammar.object, phathom.class.grammar)
        Z_PARAM_OBJ_OF_CLASS(
            chart->buffer.object,   phathom.class.buffer)
    ZEND_PARSE_PARAMETERS_END();
    /* LCOV_EXCL_STOP */

    php_phathom_earley_chart_construct(&phathom, chart);
} /* }}} */

/* {{{ internals */
zend_function_entry php_phathom_earley_chart_methods[] = {
    PHP_ME(Chart, __construct, arginfo_class_pharos_phathom_Earley_Chart___construct, ZEND_ACC_PUBLIC)
    PHP_FE_END
};

PHP_MINIT_FUNCTION(PHATHOM_EARLEY_CHART) {
    zend_class_entry ce;

    INIT_NS_CLASS_ENTRY(ce,
        "pharos\\phathom\\Earley", "Chart",
        php_phathom_earley_chart_methods);

    php_phathom_earley_chart_ce =
        zend_register_internal_class(&ce);
    php_phathom_earley_chart_ce->create_object =
        php_phathom_earley_chart_create;

    memcpy(
        &php_phathom_earley_chart_handlers,
        zend_get_std_object_handlers(),
        sizeof(zend_object_handlers));
    php_phathom_earley_chart_handlers.offset =
        XtOffsetOf(php_phathom_earley_chart_t, std);
    php_phathom_earley_chart_handlers.free_obj =
        php_phathom_earley_chart_free;

    return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(PHATHOM_EARLEY_CHART) {
    return SUCCESS;
} /* }}} */