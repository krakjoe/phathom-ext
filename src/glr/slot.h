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
#ifndef HAVE_PHATHOM_GLR_SLOT_H
#define HAVE_PHATHOM_GLR_SLOT_H

#include "phathom.h"

typedef enum {
    PHP_PHATHOM_GLR_SLOT_NULL  = 0,   /* stack-bottom sentinel      */
    PHP_PHATHOM_GLR_SLOT_TOKEN = 1,   /* index into chart->tokens   */
    PHP_PHATHOM_GLR_SLOT_NODE  = 2,   /* pointer to a C-level node  */
} php_phathom_glr_slot_kind_t;

typedef struct _php_phathom_glr_node_t php_phathom_glr_node_t;

/* A single stack/children slot: either a token index or a sub-tree. */
typedef struct {
    php_phathom_glr_slot_kind_t  kind;
    union {
        zend_long                token; /* PHP_PHATHOM_GLR_SLOT_TOKEN */
        php_phathom_glr_node_t  *node;  /* PHP_PHATHOM_GLR_SLOT_NODE  */
    };
} php_phathom_glr_slot_t;
#endif