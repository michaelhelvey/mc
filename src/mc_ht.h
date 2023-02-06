#ifndef MC_HT_H
#define MC_HT_H

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef bool (*mc_ht_cmp_fn)(void *, void *);
typedef size_t (*mc_ht_hash_fn)(void *);

#ifndef MC_HASHTABLE_DEBUG
#define MC_HASHTABLE_DEBUG 0
#endif

#define log(...)                                                                                                       \
    if (MC_HASHTABLE_DEBUG) {                                                                                          \
        printf(__VA_ARGS__);                                                                                           \
    }

typedef struct {
    void *key;
    bool _occupied;
    bool _deleted;
    void *value;
} _mc_ht_node;

typedef enum {
    DELETE_NOT_FOUND,
    DELETE_MEM_RESULT,
} mc_ht_delete_result_type;

typedef struct {
    mc_ht_delete_result_type type;
    void *key;
    void *value;
} mc_ht_delete_result;

/* inner hashtable memory */
typedef struct {
    mc_ht_cmp_fn compare_fn;
    mc_ht_hash_fn hash_fn;

    size_t cap;
    size_t size;

    _mc_ht_node *buf;
} _mc_ht;

_mc_ht *_mc_ht_new(size_t cap, mc_ht_cmp_fn cmp_fn, mc_ht_hash_fn hash_fn) {
    _mc_ht_node *buf = (_mc_ht_node *)calloc(cap, sizeof(_mc_ht_node));
    _mc_ht *ht = (_mc_ht *)malloc(sizeof(_mc_ht));

    ht->cap = cap;
    ht->size = 0;
    ht->compare_fn = cmp_fn;
    ht->hash_fn = hash_fn;
    ht->buf = buf;

    return ht;
}

void _mc_ht_free(_mc_ht *table) {
    free(table->buf);
    free(table);
}

void _mc_ht_insert(_mc_ht *table, void *key, void *value) {
    size_t index = table->hash_fn(key) % table->cap;
    _mc_ht_node *node = &table->buf[index];

    // Note: if the table never resized, then this could result in an infinite
    // loop.  However, we guarantee that `insert` will always resize when
    // necessary.
    while (node->_occupied == true && table->compare_fn(node->key, key) != true) {
        // If we have a hash collision on the last index of the array, wrap
        // around to the beginning of the array
        index = (index + 1) % table->cap;
        node = &table->buf[index];
    }

    // At this point, node is guaranteed to point to an unoccupied slot, or an
    // existing slot with the same key (in the case of a replace operation).
    node->key = key;
    node->value = value;
    node->_occupied = true;
    table->size++;
}

_mc_ht_node *_mc_ht_get(_mc_ht *table, void *key) {
    size_t index = table->hash_fn(key) % table->cap;
    _mc_ht_node *node = &table->buf[index];

    while ((node->_occupied && table->compare_fn(node->key, key) != true) | node->_deleted) {
        // If we have a hash collision on the last index of the array, wrap
        // around to the beginning of the array
        index = (index + 1) % table->cap;
        node = &table->buf[index];

        // Note that we do not need to handle wrapping around in an infinite
        // loop as long as resizing is implemented.
    }

    return node;
}

/* user facing hashtable struct that owns the inner memory */
typedef struct {
    _mc_ht *_table;
} mc_hashtable;

mc_hashtable mc_hashtable_new(size_t cap, mc_ht_cmp_fn cmp_fn, mc_ht_hash_fn hash_fn) {
    _mc_ht *table = _mc_ht_new(cap, cmp_fn, hash_fn);
    return (mc_hashtable){
        ._table = table,
    };
}

void *mc_hashtable_get(mc_hashtable *table, void *key) {
    _mc_ht_node *node = _mc_ht_get(table->_table, key);

    if (node->_occupied) {
        return node->value;
    }

    return NULL;
}

int mc_hashtable_insert(mc_hashtable *table, void *key, void *value) {
    _mc_ht_insert(table->_table, key, value);

    // When the table is more than 50% full, resize it:
    if (table->_table->size < table->_table->cap / 2) {
        return 0;
    }

    _mc_ht *new_table;
    size_t new_size = table->_table->cap * 2;

    log("[mc_hashtable]: resizing from %lu to %lu\n", table->_table->cap, new_size);
    if ((new_table = _mc_ht_new(new_size, table->_table->compare_fn, table->_table->hash_fn)) == NULL) {
        return -1;
    }

    for (size_t i = 0; i < table->_table->cap; i++) {
        _mc_ht_node node = table->_table->buf[i];

        if (node._occupied) {
            _mc_ht_insert(new_table, node.key, node.value);
        }
    }

    _mc_ht_free(table->_table);
    table->_table = new_table;

    return 0;
}

/* Deletes a key from the table and returns a struct with pointers to the key
 * and value pointers.  The hashtable does not own this memory and so returns it
 * to the user for cleanup.  Note that discarding the result type will leak the
 * key and value memory. */
mc_ht_delete_result mc_hashtable_delete(mc_hashtable *table, void *key) {
    _mc_ht_node *node = _mc_ht_get(table->_table, key);

    if (node->_occupied == false) {
        return (mc_ht_delete_result){
            .type = DELETE_NOT_FOUND,
            .key = NULL,
            .value = NULL,
        };
    }

    mc_ht_delete_result result = {
        .type = DELETE_MEM_RESULT,
        .key = node->key,
        .value = node->value,
    };

    node->_deleted = true;
    node->_occupied = false;
    node->key = NULL;

    return result;
}

/* Implements the djb2 hash function for strings */
size_t mc_djb2(void *ptr) {
    char *str = (char *)ptr;
    size_t hash = 5381;
    int c;

    while ((c = *str++))
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */

    return hash;
}

#ifdef __cplusplus
}
#endif

#endif // MC_HT_H
