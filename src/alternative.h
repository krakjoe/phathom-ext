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
#ifndef HAVE_PHATHOM_ALTERNATIVE_H
#define HAVE_PHATHOM_ALTERNATIVE_H

#include "phathom.h"

/*
 * Alternative layout:
 *   public int|false     $priority      [0]
 *   public Associativity $associativity [1]
 *   public File          $file          [2]
 *   public array         $symbols       [3]
 *   public array         $annotations   [4]
 *   public ?string       $action        [5]
 *   public Quantifier    $synthetic     [6]
 */
typedef struct php_phathom_alternative_t {
    zval priority;
    zval associativity;
    zval file;
    zval symbols;
    zval annotations;
    zval action;
    zval synthetic;
} php_phathom_alternative_t;

static zend_always_inline zend_array* php_phathom_alternative_symbols(zend_object *obj) {
    zval *member = php_phathom_fetch_member(
        obj, php_phathom_alternative_t, symbols);
    return Z_ARR_P(member);
}

static zend_always_inline zend_object* php_phathom_alternative_symbol(zend_object *obj, zend_long index) {
    zend_array* symbols =
        php_phathom_alternative_symbols(obj);
    zval *symbol = zend_hash_index_find(symbols, index);
    if (!symbol) {
        return NULL;
    }
    return Z_OBJ_P(Z_UNWRAP_P(symbol));
}

/* Returns ZEND_LONG_MIN when priority === false (no priority annotation). */
static zend_always_inline zend_long php_phathom_alternative_priority(zend_object *obj) {
    zval *member = php_phathom_fetch_member(
        obj, php_phathom_alternative_t, priority);
    if (Z_TYPE_P(member) == IS_LONG) {
        return Z_LVAL_P(member);
    }
    return ZEND_LONG_MIN;
}

/* Returns true when action != null (ie, executable alternative). */
static zend_always_inline bool php_phathom_alternative_action(zend_object *obj) {
    zval *member = php_phathom_fetch_member(
        obj, php_phathom_alternative_t, action);
    return Z_TYPE_P(member) != IS_NULL;
}

/* Returns the Quantifier enum-case object for the synthetic field. */
static zend_always_inline zend_object* php_phathom_alternative_synthetic(zend_object *obj) {
    zval *member = php_phathom_fetch_member(
        obj, php_phathom_alternative_t, synthetic);
    return Z_OBJ_P(member);
}

/* Returns the Associativity enum-case object for the associativity field. */
static zend_always_inline zend_object* php_phathom_alternative_associativity(zend_object *obj) {
    zval *member = php_phathom_fetch_member(
        obj, php_phathom_alternative_t, associativity);
    return Z_OBJ_P(member);
}
#endif