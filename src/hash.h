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
#ifndef HAVE_PHATHOM_HASH_H
#define HAVE_PHATHOM_HASH_H

#include "php.h"
#include "Zend/zend_arena.h"

/*
 * Why a custom hash here?
 *
 * phathom chart state is append-heavy, lookup-heavy, and short-lived. We use
 * an arena-backed, insertion-ordered hash so chart teardown is a single arena
 * destroy, not a recursive HashTable walk with per-node frees.
 *
 * Local perf (teardown probe) after arena migration in chart:
 * - php_phathom_chart_free dropped from ~12.70% to ~0.13% flat overhead.
 *
 * This structure also avoids nested Zend HashTable allocations for chart-owned
 * transient maps, while preserving stable insertion-order iteration semantics.
 */

typedef enum _php_phathom_hash_kind_t {
    PHP_PHATHOM_HASH_STRING,
    PHP_PHATHOM_HASH_INDEX,
} php_phathom_hash_kind_t;

typedef struct _php_phathom_hash_key_t {
    php_phathom_hash_kind_t kind;
    union {
        zend_string *string;
        uint64_t     index;
    } value;
    zend_ulong hash;
} php_phathom_hash_key_t;

/* insertion-ordered entry; stored in entries[] */
typedef struct _php_phathom_hash_entry_t {
    php_phathom_hash_key_t  key;
    void                   *value;
} php_phathom_hash_entry_t;

typedef struct _php_phathom_hash_entries_t {
    php_phathom_hash_entry_t *data;
    uint32_t                  used;
    uint32_t                  size;
} php_phathom_hash_entries_t;

/* open-address bucket: slot is 1-based index into entries[]
   (0 == PHP_PHATHOM_HASH_EMPTY_BUCKET) */

#define PHP_PHATHOM_HASH_EMPTY_BUCKET 0

typedef uint32_t php_phathom_hash_bucket_t;

typedef struct _php_phathom_hash_buckets_t {
    php_phathom_hash_bucket_t *data;
    uint32_t                   used;
    uint32_t                   size;
    uint32_t                   mask;
    uint32_t                   limit;
} php_phathom_hash_buckets_t;

typedef struct _php_phathom_hash_t {
    zend_arena                 **arena;
    php_phathom_hash_entries_t   entries; /* insertion-ordered list      */
    php_phathom_hash_buckets_t   buckets; /* open-address probe table    */
} php_phathom_hash_t;

/* ---- key constructors -------------------------------------------------- */

static zend_always_inline php_phathom_hash_key_t php_phathom_hash_key_string(zend_string *string) {
    ZEND_ASSERT(string);
    ZEND_ASSERT(ZSTR_IS_INTERNED(string));

    return (php_phathom_hash_key_t) {
        .kind  = PHP_PHATHOM_HASH_STRING,
        .value = { 
            .string = string 
        },
        .hash  = ZSTR_HASH(string),
    };
}

static zend_always_inline php_phathom_hash_key_t php_phathom_hash_key_index(uint64_t index) {
    return (php_phathom_hash_key_t) {
        .kind  = PHP_PHATHOM_HASH_INDEX,
        .value = { 
            .index = index 
        },
        /* | 1 keeps non-zero without a branch */
        .hash  = zend_hash_func(
            (const char*) &index, sizeof(index)) | 1,
    };
}

/* ---- internal helpers -------------------------------------------------- */

static zend_always_inline uint32_t php_phathom_hash_roundup(uint32_t size) {
    uint32_t result = 8;

    while (result < size) {
        result <<= 1; 
    }

    return result;
}

static zend_always_inline bool php_phathom_hash_key_equals(
    php_phathom_hash_key_t left, php_phathom_hash_key_t right) {
    if (left.kind != right.kind) {
        return false;
    }

    if (UNEXPECTED(left.kind == PHP_PHATHOM_HASH_STRING)) {
        if (left.value.string != right.value.string) {
            return zend_string_equals(
                left.value.string,
                right.value.string);
        }
        return true;
    }
    return left.value.index == right.value.index;
}

static zend_always_inline php_phathom_hash_bucket_t *php_phathom_hash_buckets_alloc(
    php_phathom_hash_t *hash, uint32_t size) {
    php_phathom_hash_bucket_t *data =
        (php_phathom_hash_bucket_t*)
            zend_arena_alloc(
                hash->arena, sizeof(php_phathom_hash_bucket_t) * size);

    memset(data, 0, sizeof(php_phathom_hash_bucket_t) * size);

    return data;
}

static zend_always_inline php_phathom_hash_entry_t *php_phathom_hash_entries_alloc(
    php_phathom_hash_t *hash, uint32_t capacity) {
    return (php_phathom_hash_entry_t*)
        zend_arena_alloc(
            hash->arena, sizeof(php_phathom_hash_entry_t) * capacity);
}

static zend_always_inline php_phathom_hash_bucket_t *php_phathom_hash_bucket_select(
    php_phathom_hash_t *hash, php_phathom_hash_key_t key, bool *found) {
    uint32_t offset = (uint32_t) (key.hash & hash->buckets.mask);

    for (;;) {
        php_phathom_hash_bucket_t *bucket =
            &hash->buckets.data[offset];

        if (*bucket == PHP_PHATHOM_HASH_EMPTY_BUCKET) {
            *found = false;
            return bucket;
        }

        php_phathom_hash_entry_t *entry =
            &hash->entries.data[*bucket];

        if (entry->key.hash == key.hash &&
            php_phathom_hash_key_equals(entry->key, key)) {
            *found = true;
            return bucket;
        }

        offset = (offset + 1) & hash->buckets.mask;
    }
}

static zend_always_inline void php_phathom_hash_entries_extend(php_phathom_hash_t *hash) {
    uint32_t size = hash->entries.size * 2;
    php_phathom_hash_entry_t *grown =
        php_phathom_hash_entries_alloc(hash, size + 1);

    /* copy 1..nentries; slot 0 stays zeroed (sentinel) */
    memcpy(grown + 1, hash->entries.data + 1,
        sizeof(php_phathom_hash_entry_t) * hash->entries.used);

    hash->entries.data = grown;
    hash->entries.size = size;
}

static zend_always_inline void php_phathom_hash_buckets_extend(php_phathom_hash_t *hash) {
    uint32_t position;

    hash->buckets.size <<= 1;
    hash->buckets.mask  = hash->buckets.size - 1;
    hash->buckets.limit = (hash->buckets.size * 7) / 10;
    hash->buckets.data  = php_phathom_hash_buckets_alloc(hash, hash->buckets.size);
    hash->buckets.used  = 0;

    /* re-index all live entries into the new bucket table */
    for (position = 1; position <= hash->entries.used; position++) {
        php_phathom_hash_entry_t *entry = &hash->entries.data[position];
        bool found;
        php_phathom_hash_bucket_t *bucket =
            php_phathom_hash_bucket_select(
                hash, entry->key, &found);
        *bucket = position;
        hash->buckets.used++;
    }
}

static zend_always_inline void php_phathom_hash_extend(php_phathom_hash_t *hash) {
    if (UNEXPECTED(hash->buckets.used >= hash->buckets.limit)) {
        php_phathom_hash_buckets_extend(hash);
    }

    if (UNEXPECTED(hash->entries.used >= hash->entries.size)) {
        php_phathom_hash_entries_extend(hash);
    }
}

/* ---- public API -------------------------------------------------------- */

static zend_always_inline void php_phathom_hash_init(
    php_phathom_hash_t *hash, zend_arena **arena, uint32_t size) {
    uint32_t cap = php_phathom_hash_roundup(size ? size : 8);

    hash->arena         = arena;
    hash->buckets.size  = cap;
    hash->buckets.mask  = cap - 1;
    hash->buckets.limit = (cap * 7) / 10;
    hash->buckets.used  = 0;
    hash->buckets.data  = php_phathom_hash_buckets_alloc(hash, cap);

    /* entries[] is 1-based; allocate cap+1 so slot 0 is always the sentinel */
    hash->entries.used = 0;
    hash->entries.size = cap;
    hash->entries.data =
        php_phathom_hash_entries_alloc(
            hash, cap + 1);
}

static zend_always_inline void php_phathom_hash_reset(php_phathom_hash_t *hash) {
    hash->buckets.used = 0;
    hash->entries.used = 0;

    memset(hash->buckets.data, 0, sizeof(php_phathom_hash_bucket_t) * hash->buckets.size);
}

static zend_always_inline void *php_phathom_hash_find(
    php_phathom_hash_t *hash, php_phathom_hash_key_t key) {
    bool found;

    php_phathom_hash_bucket_t *bucket =
        php_phathom_hash_bucket_select(
            hash, key, &found);

    return found ? hash->entries.data[*bucket].value : NULL;
}

static zend_always_inline void* php_phathom_hash_find_index(
    php_phathom_hash_t *hash, uint64_t index) {
    return php_phathom_hash_find(
        hash,
        php_phathom_hash_key_index(index));
}

static zend_always_inline void* php_phathom_hash_find_string(
    php_phathom_hash_t *hash, zend_string *string) {
    return php_phathom_hash_find(
        hash,
        php_phathom_hash_key_string(string));
}

static zend_always_inline bool php_phathom_hash_add(
    php_phathom_hash_t *hash, php_phathom_hash_key_t key, void *value) {
    bool found;
    php_phathom_hash_bucket_t *bucket;

    ZEND_ASSERT(value);

    bucket = php_phathom_hash_bucket_select(hash, key, &found);
    if (found) {
        return false;
    }

    php_phathom_hash_extend(hash);

    bucket =
        php_phathom_hash_bucket_select(
            hash, key, &found);

    ZEND_ASSERT(!found);

    uint32_t slot =
        ++hash->entries.used;

    hash->entries.data[slot].key   = key;
    hash->entries.data[slot].value = value;
    hash->buckets.used++;

    *bucket = slot;

    return true;
}

static zend_always_inline void *php_phathom_hash_update(
    php_phathom_hash_t *hash, php_phathom_hash_key_t key, void *value) {
    bool found;
    php_phathom_hash_bucket_t *bucket;
    void *previous;

    ZEND_ASSERT(value);

    bucket = php_phathom_hash_bucket_select(hash, key, &found);
    previous =
        found ?
            hash->entries.data[*bucket].value : NULL;

    if (found) {
        hash->entries.data[*bucket].value = value;
        return previous;
    }

    php_phathom_hash_extend(hash);

    bucket =
        php_phathom_hash_bucket_select(
            hash, key, &found);
    ZEND_ASSERT(!found);

    uint32_t slot =
        ++hash->entries.used;

    hash->entries.data[slot].key   = key;
    hash->entries.data[slot].value = value;
    hash->buckets.used++;

    *bucket = slot;

    return previous;
}

static zend_always_inline void php_phathom_hash_append(
    php_phathom_hash_t *hash, void *data) {
    php_phathom_hash_add(
        hash,
        php_phathom_hash_key_index(
            (uint64_t) hash->entries.used),
        data);
}

/* ---- iteration macros -------------------------------------------------- */

/*
 * PHP_PHATHOM_HASH_FOREACH_CURRENT(hash, type, var)
 *
 * Concurrent modification is invisible.
 *
 * Usage:
 *   PHP_PHATHOM_HASH_FOREACH_CURRENT(&my_hash, my_type_t, item) {
 *       // item is my_type_t*
 *   } PHP_PHATHOM_HASH_FOREACH_END();
 */
#define PHP_PHATHOM_HASH_FOREACH_CURRENT(_hash, _type, _var)            \
    do {                                                                \
        php_phathom_hash_entry_t *_entries = (_hash)->entries.data;     \
        uint32_t _snap = (_hash)->entries.used;                         \
        for (uint32_t _i = 1; _i <= _snap; _i++) {                      \
            _type *_var = (_type*)                                      \
                _entries[_i].value;                                     \

/*
 * PHP_PHATHOM_HASH_FOREACH_CONCURRENT(hash, type, var)
 *
 * Concurrent modification is visible.
 *
 * Usage:
 *   PHP_PHATHOM_HASH_FOREACH_CONCURRENT(&my_hash, my_type_t, item) {
 *       // item is my_type_t*
 *   } PHP_PHATHOM_HASH_FOREACH_END();
 */
#define PHP_PHATHOM_HASH_FOREACH_CONCURRENT(_hash, _type, _var)         \
    do {                                                                \
        for (uint32_t _i = 1; _i <= (_hash)->entries.used; _i++) {      \
            _type *_var = (_type*)                                      \
                (_hash)->entries.data[_i].value;                        \

#define PHP_PHATHOM_HASH_FOREACH_END()                                  \
        }                                                               \
    } while (0)

#endif /* HAVE_PHATHOM_HASH_H */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
