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
#ifndef HAVE_PHATHOM_GLR_EVALUATOR_H
#define HAVE_PHATHOM_GLR_EVALUATOR_H

#include "chart.h"

extern zend_class_entry* php_phathom_glr_evaluator_ce;

typedef struct _php_phathom_glr_evaluator_t {
    php_phathom_glr_chart_t *chart;
    zend_object             *context;
    HashTable                actions;  /* rule => HashTable (alt => zend_function*) */
    zend_object              std;
} php_phathom_glr_evaluator_t;

static zend_always_inline php_phathom_glr_evaluator_t* php_phathom_glr_evaluator_fetch(zend_object* std) {
    return (php_phathom_glr_evaluator_t*) (((char*) std) - XtOffsetOf(php_phathom_glr_evaluator_t, std));
}

PHP_MINIT_FUNCTION(PHATHOM_GLR_EVALUATOR);
PHP_MSHUTDOWN_FUNCTION(PHATHOM_GLR_EVALUATOR);
#endif
