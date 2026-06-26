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
#ifndef HAVE_PHATHOM_BUFFER_H
#define HAVE_PHATHOM_BUFFER_H
#include "php.h"
#include "phathom.h"

typedef struct _php_phathom_buffer_t {
    zend_string *contents;
    zend_object *object;
} php_phathom_buffer_t;

static zend_always_inline void php_phathom_buffer_fetch(php_phathom_t* phathom, php_phathom_buffer_t *buffer) {
    /* no layout possible, hooked possibly */
    zval property, *address =
        zend_std_read_property(
            buffer->object, phathom->word.contents,
            BP_VAR_R, NULL, &property);

    buffer->contents =
        zend_string_copy(
            Z_STR_P(address));

    if (&property == address) {
        zval_ptr_dtor(address);
    }

    GC_ADDREF(buffer->object);
}

static zend_always_inline void php_phathom_buffer_free(php_phathom_buffer_t *buffer) {
    if (UNEXPECTED(!buffer->object)) {
        return;
    }

    zend_string_release(buffer->contents);

    OBJ_RELEASE(buffer->object);
}
#endif