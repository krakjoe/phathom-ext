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
#ifndef HAVE_PHATHOM_GLR_WORKLIST_H
#define HAVE_PHATHOM_GLR_WORKLIST_H

#include "phathom.h"
#include "thread.h"

typedef struct {
    php_phathom_glr_thread_t **data;
    zend_long                  used;
    zend_long                  limit;
} php_phathom_glr_worklist_t;

static zend_always_inline void php_phathom_glr_worklist_push(
    php_phathom_glr_worklist_t  *wl,
    php_phathom_glr_thread_t    *thread)
{
    if (wl->used == wl->limit) {
        wl->limit = wl->limit ? wl->limit * 2 : 16;
        wl->data  = (void*) erealloc(wl->data,
            (size_t) wl->limit * sizeof(*wl->data));
    }
    wl->data[wl->used++] = thread;
}

static zend_always_inline void php_phathom_glr_worklist_free(
    php_phathom_glr_worklist_t *wl) {
    if (wl->data) {
        efree(wl->data);
    }

    wl->data  = NULL;
    wl->used  = 0;
    wl->limit = 0;
} /* }}} */
#endif