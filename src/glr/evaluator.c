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

zend_class_entry* php_phathom_glr_evaluator_ce = NULL;
static zend_object_handlers php_phathom_glr_evaluator_handlers;

static zend_object* php_phathom_glr_evaluator_create(zend_class_entry* type) {
    php_phathom_glr_evaluator_t* evaluator = ecalloc(1,
        sizeof(php_phathom_glr_evaluator_t) + zend_object_properties_size(type));

    zend_object_std_init(&evaluator->std, type);
    object_properties_init(&evaluator->std, type);

    evaluator->std.handlers = &php_phathom_glr_evaluator_handlers;

    zend_hash_init(
        &evaluator->actions, 4, NULL,
        php_phathom_table_destroy, 0);

    return &evaluator->std;
}

static void php_phathom_glr_evaluator_free(zend_object* std) {
    php_phathom_glr_evaluator_t* evaluator =
        php_phathom_glr_evaluator_fetch(std);

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
 * Action method lookup/cache
 *   actions[rule][alt] -> zend_function*
 * ======================================================================= */

static zend_always_inline zend_function* php_phathom_glr_evaluator_action(
    php_phathom_glr_evaluator_t *eval,
    php_phathom_glr_node_t      *node)
{
    HashTable *table =
        zend_hash_find_ptr(&eval->actions, node->rule);
    if (!table) {
        table = (HashTable*) emalloc(sizeof(HashTable));
        zend_hash_init(table, 4, NULL, NULL, 0);
        zend_hash_add_ptr(&eval->actions, node->rule, table);
    }

    zend_function *fn =
        zend_hash_index_find_ptr(table, node->alt);
    if (!fn) {
        zend_string *method = zend_strpprintf(
            0, "__action_%s_" ZEND_LONG_FMT "__",
            ZSTR_VAL(node->rule), node->alt);
        zend_string *key = zend_string_tolower(method);
        zend_string_release(method);
        fn = zend_hash_find_ptr(
            &eval->context->ce->function_table, key);
        zend_string_release(key);
        zend_hash_index_add_ptr(table, node->alt, fn);
    }
    return fn;
}

/* =========================================================================
 * apply(node, partial, npartial) : mixed
 *   Mirrors Evaluator::apply().
 * ======================================================================= */

static bool php_phathom_glr_evaluator_apply(
    php_phathom_t               *phathom,
    php_phathom_glr_evaluator_t *eval,
    php_phathom_glr_node_t      *node,
    zval                        *partial,
    zend_long                    npartial,
    zval                        *result)
{
    if (php_phathom_alternative_action(node->alternative)) {
        zend_function *fn =
            php_phathom_glr_evaluator_action(eval, node);
        zend_call_known_function(fn,
            eval->context, eval->context->ce,
            result, (uint32_t) npartial, partial, NULL);
        return !EG(exception);
    }

    zend_array  *symbols   = php_phathom_alternative_symbols(node->alternative);
    zend_object *synthetic = php_phathom_alternative_synthetic(node->alternative);

    if (synthetic == phathom->enumerated.quantifier.star ||
        synthetic == phathom->enumerated.quantifier.plus) {
        array_init(result);
        if (zend_hash_num_elements(symbols) == 2) {
            /* [...$values[0], $values[1]] */
            if (Z_TYPE(partial[0]) == IS_ARRAY) {
                zval *v;
                ZEND_HASH_FOREACH_VAL(Z_ARR(partial[0]), v) {
                    zend_hash_next_index_insert_new(Z_ARR_P(result), v);
                    Z_TRY_ADDREF_P(v);
                } ZEND_HASH_FOREACH_END();
            }
            zend_hash_next_index_insert_new(Z_ARR_P(result), &partial[1]);
            Z_TRY_ADDREF(partial[1]);
        } else {
            /* [$values[0]] */
            zend_hash_next_index_insert_new(Z_ARR_P(result), &partial[0]);
            Z_TRY_ADDREF(partial[0]);
        }
        return true;
    }

    if (synthetic == phathom->enumerated.quantifier.optional) {
        ZVAL_COPY(result, &partial[0]);
        return true;
    }

    /* Quantifier::NONE */
    if (npartial == 1) {
        ZVAL_COPY(result, &partial[0]);
    } else {
        array_init_size(result, (uint32_t) npartial);
        for (zend_long i = 0; i < npartial; i++) {
            zend_hash_next_index_insert_new(Z_ARR_P(result), &partial[i]);
            Z_TRY_ADDREF(partial[i]);
        }
    }
    return true;
}

/* =========================================================================
 * execute(node) : mixed
 *
 * Iterative trampoline mirroring Evaluator::execute() / Frame::__invoke().
 *
 * SELECT frame: walk node's children right-to-left; token slots fill
 *               partial[] directly from chart->tokens; NODE children
 *               become queued SELECT frames with an APPLY frame in front.
 * APPLY frame:  pop heap values into partial slots, call apply(), push
 *               the result onto the heap.
 * ======================================================================= */

static bool php_phathom_glr_evaluator_execute(
    php_phathom_t               *phathom,
    php_phathom_glr_evaluator_t *eval,
    php_phathom_glr_node_t      *start_node,
    zval                        *return_value)
{
    php_phathom_frame_stack_t stack = {0};
    php_phathom_frame_heap_t  heap  = {0};

    php_phathom_frame_push(&stack, (php_phathom_frame_t){
        .kind = PHP_PHATHOM_FRAME_SELECT,
        .item = start_node,
    });

    while (stack.used > 0) {
        php_phathom_frame_t frame = php_phathom_frame_pop(&stack);

        if (frame.kind == PHP_PHATHOM_FRAME_SELECT) {
            php_phathom_glr_node_t *node =
                (php_phathom_glr_node_t *) frame.item;
            zend_long limit = node->nchildren;

            if (limit == 0) {
                /* empty alternative: push [] or null */
                zval v;
                if (php_phathom_alternative_synthetic(node->alternative)
                        != phathom->enumerated.quantifier.none) {
                    array_init(&v);
                } else {
                    ZVAL_NULL(&v);
                }
                php_phathom_frame_heap_push(&heap, &v);
                continue;
            }

            zval      *partial = ecalloc((size_t) limit, sizeof(zval));
            zend_long *slots   = emalloc((size_t) limit * sizeof(zend_long));
            zend_long  nslots  = 0;

            /* child nodes collected right-to-left (lowest pos first on stack) */
            php_phathom_glr_node_t **items  =
                emalloc((size_t) limit * sizeof(php_phathom_glr_node_t *));
            zend_long nitems = 0;

            for (zend_long pos = limit - 1; pos >= 0; pos--) {
                php_phathom_glr_slot_t *slot = &node->children[pos];
                if (slot->kind == PHP_PHATHOM_GLR_SLOT_TOKEN) {
                    zval *tok = zend_hash_index_find(
                        &eval->chart->tokens, (zend_ulong) slot->token);
                    ZVAL_COPY(&partial[pos], tok);
                } else if (slot->kind == PHP_PHATHOM_GLR_SLOT_NODE) {
                    slots[nslots++] = pos;
                    items[nitems++] = slot->node;
                } else {
                    /* SLOT_NULL (stack bottom sentinel) — treat as null */
                    ZVAL_NULL(&partial[pos]);
                }
            }

            if (nslots == 0) {
                /* All positions resolved from tokens/null — apply immediately */
                zval res;
                ZVAL_UNDEF(&res);
                bool applied = php_phathom_glr_evaluator_apply(
                    phathom, eval, node, partial, limit, &res);
                php_phathom_frame_partial_free(partial, limit);
                efree(slots);
                efree(items);
                if (!applied) {
                    php_phathom_frame_stack_cleanup(&stack);
                    php_phathom_frame_heap_cleanup(&heap);
                    return false;
                }
                php_phathom_frame_heap_push(&heap, &res);
                continue;
            }

            /* Push APPLY frame first (runs after all child SELECTs complete) */
            php_phathom_frame_push(&stack, (php_phathom_frame_t){
                .kind     = PHP_PHATHOM_FRAME_APPLY,
                .item     = node,
                .partial  = partial,
                .slots    = slots,
                .npartial = limit,
                .nslots   = nslots,
            });

            /* Push SELECT frames for each child node.
               items[0] is the highest-position child (collected right-to-left),
               so push in order 0..nitems-1 to put lowest-position on top. */
            for (zend_long k = 0; k < nitems; k++) {
                php_phathom_frame_push(&stack, (php_phathom_frame_t){
                    .kind = PHP_PHATHOM_FRAME_SELECT,
                    .item = items[k],
                });
            }
            efree(items);

        } else { /* FRAME_APPLY */

            php_phathom_glr_node_t *node =
                (php_phathom_glr_node_t *) frame.item;

            /* Pop heap values into partial slots */
            for (zend_long s = 0; s < frame.nslots; s++) {
                zval v = php_phathom_frame_heap_pop(&heap);
                ZVAL_COPY_VALUE(&frame.partial[frame.slots[s]], &v);
            }

            zval res;
            ZVAL_UNDEF(&res);
            bool applied = php_phathom_glr_evaluator_apply(
                phathom, eval, node, frame.partial, frame.npartial, &res);
            php_phathom_frame_partial_free(frame.partial, frame.npartial);
            efree(frame.slots);

            if (!applied) {
                php_phathom_frame_stack_cleanup(&stack);
                php_phathom_frame_heap_cleanup(&heap);
                return false;
            }
            php_phathom_frame_heap_push(&heap, &res);
        }
    }

    if (heap.used > 0) {
        zval top = php_phathom_frame_heap_pop(&heap);
        ZVAL_COPY_VALUE(return_value, &top);
    }

    if (stack.data) efree(stack.data);
    if (heap.data)  efree(heap.data);
    return true;
}

PHP_FUNCTION(php_phathom_glr_evaluator___construct) {
    php_phathom_t phathom =
        php_phathom_fetch();
    php_phathom_glr_evaluator_t *evaluator =
        php_phathom_glr_evaluator_fetch(Z_OBJ(EX(This)));
    zend_object *chart;

    ZEND_PARSE_PARAMETERS_START(2, 2)
        Z_PARAM_OBJ_OF_CLASS(chart, php_phathom_glr_chart_ce);
        Z_PARAM_OBJ_OF_CLASS(evaluator->context, phathom.class.context)
    ZEND_PARSE_PARAMETERS_END();

    evaluator->chart = php_phathom_glr_chart_fetch(chart);
    GC_ADDREF(&evaluator->chart->std);
    GC_ADDREF(evaluator->context);
}

PHP_FUNCTION(php_phathom_glr_evaluator___invoke) {
    php_phathom_t phathom =
        php_phathom_fetch();
    php_phathom_glr_evaluator_t *evaluator =
        php_phathom_glr_evaluator_fetch(Z_OBJ(EX(This)));

    ZEND_PARSE_PARAMETERS_NONE();

    php_phathom_glr_chart_t *chart = evaluator->chart;

    /* No accepting threads -> nomatch */
    if (chart->nthreads == 0) {
        php_phathom_exception_execute_nomatch(
            &phathom,
            evaluator->context,
            chart->start,
            &chart->tokens);
        return;
    }

    /* Find the single winner thread; more than one -> ambiguity */
    php_phathom_glr_node_t *winner = NULL;

    for (zend_long i = 0; i < chart->nthreads; i++) {
        php_phathom_glr_thread_t *t = chart->threads[i];
        /* top slot of accepting thread is always the root Node */
        php_phathom_glr_node_t *node =
            t->nodes[t->depth - 1].node;

        if (winner == NULL) {
            winner = node;
            continue;
        }

        /* Second accepting thread -> ambiguity */
        php_phathom_exception_ambiguity_range(
            &phathom,
            evaluator->context,
            chart->start,
            0,
            chart->limit - 1,
            &chart->tokens);
        return;
    }

    php_phathom_glr_evaluator_execute(&phathom, evaluator, winner, return_value);
}

static zend_function_entry php_phathom_glr_evaluator_methods[] = {
    ZEND_NAMED_ME(__construct, PHP_FN(php_phathom_glr_evaluator___construct), arginfo_class_pharos_phathom_GLR_Evaluator___construct, ZEND_ACC_PUBLIC)
    ZEND_NAMED_ME(__invoke,    PHP_FN(php_phathom_glr_evaluator___invoke),    arginfo_class_pharos_phathom_GLR_Evaluator___invoke,    ZEND_ACC_PUBLIC)
    PHP_FE_END
};

PHP_MINIT_FUNCTION(PHATHOM_GLR_EVALUATOR) {
    zend_class_entry ce;
    INIT_NS_CLASS_ENTRY(ce, "pharos\\phathom\\GLR", "Evaluator",
        php_phathom_glr_evaluator_methods);

    php_phathom_glr_evaluator_ce = zend_register_internal_class(&ce);
    php_phathom_glr_evaluator_ce->create_object = php_phathom_glr_evaluator_create;

    memcpy(&php_phathom_glr_evaluator_handlers,
        zend_get_std_object_handlers(), sizeof(zend_object_handlers));
    php_phathom_glr_evaluator_handlers.offset =
        XtOffsetOf(php_phathom_glr_evaluator_t, std);
    php_phathom_glr_evaluator_handlers.free_obj =
        php_phathom_glr_evaluator_free;

    return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(PHATHOM_GLR_EVALUATOR) {
    return SUCCESS;
}
