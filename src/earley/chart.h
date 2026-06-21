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
#ifndef HAVE_PHATHOM_EARLEY_CHART_H
#define HAVE_PHATHOM_EARLEY_CHART_H

#include "php.h"
#include "grammar.h"
#include "buffer.h"
#include "hash.h"

extern zend_class_entry* php_phathom_earley_chart_ce;

typedef struct _php_phathom_earley_item_t php_phathom_earley_item_t;
typedef struct _php_phathom_earley_back_t php_phathom_earley_back_t;

struct _php_phathom_earley_back_t {
    php_phathom_earley_item_t *prev;
    php_phathom_earley_item_t *child;
    zend_long           token;
};

typedef struct _php_phathom_earley_backs_t {
    uint64_t           used;
    uint64_t           limit;
    union {
        php_phathom_earley_back_t  one;   /* limit <= 1: inline */
        php_phathom_earley_back_t *many;  /* limit >  1: arena  */
    };
} php_phathom_earley_backs_t;

struct _php_phathom_earley_item_t {
    zend_long           pos;
    zend_long           alt;
    zend_long           dot;
    zend_long           origin;
    php_phathom_earley_backs_t backs;

    zend_string  *rule;
    zend_object  *alternative;
};

typedef struct _php_phathom_earley_items_t {
    php_phathom_earley_item_t *items;
    zend_long           used;
    zend_long           limit;
} php_phathom_earley_items_t;

typedef struct _php_phathom_earley_chart_t {
    php_phathom_grammar_t grammar;
    php_phathom_buffer_t  buffer;
    zend_arena           *arena;
    php_phathom_hash_t    index;
    php_phathom_hash_t    waiting;
    php_phathom_hash_t    nullable;
    php_phathom_hash_t    path;
    HashTable             tokens;
    zend_long             position;
    zend_long             limit;
    zend_object           std;
} php_phathom_earley_chart_t;

static zend_always_inline php_phathom_earley_chart_t* php_phathom_earley_chart_fetch(zend_object* std) {
    return (php_phathom_earley_chart_t*) (((char*) std) - XtOffsetOf(php_phathom_earley_chart_t, std));
}

static zend_always_inline php_phathom_earley_back_t*
    php_phathom_earley_chart_back_fetch(
        php_phathom_earley_backs_t *backs, uint64_t index) {
    return backs->limit > 1 ?
        &backs->many[index] :
        &backs->one;
}

PHP_MINIT_FUNCTION(PHATHOM_EARLEY_CHART);
PHP_MSHUTDOWN_FUNCTION(PHATHOM_EARLEY_CHART);
#endif
