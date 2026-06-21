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
