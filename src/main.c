#include <assert.h>
#include <stdio.h>
#include <string.h>
#define MC_HASHTABLE_DEBUG 1

#include "mc_ht.h"

bool compare_strings(void *str1, void *str2) {
    return strcmp((char *)str1, (char *)str2) == 0;
}

void test_ht() {
    // Test basic inserts, gets, & deletes
    mc_hashtable table = mc_hashtable_new(1, &compare_strings, &mc_djb2);
    mc_hashtable_insert(&table, (void *)"name", (void *)"pepe the frog");
    mc_hashtable_insert(&table, (void *)"genre", (void *)"Black Metal");

    assert(strcmp((char *)mc_hashtable_get(&table, (void *)"name"), "pepe the frog") == 0);
    assert(strcmp((char *)mc_hashtable_get(&table, (void *)"genre"), "Black Metal") == 0);

    // keys and values are const char*, so it is safe to leak them:
    mc_hashtable_delete(&table, (void *)"genre");
    assert((char *)mc_hashtable_get(&table, (void *)"genre") == NULL);

    mc_hashtable_insert(&table, (void *)"genre", (void *)"Djent");
    assert(strcmp((char *)mc_hashtable_get(&table, (void *)"genre"), "Djent") == 0);
}

int main(void) {
    test_ht();
}
