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
#ifndef HAVE_PHATHOM_GLR_CHART_H
#define HAVE_PHATHOM_GLR_CHART_H

#include "php.h"
#include "engine.h"
#include "grammar.h"
#include "buffer.h"
#include "thread.h"
#include "node.h"

extern zend_class_entry* php_phathom_glr_chart_ce;

typedef struct {
    php_phathom_grammar_t      grammar;
    php_phathom_buffer_t       buffer;
    zend_arena                *arena;
    zend_string               *start;
    HashTable                  tokens;
    zend_long                  position;
    zend_long                  limit;
    php_phathom_glr_thread_t **threads;
    zend_long                  nthreads;
    zend_object                std;
} php_phathom_glr_chart_t;

static zend_always_inline php_phathom_glr_chart_t* php_phathom_glr_chart_fetch(zend_object *std) {
    return (php_phathom_glr_chart_t*) (((char*) std) - XtOffsetOf(php_phathom_glr_chart_t, std));
}

PHP_MINIT_FUNCTION(PHATHOM_GLR_CHART);
PHP_MSHUTDOWN_FUNCTION(PHATHOM_GLR_CHART);
#endif
