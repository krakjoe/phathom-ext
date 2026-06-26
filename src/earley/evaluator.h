/*
  +----------------------------------------------------------------------+
  | phathom                                                              |
  +----------------------------------------------------------------------+
  | Copyright (c) Joe Watkins 2026                                       |
  +----------------------------------------------------------------------+
  | This source file is subject to the BSD 3-Clause License bundled     |
  | with this package in the file LICENSE.                               |
  +----------------------------------------------------------------------+
  | Author: krakjoe                                                      |
  +----------------------------------------------------------------------+
 */
#ifndef HAVE_PHATHOM_EARLEY_EVALUATOR_H
#define HAVE_PHATHOM_EARLEY_EVALUATOR_H

#include "chart.h"

extern zend_class_entry* php_phathom_earley_evaluator_ce;

typedef struct _php_phathom_earley_evaluator_t {
    php_phathom_earley_chart_t *chart;
    zend_object                *context;
    HashTable                   actions;
    zend_object                 std;
} php_phathom_earley_evaluator_t;

static zend_always_inline php_phathom_earley_evaluator_t* php_phathom_earley_evaluator_fetch(zend_object* std) {
    return (php_phathom_earley_evaluator_t*) (((char*) std) - XtOffsetOf(php_phathom_earley_evaluator_t, std));
}

static zend_always_inline zval php_phathom_earley_evaluator_tokens(php_phathom_earley_evaluator_t *eval) {
    zval result;
    array_init_size(&result,
        zend_hash_num_elements(&eval->chart->tokens));

    zval *token;
    ZEND_HASH_FOREACH_VAL(&eval->chart->tokens, token) {
        zend_hash_next_index_insert_new(
            Z_ARR(result), token);
        Z_TRY_ADDREF_P(token);
    } ZEND_HASH_FOREACH_END();
    return result;
}

PHP_MINIT_FUNCTION(PHATHOM_EARLEY_EVALUATOR);
PHP_MSHUTDOWN_FUNCTION(PHATHOM_EARLEY_EVALUATOR);
#endif