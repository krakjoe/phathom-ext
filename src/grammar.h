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
#ifndef HAVE_PHATHOM_GRAMMAR_H
#define HAVE_PHATHOM_GRAMMAR_H
#include "php.h"
#include "phathom.h"

typedef struct _php_phathom_grammar_t {
    zend_function *scanner;
    zend_object   *lexer;
    zend_string   *context;
    zend_string   *token;
    zend_string   *start;
    zend_array    *rules;
    zend_array    *terminals;
    zend_array    *patterns;
    zend_object   *object;
} php_phathom_grammar_t;

static zend_always_inline void php_phathom_grammar_fetch(php_phathom_t* phathom, php_phathom_grammar_t *grammar) {
    if (UNEXPECTED(!grammar->object)) {
        return;
    }

    struct __layout__ {
        zval __unused__[3];
        zval lexer;
        zval abstracts;
        zval context;
        zval token;
        zval start;
        zval rules;
        zval terminals;
        zval patterns;
    };

    struct __layout__* layout =
        (struct __layout__*)
            (char*)
                (grammar->object->properties_table);

    grammar->context   = Z_STR(layout->context);
    grammar->token     = Z_STR(layout->token);
    grammar->start     = Z_STR(layout->start);
    grammar->rules     = Z_ARR(layout->rules);
    grammar->lexer     = Z_OBJ(layout->lexer);
    grammar->scanner   =
        zend_std_get_method(
            &grammar->lexer,
            phathom->word.scan, NULL);

    GC_ADDREF(grammar->object);
}

static zend_always_inline void php_phathom_grammar_free(php_phathom_grammar_t *grammar) {
    if (UNEXPECTED(!grammar->object)) {
        return;
    }
    OBJ_RELEASE(grammar->object);
}
#endif