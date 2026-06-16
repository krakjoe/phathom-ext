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
        Z_STR_P(address);
    GC_ADDREF(buffer->object);
}

static zend_always_inline void php_phathom_buffer_free(php_phathom_buffer_t *buffer) {
    if (UNEXPECTED(!buffer->object)) {
        return;
    }

    OBJ_RELEASE(buffer->object);
}
#endif