#ifndef LF_HASH_H_INCLUDED
#define LF_HASH_H_INCLUDED

#include <string.h>
#include <stdlib.h> /* for malloc/free/etc */
#include <inttypes.h> /* for uint64_t etc. */

typedef uintptr_t hash_key_t;
typedef uintptr_t marked_ptr_t;

#define UNINITIALIZED ((marked_ptr_t)0)
#define PAGE_SIZE 4096
#define TABLE_INITIALIZER {{0},0}

#define MARK_OF(x)           ((x) & 1)
#define PTR_MASK(x)          ((x) & ~(marked_ptr_t)1)
#define PTR_OF(x)            ((Node*)PTR_MASK(x))
#define CONSTRUCT(mark, ptr) (PTR_MASK((uintptr_t)ptr) | (mark))

#define CAS(ADDR, OLDV, NEWV) __sync_val_compare_and_swap((ADDR), (OLDV), (NEWV))
#define INCR(ADDR, INCVAL) __sync_fetch_and_add((ADDR), (INCVAL))

#define MAX_NODE_SIZE PAGE_SIZE / sizeof(Node*)
#define HASH(key) CONSTRUCT(0,key) % MAX_NODE_SIZE

typedef struct
{
    void* value;
    hash_key_t key; //NOTE: we use the pointer address as the key not the characters there
    marked_ptr_t next;
    // we make use of the least significant bit of next since a pointer is always aligned to word
    // boolean delete_mark;
} Node;

typedef struct
{
    marked_ptr_t buckets[MAX_NODE_SIZE];
    uint32_t size;
} LF_HashTable;

void* lf_table_get(LF_HashTable*,void*);

void* lf_table_put_if_absent(LF_HashTable*,void*,void*);

int lf_table_remove(LF_HashTable*,void*);

#endif // LF_HASH_H_INCLUDED
