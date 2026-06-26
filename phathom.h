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

#ifndef PHATHOM_H
#define PHATHOM_H

#include "php.h"

extern zend_module_entry phathom_module_entry;
#define phpext_phathom_ptr &phathom_module_entry

#define PHP_PHATHOM_VERSION "0.0.1-dev"
#define PHP_PHATHOM_EXTNAME "phathom"

typedef struct _php_phathom_t {
    struct {
        zend_class_entry* grammar;
        zend_class_entry* buffer;
        zend_class_entry* context;
        zend_class_entry* quantifier;
        struct {
            zend_class_entry *ambiguity;
            zend_class_entry *execute;
        } exception;
    } class;
    struct {
        struct {
            zend_function* range;
        } ambiguity;
        struct {
            zend_function* nomatch;
        } execute;
    } exception;
    struct {
        struct {
            zend_object* none;
            zend_object* plus;
            zend_object* star;
            zend_object* optional;
        } quantifier;
        struct {
            zend_object* none;
            zend_object* left;
            zend_object* right;
        } associativity;
    } enumerated;
    struct {
        zend_string *scan;
        zend_string *contents;
    } word;
} php_phathom_t;

extern php_phathom_t php_phathom_fetch(void);

static zend_always_inline void php_phathom_table_destroy(zval* zv) {
    zend_hash_destroy(
        Z_PTR_P(zv));
    efree(Z_PTR_P(zv));
}

#define Z_UNWRAP_P(zv) Z_ISREF_P(zv) ? Z_REFVAL_P(zv) : (zv)

static zend_always_inline zval* __php_phathom_fetch_member__(
    zend_object *std, off64_t offset) {
    zval *member = (zval*)
        (((char*) std->properties_table) + offset);
    return Z_UNWRAP_P(member);
}

#define php_phathom_fetch_member(object, layout, member) \
    __php_phathom_fetch_member__(object, XtOffsetOf(layout, member))
#endif	/* PHATHOM_H */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
