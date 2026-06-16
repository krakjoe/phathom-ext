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

zend_class_entry* php_phathom_chart_ce = NULL;
static zend_object_handlers php_phathom_chart_handlers;

/* {{{ table helpers */
typedef enum _php_phathom_chart_table_type_t {
    PHP_PHATHOM_CHART_TABLE_INDEX,
    PHP_PHATHOM_CHART_TABLE_STRING
} php_phathom_chart_table_type_t;

typedef struct _php_phathom_chart_table_key_t {
    php_phathom_chart_table_type_t type;
    union {
        zend_long    index;
        zend_string *string;
    } value;
} php_phathom_chart_table_key_t;

static zend_always_inline HashTable* php_phathom_chart_table(
        HashTable *root,
        php_phathom_chart_table_key_t key, dtor_func_t dtor) {
    HashTable *table = key.type == PHP_PHATHOM_CHART_TABLE_INDEX ?
        zend_hash_index_find_ptr(root, key.value.index) :
        zend_hash_find_ptr(root, key.value.string);
    if (!table) {
        HashTable tabled;
        zend_hash_init(&tabled, 8, 0, dtor, 0);
        return key.type == PHP_PHATHOM_CHART_TABLE_INDEX ?
            zend_hash_index_add_mem(root, key.value.index,
                &tabled, sizeof(HashTable)) :
            zend_hash_add_mem(root, key.value.string,
                &tabled, sizeof(HashTable));
    }
    return table;
} /* }}} */

zend_object* php_phathom_chart_create(zend_class_entry* type) {
    php_phathom_chart_t* chart = ecalloc(1,
        sizeof(php_phathom_chart_t) + zend_object_properties_size(type));

    zend_object_std_init(&chart->std, type);
    object_properties_init(&chart->std, type);

    chart->std.handlers =
        &php_phathom_chart_handlers;

    zend_hash_init(&chart->index,    8, NULL, php_phathom_table_destroy, 0);
    zend_hash_init(&chart->waiting,  8, NULL, php_phathom_table_destroy, 0);
    zend_hash_init(&chart->nullable, 8, NULL, php_phathom_table_destroy, 0);
    zend_hash_init(&chart->path,     8, NULL, php_phathom_table_destroy, 0);
    zend_hash_init(&chart->tokens,   8, NULL, ZVAL_PTR_DTOR, 0);

    return &chart->std;
}

void php_phathom_chart_free(zend_object* std) {
    php_phathom_chart_t* chart =
        php_phathom_chart_fetch(std);

    zend_hash_destroy(&chart->index);
    zend_hash_destroy(&chart->waiting);
    zend_hash_destroy(&chart->nullable);
    zend_hash_destroy(&chart->path);
    zend_hash_destroy(&chart->tokens);

    php_phathom_grammar_free(&chart->grammar);
    php_phathom_buffer_free(&chart->buffer);
}

/* Forward declarations */
static void php_phathom_chart_add(php_phathom_chart_t*, zend_long, php_phathom_item_t*);
static void php_phathom_chart_drain(php_phathom_chart_t*, zend_long, zend_object*, php_phathom_item_t*);
static void php_phathom_chart_complete(php_phathom_chart_t*, zend_long, php_phathom_item_t*);
static void php_phathom_chart_predict(php_phathom_chart_t*, zend_long, zend_object*);
static bool php_phathom_chart_scan(php_phathom_chart_t*, zend_long, HashTable*);

/* {{{ index */
static void php_phathom_chart_index_free(zval *zv) {
    php_phathom_item_t *item = Z_PTR_P(zv);
    if (item->backs.path) {
        efree(item->backs.path);
    }
    efree(item);
}

static zend_always_inline php_phathom_item_t* php_phathom_chart_index(
    php_phathom_chart_t* chart, zend_long position, php_phathom_item_t *item, bool *isset) {
    HashTable *index;

    /* index[pos] */
    index = php_phathom_chart_table(
        &chart->index, 
        (php_phathom_chart_table_key_t){
            .type = PHP_PHATHOM_CHART_TABLE_INDEX,
            .value = {
                .index = position
            }
        }, php_phathom_table_destroy);

    /* index[pos][rule] */
    index = php_phathom_chart_table(
        index,
        (php_phathom_chart_table_key_t){
            .type = PHP_PHATHOM_CHART_TABLE_STRING,
            .value = {
                .string = item->rule,
            }
        }, php_phathom_table_destroy);

    /* index[pos][rule][alt] */
    index = php_phathom_chart_table(
        index,
        (php_phathom_chart_table_key_t){
            .type = PHP_PHATHOM_CHART_TABLE_INDEX,
            .value = {
                .index = item->alt,
            }
        }, php_phathom_table_destroy);

    /* index[pos][rule][alt][dot] */
    index = php_phathom_chart_table(
        index,
        (php_phathom_chart_table_key_t){
            .type = PHP_PHATHOM_CHART_TABLE_INDEX,
            .value = {
                .index = item->dot,
            }
        }, php_phathom_chart_index_free);

    /* index[pos][rule][alt][dot][origin] */
    php_phathom_item_t *slot =
        zend_hash_index_find_ptr(
            index, item->origin);

    if (UNEXPECTED(slot)) {
        *isset = true;
        return slot;
    }

    *isset = false;
    slot = emalloc(
        sizeof(php_phathom_item_t));
    zend_hash_index_add_ptr(
        index, item->origin, slot);
    return slot;
}

/* Returns true if (position, rule, alt, dot, origin) is already in the index.
   Callers use this to skip bpath allocation before calling chart_add. */
static zend_always_inline bool php_phathom_chart_indexed(
    php_phathom_chart_t *chart, zend_long position, php_phathom_item_t *item) {
    HashTable *index;
    if (!(index = zend_hash_index_find_ptr(&chart->index, position)))
        return false;
    if (!(index = zend_hash_find_ptr(index, item->rule)))
        return false;
    if (!(index = zend_hash_index_find_ptr(index, item->alt)))
        return false;
    if (!(index = zend_hash_index_find_ptr(index, item->dot)))
        return false;
    if (!(index = zend_hash_index_find_ptr(index, item->origin)))
        return false;
    return true;
} /* }}}*/

/* {{{ path */
static zend_always_inline void php_phathom_chart_path(php_phathom_chart_t *chart, zend_long position, php_phathom_item_t *item) {
    /* path[position][] = item; */
    HashTable *path = php_phathom_chart_table(
        &chart->path, 
        (php_phathom_chart_table_key_t) {
            .type = PHP_PHATHOM_CHART_TABLE_INDEX,
            .value = {
                .index = position,
            },
        },
        NULL);
    zend_hash_next_index_insert_ptr(path, item);
} /* }}} */

/* {{{ nullable */
static zend_always_inline void php_phathom_chart_nullable(php_phathom_chart_t *chart, zend_long position, php_phathom_item_t *item) {
    /* nullable[position][item->rule][] = item */
    HashTable *nullable = php_phathom_chart_table(
        &chart->nullable, 
        (php_phathom_chart_table_key_t) {
            .type = PHP_PHATHOM_CHART_TABLE_INDEX,
            .value = {
                .index = position,
            },
        }, php_phathom_table_destroy);

    nullable = php_phathom_chart_table(
        nullable,
        (php_phathom_chart_table_key_t) {
            .type = PHP_PHATHOM_CHART_TABLE_STRING,
            .value = {
                .string = item->rule,
            },
        },
        NULL);

    zend_hash_next_index_insert_ptr(nullable, item);
} /* }}} */

/* {{{ waiting */
static zend_always_inline void php_phathom_chart_waiting(php_phathom_chart_t *chart, zend_long position, zend_string *name, php_phathom_item_t *item) {
    /* waiting[position][name][] = item; */
    HashTable *waiting = php_phathom_chart_table(&chart->waiting,
        (php_phathom_chart_table_key_t) {
            .type = PHP_PHATHOM_CHART_TABLE_INDEX,
            .value = {
                .index = position,
            },
        }, php_phathom_table_destroy);
    waiting = php_phathom_chart_table(waiting,
        (php_phathom_chart_table_key_t) {
            .type = PHP_PHATHOM_CHART_TABLE_STRING,
            .value = {
                .string = name,
            },
        }, NULL);
    zend_hash_next_index_insert_ptr(waiting, item);
} /* }}} */

/* {{{ add */
static zend_always_inline void
    php_phathom_chart_append(
        php_phathom_backs_t *backs,
        php_phathom_back_t back) {
    if (backs->used == backs->limit) {
        backs->limit = backs->limit ? backs->limit * 2 : 4;
        backs->path  = erealloc(
            backs->path,
            sizeof(php_phathom_back_t) * backs->limit);
    }
    backs->path[backs->used++] = back;
}

static void php_phathom_chart_add(
    php_phathom_chart_t* chart, zend_long position, php_phathom_item_t *item) {
    bool isset;
    php_phathom_item_t *slot =
        php_phathom_chart_index(
            chart, position, item, &isset);
    if (isset) {
        if (item->backs.used == 0) {
            return;
        }
        for (uint64_t b = 0; b < item->backs.used; b++) {
            php_phathom_chart_append(
                &slot->backs, item->backs.path[b]);
        }
        if (item->backs.path) {
            efree(item->backs.path);
        }
        return;
    }

    *slot     = *item;
    slot->pos = position;

    php_phathom_chart_path(chart, position, slot);

    /* $dotted = $item->alternative->symbols[$item->dot] ?? null */
    zend_array *symbols =
        php_phathom_alternative_symbols(
            slot->alternative);
    zval *dotted =
        zend_hash_index_find(
            symbols, slot->dot);

    if (!dotted) {
        /* dotted === null */
        if (slot->origin == position) {
            php_phathom_chart_nullable(chart, position, slot);
            php_phathom_chart_complete(chart, position, slot);
        }
        return;
    }

    php_phathom_chart_waiting(chart, position,
        php_phathom_symbol_name(
            Z_OBJ_P(Z_UNWRAP_P(dotted))), slot);
    php_phathom_chart_drain(chart, position,
        Z_OBJ_P(Z_UNWRAP_P(dotted)), slot);
} /* }}} */

/* {{{ drain */
static void php_phathom_chart_drain(
    php_phathom_chart_t* chart, zend_long position, zend_object *dotted, php_phathom_item_t* item) {

    zend_string *name =
        php_phathom_symbol_name(dotted);

    HashTable *index =
        zend_hash_index_find_ptr(
            &chart->nullable, position);
    if (!index) {
        return;
    }

    HashTable *rule =
        zend_hash_find_ptr(
            index, name);
    if (!rule) {
        return;
    }

    /* Snapshot item pointers before any recursive chart_add calls can
       add new entries to nullable[position][name], causing arData to be
       reallocated and invalidating any in-flight FOREACH iterator. */
    zend_long count = (zend_long) zend_hash_num_elements(rule);
    php_phathom_item_t **snap = emalloc(count * sizeof(php_phathom_item_t*));
    zend_long n = 0;
    php_phathom_item_t *s;
    ZEND_HASH_FOREACH_PTR(rule, s) { snap[n++] = s; } ZEND_HASH_FOREACH_END();

    for (zend_long k = 0; k < n; k++) {
        php_phathom_item_t *completed = snap[k];

        php_phathom_item_t draining = {
            .rule        = item->rule,
            .alt         = item->alt,
            .dot         = item->dot + 1,
            .origin      = item->origin,
            .backs       = { .path = NULL, .used = 0, .limit = 0 },
            .alternative = item->alternative,
        };
        php_phathom_back_t *bpath = emalloc(sizeof(php_phathom_back_t));
        bpath[0] = (php_phathom_back_t){ 
            .prev = item,
            .child = completed,
            .token = -1 
        };
        draining.backs = (php_phathom_backs_t){ 
            .path = bpath,
            .used = 1,
            .limit = 1 
        };
        php_phathom_chart_add(chart, position, &draining);
    }
    efree(snap);
} /* }}} */

/* {{{ complete */
static void php_phathom_chart_complete(
    php_phathom_chart_t* chart, zend_long position, php_phathom_item_t *item) {

    HashTable *waiting =
        zend_hash_index_find_ptr(
            &chart->waiting, item->origin);
    if (!waiting) {
        return;
    }

    HashTable *rule =
        zend_hash_find_ptr(
            waiting, item->rule);
    if (!rule) {
        return;
    }

    /* Snapshot item pointers before any recursive chart_add calls can
       add new entries to waiting[origin][rule], causing arData to be
       reallocated and invalidating any in-flight FOREACH iterator. */
    zend_long count = (zend_long) zend_hash_num_elements(rule);
    php_phathom_item_t **snap = emalloc(count * sizeof(php_phathom_item_t*));
    zend_long n = 0;
    php_phathom_item_t *s;
    ZEND_HASH_FOREACH_PTR(rule, s) { snap[n++] = s; } ZEND_HASH_FOREACH_END();
    
    for (zend_long k = 0; k < n; k++) {
        php_phathom_item_t *complete = snap[k];

        php_phathom_item_t add = {
            .rule        = complete->rule,
            .alt         = complete->alt,
            .dot         = complete->dot + 1,
            .origin      = complete->origin,
            .backs       = { 
                .path = NULL,
                .used = 0,
                .limit = 0 
            },
            .alternative = complete->alternative,
        };
        php_phathom_back_t *bpath = emalloc(sizeof(php_phathom_back_t));
        bpath[0] = (php_phathom_back_t){ 
            .prev = complete,
            .child = item,
            .token = -1 
        };
        add.backs = (php_phathom_backs_t){ 
            .path = bpath,
            .used = 1,
            .limit = 1 
        };
        php_phathom_chart_add(chart, position, &add);
    }
    efree(snap);
} /* }}} */

/* {{{ predict */
static void php_phathom_chart_predict(
    php_phathom_chart_t* chart, zend_long position, zend_object *dotted) {

    zend_string *name = php_phathom_symbol_name(dotted);
    zval        *rule = zend_hash_find_deref(chart->grammar.rules, name);

    if (!rule) {
        return;
    }

    zend_long    aid;
    zval        *alt;
    ZEND_HASH_FOREACH_NUM_KEY_VAL(Z_ARRVAL_P(rule), aid, alt) {
        ZVAL_DEREF(alt);
        php_phathom_item_t item = {
            .rule        = name,
            .alt         = aid,
            .dot         = 0,
            .origin      = position,
            .backs       = { 
                .path = NULL,
                .used = 0,
                .limit = 0 
            },
            .alternative = Z_OBJ_P(alt),
        };
        if (!php_phathom_chart_indexed(chart, position, &item)) {
            php_phathom_chart_add(chart, position, &item);
        }
    } ZEND_HASH_FOREACH_END();
} /* }}} */

/* {{{ scan */
static bool php_phathom_chart_scan(
    php_phathom_chart_t* chart, zend_long position, HashTable *expected) {
    zval args[4], retval;

    ZVAL_OBJ(&args[0],  chart->buffer.object);
    ZVAL_LONG(&args[1], chart->position);
    ZVAL_ARR(&args[2],  expected);
    ZVAL_STR(&args[3],  chart->grammar.token);

    ZVAL_NEW_REF(&args[1], &args[1]);
    GC_ADDREF(expected);
    {
        ZVAL_UNDEF(&retval);
        zend_call_known_function(
            chart->grammar.scanner,
            chart->grammar.lexer,
            chart->grammar.lexer->ce,
            &retval, 4, args, NULL);
        chart->position =
            Z_LVAL_P(Z_UNWRAP_P(&args[1]));
    }
    GC_DELREF(expected);
    zval_ptr_dtor(&args[1]);

    if (Z_TYPE(retval) == IS_NULL) {
        zval_ptr_dtor(&retval);
        return false;
    }

    zend_long ti =
        (zend_long)
            zend_hash_num_elements(
                &chart->tokens);
    /* capture the type of the token that was actually scanned */
    zend_long token_type = php_phathom_token_type(Z_OBJ(retval));
    zend_hash_next_index_insert(
        &chart->tokens, &retval);

    /* ensure path[position+1] exists */
    php_phathom_chart_table(&chart->path,
        (php_phathom_chart_table_key_t){
            .type = PHP_PHATHOM_CHART_TABLE_INDEX,
            .value = {
                .index = position + 1,
            },
        }, NULL);

    /* advance items in path[position] whose terminal matches */
    HashTable *path = zend_hash_index_find_ptr(&chart->path, position);
    if (path) {
        php_phathom_item_t *item;
        ZEND_HASH_FOREACH_PTR(path, item) {
            zend_array *symbols = php_phathom_alternative_symbols(item->alternative);
            zval       *dotted  = zend_hash_index_find(symbols, item->dot);

            if (!dotted) {
                continue;
            }

            zend_long terminal = php_phathom_symbol_terminal(Z_OBJ_P(Z_UNWRAP_P(dotted)));
            if (terminal != token_type) {
                continue;
            }

            php_phathom_item_t add = {
                .rule        = item->rule,
                .alt         = item->alt,
                .dot         = item->dot + 1,
                .origin      = item->origin,
                .backs       = { 
                    .path = NULL,
                    .used = 0,
                    .limit = 0 
                },
                .alternative = item->alternative,
            };

            if (php_phathom_chart_indexed(chart, position + 1, &add)) {
                continue;
            }

            php_phathom_back_t *bpath = emalloc(sizeof(php_phathom_back_t));
            bpath[0] = (php_phathom_back_t){
                .prev = item,
                .child = NULL,
                .token = ti 
            };
            add.backs = (php_phathom_backs_t){ 
                .path = bpath,
                .used = 1,
                .limit = 1 
            };

            php_phathom_chart_add(chart, position + 1, &add);
        } ZEND_HASH_FOREACH_END();
    }

    return true;
} /* }}} */

/* {{{ start */
static zend_always_inline void php_phathom_chart_start(php_phathom_t* phathom, php_phathom_chart_t* chart) {
    zval *alts = zend_hash_find(chart->grammar.rules, chart->grammar.start);
    if (!alts) {
        return;
    }

    zend_long    aid;
    zend_string *unused;
    zval        *alt;
    ZEND_HASH_FOREACH_KEY_VAL(Z_ARRVAL_P(Z_UNWRAP_P(alts)), aid, unused, alt) {
        php_phathom_item_t item = {
            .alt         = aid,
            .dot         = 0,
            .origin      = 0,
            .pos         = 0,
            .backs       = { 
                .path = NULL,
                .used = 0,
                .limit = 0 
            },
            .rule        = chart->grammar.start,
            .alternative = Z_OBJ_P(Z_UNWRAP_P(alt)),
        };
        php_phathom_chart_add(chart, 0, &item);
    } ZEND_HASH_FOREACH_END();
} /* }}} */

/* {{{ construct */
static zend_always_inline void php_phathom_chart_construct(php_phathom_t* phathom, php_phathom_chart_t* chart) {
    php_phathom_grammar_fetch(phathom, &chart->grammar);
    php_phathom_buffer_fetch(phathom, &chart->buffer);

    php_phathom_chart_start(phathom, chart);

    for (zend_long i = 0; ; i++) {
        if (!zend_hash_index_find_ptr(&chart->path, i)) break;

        zend_long j = 0;
        HashTable expected;
        zend_hash_init(&expected, 8, NULL, NULL, 0);

        /* Re-fetch path_i on every step: predict/complete/drain may add new
           positions to chart->path, causing it to rehash and invalidating
           any previously obtained pointer. */
        HashTable *path_i;
        while ((path_i = zend_hash_index_find_ptr(&chart->path, i)) &&
               j < (zend_long) zend_hash_num_elements(path_i)) {
            php_phathom_item_t *item    = zend_hash_index_find_ptr(path_i, j++);
            zend_array         *symbols = php_phathom_alternative_symbols(item->alternative);
            zval               *dotted  = zend_hash_index_find(symbols, item->dot);

            if (!dotted) {
                if (i == item->origin) {
                    continue; /* skip nullable */
                }
                php_phathom_chart_complete(chart, i, item);
            } else {
                zend_long  terminal = php_phathom_symbol_terminal(
                    Z_OBJ_P(Z_UNWRAP_P(dotted)));
                if (terminal != -1) {
                    zval t;
                    ZVAL_TRUE(&t);
                    zend_hash_index_add(
                        &expected, terminal, &t);
                } else {
                    php_phathom_chart_predict(chart, i, Z_OBJ_P(Z_UNWRAP_P(dotted)));
                }
            }
        }

        if (!zend_hash_num_elements(&expected)) {
            zend_hash_destroy(&expected);
            break;
        }

        if (!php_phathom_chart_scan(chart, i, &expected)) {
            zend_hash_destroy(&expected);
            break;
        }
        zend_hash_destroy(&expected);
    }

    {
        zval args[4], retval;
        ZVAL_OBJ(&args[0],  chart->buffer.object);
        ZVAL_LONG(&args[1], chart->position);
        array_init(&args[2]); /* [] */
        ZVAL_STR(&args[3],  chart->grammar.token);

        ZVAL_NEW_REF(&args[1], &args[1]);
        {
            ZVAL_UNDEF(&retval);
            zend_call_known_function(
                chart->grammar.scanner,
                chart->grammar.lexer,
                chart->grammar.lexer->ce,
                &retval, 4, args, NULL);
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
    php_phathom_chart_t* chart =
        php_phathom_chart_fetch(Z_OBJ(EX(This)));

    /* LCOV_EXCL_START */
    ZEND_PARSE_PARAMETERS_START(2, 2)
        Z_PARAM_OBJ_OF_CLASS(
            chart->grammar.object, phathom.class.grammar)
        Z_PARAM_OBJ_OF_CLASS(
            chart->buffer.object,   phathom.class.buffer)
    ZEND_PARSE_PARAMETERS_END();
    /* LCOV_EXCL_STOP */

    php_phathom_chart_construct(&phathom, chart);
} /* }}} */

/* {{{ internals */
zend_function_entry php_phathom_chart_methods[] = {
    PHP_ME(Chart, __construct, arginfo_class_pharos_phathom_Earley_Chart___construct, ZEND_ACC_PUBLIC)
    PHP_FE_END
};

PHP_MINIT_FUNCTION(PHATHOM_CHART) {
    zend_class_entry ce;

    INIT_NS_CLASS_ENTRY(ce,
        "pharos\\phathom\\Earley", "Chart",
        php_phathom_chart_methods);

    php_phathom_chart_ce =
        zend_register_internal_class(&ce);
    php_phathom_chart_ce->create_object =
        php_phathom_chart_create;

    memcpy(
        &php_phathom_chart_handlers,
        zend_get_std_object_handlers(),
        sizeof(zend_object_handlers));
    php_phathom_chart_handlers.offset =
        XtOffsetOf(php_phathom_chart_t, std);
    php_phathom_chart_handlers.free_obj =
        php_phathom_chart_free;

    return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(PHATHOM_CHART) {
    return SUCCESS;
} /* }}} */