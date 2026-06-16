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
#include "zend_enum.h"

#include "evaluator.h"
#include "evaluator_arginfo.h"

#include "frame.h"
#include "exceptions.h"
#include "alternative.h"

zend_class_entry* php_phathom_evaluator_ce = NULL;
static zend_object_handlers php_phathom_evaluator_handlers;

static zend_object* php_phathom_evaluator_create(zend_class_entry* type) {
    php_phathom_evaluator_t* evaluator = ecalloc(1,
        sizeof(php_phathom_evaluator_t) + zend_object_properties_size(type));

    zend_object_std_init(&evaluator->std, type);
    object_properties_init(&evaluator->std, type);

    evaluator->std.handlers =
        &php_phathom_evaluator_handlers;

    zend_hash_init(
        &evaluator->actions, 4, NULL,
        php_phathom_table_destroy, 0);

    return &evaluator->std;
}

static void php_phathom_evaluator_free(zend_object* std) {
    php_phathom_evaluator_t* evaluator = php_phathom_evaluator_fetch(std);

    if (evaluator->chart) {
        OBJ_RELEASE(&evaluator->chart->std);
    }
    if (evaluator->context) {
        OBJ_RELEASE(evaluator->context);
    }

    zend_hash_destroy(&evaluator->actions);
    zend_object_std_dtor(std);
}

/* =========================================================================
 * Evaluator::priority(Back $back) : int|false
 *   Returns ZEND_LONG_MIN when back->child is NULL or priority is false.
 * ======================================================================= */

static zend_always_inline zend_long php_phathom_evaluator_priority(
    php_phathom_back_t *back)
{
    if (back->child == NULL) {
        return ZEND_LONG_MIN;
    }
    return php_phathom_alternative_priority(back->child->alternative);
}

/* =========================================================================
 * Evaluator::select(array $backs, array $tokens) : Back
 *   Mirrors Evaluator::select().  Returns NULL and throws on ambiguity.
 * ======================================================================= */

static php_phathom_back_t* php_phathom_evaluator_select(
    php_phathom_t           *phathom,
    php_phathom_evaluator_t *eval,
    php_phathom_backs_t     *backs)
{
    php_phathom_back_t *selected    = &backs->path[0];
    zend_long           prioritized = php_phathom_evaluator_priority(selected);

    if (prioritized == ZEND_LONG_MIN) {
        /* priority() === false */
        if (backs->used > 1) {
            php_phathom_item_t *child = selected->child;
            php_phathom_exception_ambiguity_range(
                phathom, eval, child->rule, child->origin, child->pos - 1);
            return NULL;
        }
        return selected;
    }

    for (uint64_t b = 1; b < backs->used; b++) {
        zend_long priority = php_phathom_evaluator_priority(&backs->path[b]);
        if (priority > prioritized) {
            selected    = &backs->path[b];
            prioritized = priority;
        }
    }
    return selected;
}

/* =========================================================================
 * Action method lookup/cache
 *   actions[rule][alt] -> zend_function*
 * ======================================================================= */

static zend_always_inline zend_function* php_phathom_evaluator_action(
    php_phathom_evaluator_t *eval,
    php_phathom_item_t      *item)
{
    HashTable *table =
        zend_hash_find_ptr(
            &eval->actions, item->rule);
    if (!table) {
        table =
            (HashTable*)
                emalloc(sizeof(HashTable));
        zend_hash_init(
            table, 4, NULL, NULL, 0);
        zend_hash_add_ptr(
            &eval->actions, item->rule, table);
    }

    zend_function *fn =
        zend_hash_index_find_ptr(
            table, item->alt);
    if (!fn) {
        zend_string *method = zend_strpprintf(
            0, "__action_%s_" ZEND_LONG_FMT "__",
            ZSTR_VAL(item->rule), (zend_long) item->alt);
        zend_string *key =
            zend_string_tolower(method);
        zend_string_release(method);
        fn = zend_hash_find_ptr(
            &eval->context->ce->function_table, key);
        zend_string_release(key);
        zend_hash_index_add_ptr(table, item->alt, fn);
    }
    return fn;
}

/* =========================================================================
 * Evaluator::apply(Item $item, array $values) : mixed
 *   Mirrors Evaluator::apply().
 *   partial[0..npartial-1] are the $values; ownership stays with caller.
 *   *result receives the new value (caller owns).
 *   Returns false on exception.
 * ======================================================================= */

static bool php_phathom_evaluator_apply(
    php_phathom_t           *phathom,
    php_phathom_evaluator_t *eval,
    php_phathom_item_t      *item,
    zval                    *partial,
    zend_long                npartial,
    zval                    *result)
{
    if (php_phathom_alternative_action(item->alternative)) {
        /* $action(...$values) */
        zend_function *fn =
            php_phathom_evaluator_action(eval, item);
        zend_call_known_function(fn,
            eval->context, eval->context->ce,
            result, (uint32_t) npartial, partial, NULL);
        return !EG(exception);
    }

    zend_array  *symbols   = php_phathom_alternative_symbols(item->alternative);
    zend_object *synthetic = php_phathom_alternative_synthetic(item->alternative);

    if (synthetic == phathom->enumerated.quantifier.star ||
        synthetic == phathom->enumerated.quantifier.plus) {
        array_init(result);
        if (zend_hash_num_elements(symbols) == 2) {
            /* [...$values[0], $values[1]] */
            if (Z_TYPE(partial[0]) == IS_ARRAY) {
                zval *v;
                ZEND_HASH_FOREACH_VAL(Z_ARR(partial[0]), v) {
                    zend_hash_next_index_insert_new(
                        Z_ARR_P(result), v);
                    Z_TRY_ADDREF_P(v);
                } ZEND_HASH_FOREACH_END();
            }
            zend_hash_next_index_insert_new(
                Z_ARR_P(result),
                &partial[1]);
            Z_TRY_ADDREF(partial[1]);
        } else {
            /* [$values[0]] */
            zend_hash_next_index_insert_new(
                Z_ARR_P(result),
                &partial[0]);
            Z_TRY_ADDREF(partial[0]);
        }
        return true;
    }

    if (synthetic == phathom->enumerated.quantifier.optional) {
        /* $values[0] */
        ZVAL_COPY(result, &partial[0]);
        return true;
    }

    /* Quantifier::NONE */
    if (npartial == 1) {
        ZVAL_COPY(result, &partial[0]);
    } else {
        array_init_size(result, (uint32_t) npartial);
        for (zend_long i = 0; i < npartial; i++) {
            zend_hash_next_index_insert_new(
                Z_ARR_P(result),
                &partial[i]);
            Z_TRY_ADDREF(partial[i]);
        }
    }
    return true;
}

/* =========================================================================
 * Evaluator::execute(Item $item) : mixed
 *
 * Iterative trampoline mirroring Evaluator::execute() / Frame::__invoke().
 *
 * Stack discipline (matches PHP LIFO behaviour):
 *   SELECT frame:  walk backs from dot-limit-1 to 0; token backs fill
 *                  partial[] directly; child backs become queued SELECT
 *                  frames with an APPLY frame in front.
 *   APPLY frame:   pop heap values into partial slots, call apply, push
 *                  the result onto the heap.
 * ======================================================================= */

static bool php_phathom_evaluator_execute(
    php_phathom_t           *phathom,
    php_phathom_evaluator_t *eval,
    php_phathom_item_t      *start_item,
    zval                    *return_value)
{
    php_phathom_frame_stack_t stack = {0};
    php_phathom_frame_heap_t  heap  = {0};

    php_phathom_frame_push(&stack, (php_phathom_frame_t){
        .kind = PHP_PHATHOM_FRAME_SELECT,
        .item = start_item,
    });

    while (stack.used > 0) {
        php_phathom_frame_t frame =
            php_phathom_frame_pop(&stack);

        if (frame.kind == PHP_PHATHOM_FRAME_SELECT) {
            php_phathom_item_t *item    = frame.item;
            zend_array *symbols =
                php_phathom_alternative_symbols(item->alternative);
            zend_long limit   =
                (zend_long) zend_hash_num_elements(symbols);

            if (limit == 0) {
                /* empty($alternative->symbols): push [] or null */
                zval v;
                if (php_phathom_alternative_synthetic(item->alternative)
                        != phathom->enumerated.quantifier.none) {
                    array_init(&v);
                } else {
                    ZVAL_NULL(&v);
                }
                php_phathom_frame_heap_push(&heap, &v);
                continue;
            }

            zval      *partial = ecalloc(limit, sizeof(zval));
            zend_long *slots   = emalloc(limit * sizeof(zend_long));
            zend_long  nslots  = 0;

            /* Temporary items list — max `limit` children. */
            php_phathom_item_t **items  = emalloc(limit * sizeof(php_phathom_item_t*));
            zend_long            nitems = 0;

            php_phathom_item_t *cur = item;
            bool ok = true;

            for (zend_long pos = limit - 1; pos >= 0; pos--) {
                php_phathom_back_t *back =
                    php_phathom_evaluator_select(phathom, eval, &cur->backs);
                if (!back) {
                    ok = false;
                    break;
                }
                if (back->token != -1) {
                    zval *tok = zend_hash_index_find(
                        &eval->chart->tokens, back->token);
                    ZVAL_COPY(&partial[pos], tok);
                } else {
                    slots[nslots++]   = pos;
                    items[nitems++]   = back->child;
                }
                cur = back->prev;
            }

            if (!ok) {
                php_phathom_frame_partial_free(partial, limit);
                efree(slots);
                efree(items);
                php_phathom_frame_stack_cleanup(&stack);
                php_phathom_frame_heap_cleanup(&heap);
                return false;
            }

            if (nslots == 0) {
                /* All positions resolved from tokens — apply immediately. */
                zval result;
                ZVAL_UNDEF(&result);
                bool applied = php_phathom_evaluator_apply(
                    phathom, eval, item, partial, limit, &result);
                php_phathom_frame_partial_free(partial, limit);
                efree(slots);
                efree(items);
                if (!applied) {
                    php_phathom_frame_stack_cleanup(&stack);
                    php_phathom_frame_heap_cleanup(&heap);
                    return false;
                }
                php_phathom_frame_heap_push(&heap, &result);
                continue;
            }

            /* Push APPLY frame first (runs after all child SELECTs complete). */
            php_phathom_frame_push(&stack, (php_phathom_frame_t){
                .kind     = PHP_PHATHOM_FRAME_APPLY,
                .item     = item,
                .partial  = partial,
                .slots    = slots,
                .npartial = limit,
                .nslots   = nslots,
            });

            /* Push SELECT frames for each child item.  Items were collected
               from pos=limit-1 down to 0, so items[0] is highest-position.
               Pushing in that order puts items[nitems-1] (lowest) on top → it
               runs first, matching the PHP foreach($items) push order. */
            for (zend_long k = 0; k < nitems; k++) {
                php_phathom_frame_push(&stack, (php_phathom_frame_t){
                    .kind = PHP_PHATHOM_FRAME_SELECT,
                    .item = items[k],
                });
            }
            efree(items);

        } else { /* FRAME_APPLY */

            /* Pop heap values into the slots of partial (reverse order). */
            for (zend_long s = 0; s < frame.nslots; s++) {
                zval v = php_phathom_frame_heap_pop(&heap);
                ZVAL_COPY_VALUE(&frame.partial[frame.slots[s]], &v);
            }

            zval result;
            ZVAL_UNDEF(&result);
            bool applied = php_phathom_evaluator_apply(
                phathom, eval, frame.item, frame.partial, frame.npartial, &result);
            php_phathom_frame_partial_free(frame.partial, frame.npartial);
            efree(frame.slots);

            if (!applied) {
                php_phathom_frame_stack_cleanup(&stack);
                php_phathom_frame_heap_cleanup(&heap);
                return false;
            }
            php_phathom_frame_heap_push(&heap, &result);
        }
    }

    /* Normal exit: heap holds exactly the one result. */
    if (heap.used > 0) {
        zval top =
            php_phathom_frame_heap_pop(&heap);
        ZVAL_COPY_VALUE(return_value, &top);
    }

    if (stack.data) {
        efree(stack.data);
    }

    if (heap.data) {
        efree(heap.data);
    }

    return true;
}

PHP_METHOD(Evaluator, __construct) {
    php_phathom_t phathom =
        php_phathom_fetch();
    php_phathom_evaluator_t* evaluator =
        php_phathom_evaluator_fetch(Z_OBJ(EX(This)));
    zend_object *chart;

    ZEND_PARSE_PARAMETERS_START(2, 2)
        Z_PARAM_OBJ_OF_CLASS(chart, php_phathom_chart_ce);
        Z_PARAM_OBJ_OF_CLASS(evaluator->context, phathom.class.context)
    ZEND_PARSE_PARAMETERS_END();

    evaluator->chart =
        php_phathom_chart_fetch(chart);
    GC_ADDREF(&evaluator->chart->std);
    GC_ADDREF(evaluator->context);
}

PHP_METHOD(Evaluator, __invoke) {
    php_phathom_t phathom =
        php_phathom_fetch();
    php_phathom_evaluator_t* evaluator =
        php_phathom_evaluator_fetch(Z_OBJ(EX(This)));

    ZEND_PARSE_PARAMETERS_NONE();

    php_phathom_chart_t *chart = evaluator->chart;

    /* path[$limit]: the set of items that completed at the end of input. */
    HashTable *final_path =
        zend_hash_index_find_ptr(&chart->path, chart->limit);
    if (!final_path) {
        php_phathom_exception_execute_nomatch(&phathom, evaluator);
        return;
    }

    /* Mirror __invoke(): find the best completed start item. */
    php_phathom_item_t *best_item     = NULL;
    zend_long           best_priority = ZEND_LONG_MIN; /* === false */

    php_phathom_item_t *nitem;
    ZEND_HASH_FOREACH_PTR(final_path, nitem) {
        zend_array *nalt_sym = php_phathom_alternative_symbols(nitem->alternative);
        zend_long   nsym     = (zend_long) zend_hash_num_elements(nalt_sym);

        if (!zend_string_equals(nitem->rule, chart->grammar.start) ||
                nitem->origin != 0 ||
                nitem->dot    != nsym) {
            continue;
        }

        zend_long priority = php_phathom_alternative_priority(nitem->alternative);

        if (best_item == NULL) {
            best_item     = nitem;
            best_priority = priority;
        } else if (best_priority == ZEND_LONG_MIN) {
            /* prioritized === false: second match → ambiguous */
            php_phathom_exception_ambiguity_range(
                &phathom, evaluator, chart->grammar.start, 0, chart->limit - 1);
            return;
        } else if (priority > best_priority) {
            best_item     = nitem;
            best_priority = priority;
        }
    } ZEND_HASH_FOREACH_END();

    if (best_item == NULL) {
        php_phathom_exception_execute_nomatch(&phathom, evaluator);
        return;
    }

    php_phathom_evaluator_execute(&phathom, evaluator, best_item, return_value);
}

static zend_function_entry php_phathom_evaluator_methods[] = {
    PHP_ME(Evaluator, __construct, arginfo_class_pharos_phathom_Earley_Evaluator___construct, ZEND_ACC_PUBLIC)
    PHP_ME(Evaluator, __invoke,    arginfo_class_pharos_phathom_Earley_Evaluator___invoke,    ZEND_ACC_PUBLIC)
    PHP_FE_END
};

PHP_MINIT_FUNCTION(PHATHOM_EVALUATOR) {
    zend_class_entry ce;
    INIT_NS_CLASS_ENTRY(ce, "pharos\\phathom\\Earley", "Evaluator",
        php_phathom_evaluator_methods);

    php_phathom_evaluator_ce = zend_register_internal_class(&ce);
    php_phathom_evaluator_ce->create_object = php_phathom_evaluator_create;

    memcpy(&php_phathom_evaluator_handlers,
        zend_get_std_object_handlers(), sizeof(zend_object_handlers));
    php_phathom_evaluator_handlers.offset   =
        XtOffsetOf(php_phathom_evaluator_t, std);
    php_phathom_evaluator_handlers.free_obj =
        php_phathom_evaluator_free;

    return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(PHATHOM_EVALUATOR) {
    return SUCCESS;
}