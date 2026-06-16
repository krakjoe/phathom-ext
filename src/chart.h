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
#ifndef HAVE_PHATHOM_CHART_H
#define HAVE_PHATHOM_CHART_H

#include "php.h"
#include "grammar.h"
#include "buffer.h"

extern zend_class_entry* php_phathom_chart_ce;

typedef struct _php_phathom_item_t php_phathom_item_t;
typedef struct _php_phathom_back_t php_phathom_back_t;

typedef struct _php_phathom_backs_t {
    php_phathom_back_t *path;
    uint64_t            used;
    uint64_t            limit;
} php_phathom_backs_t;

struct _php_phathom_back_t {
    php_phathom_item_t *prev;
    php_phathom_item_t *child;
    zend_long           token;
};

struct _php_phathom_item_t {
    zend_long           pos;
    zend_long           alt;
    zend_long           dot;
    zend_long           origin;
    php_phathom_backs_t backs;

    zend_string  *rule;
    zend_object  *alternative;
};

typedef struct _php_phathom_items_t {
    php_phathom_item_t *items;
    zend_long           used;
    zend_long           limit;
} php_phathom_items_t;

typedef struct _php_phathom_chart_t {
    php_phathom_grammar_t grammar;
    php_phathom_buffer_t  buffer;
    HashTable             index;
    HashTable             waiting;
    HashTable             nullable;
    HashTable             path;
    HashTable             tokens;
    zend_long             position;
    zend_long             limit;
    zend_object           std;
} php_phathom_chart_t;

static zend_always_inline php_phathom_chart_t* php_phathom_chart_fetch(zend_object* std) {
    return (php_phathom_chart_t*) (((char*) std) - XtOffsetOf(php_phathom_chart_t, std));
}

PHP_MINIT_FUNCTION(PHATHOM_CHART);
PHP_MSHUTDOWN_FUNCTION(PHATHOM_CHART);
#endif