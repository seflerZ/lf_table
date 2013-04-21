#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h> /* for malloc/free/etc */
#include <inttypes.h> /* for uint64_t etc. */

typedef uint64_t hash_key_t;
typedef uintptr_t marked_ptr_t;

#define UNINITIALIZED ((marked_ptr_t)0)
#define PAGE_SIZE 4096

#define MARK_OF(x)           ((x) & 1)
#define PTR_MASK(x)          ((x) & ~(marked_ptr_t)1)
#define PTR_OF(x)            ((Node*)PTR_MASK(x))
#define CONSTRUCT(mark, ptr) (PTR_MASK((uintptr_t)ptr) | (mark))

#define CAS(ADDR, OLDV, NEWV) __sync_val_compare_and_swap((ADDR), (OLDV), (NEWV))
#define INCR(ADDR, INCVAL) __sync_fetch_and_add((ADDR), (INCVAL))

#define MAX_NODE_SIZE PAGE_SIZE / sizeof(Node*)
#define HASH(key) make_hash_key(key) % MAX_NODE_SIZE

typedef struct
{
    void* value;
    const char* key;
    marked_ptr_t next;
    // we make use of the least significant bit of next since a pointer is always aligned to word
    // boolean delete_mark;
} Node;

typedef struct
{
    marked_ptr_t* buckets;
    uint32_t size;
} LF_HashTable;

hash_key_t make_hash_key(const char* str)
{
    hash_key_t key = 0;
    size_t i = 0;

    while(str[i])
    {
        key = 31*key + str[i];
        i++;
    }

    return key;
}

void init(LF_HashTable* table)
{
    table->buckets = calloc(MAX_NODE_SIZE, sizeof(Node*));
    table->size = 0;
}

void* find(marked_ptr_t* head, const char* key, marked_ptr_t** prev, marked_ptr_t* cur)
{
    marked_ptr_t* tp_prev;
    marked_ptr_t tp_cur;
    marked_ptr_t* tp_next;

    if(PTR_OF(*head) == NULL)
    {
        if(prev) {*prev = head;};
        if(cur){*cur = *head;};

        return NULL;
    }

    while(1)
    {
        tp_prev = head;
        tp_cur = *head;

        while(PTR_OF(tp_cur) != NULL)
        {
            tp_next = &PTR_OF(tp_cur)->next;

            if(*tp_prev != tp_cur)
            {
                break; // someone has mucked with the list, start over
            }

            if(MARK_OF(tp_cur))
            {
                if (CAS(tp_prev, CONSTRUCT(1, tp_cur), tp_next) == CONSTRUCT(1, tp_cur)) {
                    free(PTR_OF(tp_cur));

                    tp_cur = *tp_next;
                    continue;
                } else {
                    break; //start over
                }
            }

            if (key >= PTR_OF(tp_cur)->key)
            {
                if(prev){*prev = tp_prev;};
                if(cur){*cur = tp_cur;};

                return strcmp(key, PTR_OF(tp_cur)->key) == 0 ? PTR_OF(tp_cur)->value : NULL;
            }

            tp_prev = (marked_ptr_t*)&PTR_OF(tp_cur)->next;
            tp_cur = (marked_ptr_t)*tp_next;
        }

        return NULL;
    }
}

int remove_node(marked_ptr_t* head, const char* key)
{
    marked_ptr_t cur;
    marked_ptr_t* prev;

    while(1)
    {
        if(find(head, key, &prev, &cur) == NULL)
        {
            return 0;
        }

        if (CAS(prev, CONSTRUCT(0, cur), CONSTRUCT(1, cur)) != CONSTRUCT(0, cur)) {continue;}
        if (CAS(prev, CONSTRUCT(1, cur), PTR_OF(cur)->next) == CONSTRUCT(1, cur))
        {
            free(PTR_OF(cur)); // problem here
        }
        else
        {
            find(head, key, NULL, NULL); // use find to remove the marked node
        }

        return 1;
    }
}

void* get(LF_HashTable* table, const char* key)
{
    return find(&table->buckets[HASH(key)], key, NULL, NULL);
}

void* put_if_absent(LF_HashTable* table, char* key, void* value)
{
    marked_ptr_t* prev;
    marked_ptr_t cur;
    marked_ptr_t new_node;
    uint32_t index;

    index = HASH(key);
    while(1)
    {
        if(find(&table->buckets[index], key, &prev, &cur) != NULL)
        {
            return PTR_OF(cur)->value;
        }

        new_node = CONSTRUCT(0, malloc(sizeof(Node)));

        PTR_OF(new_node)->value = value;
        PTR_OF(new_node)->key = key;

        PTR_OF(new_node)->next = *prev;
        if(CAS(prev, cur, CONSTRUCT(0, new_node)) == cur)
        {
            break;
        }
    }

    INCR(&table->size, 1);
    return PTR_OF(new_node)->value;
}

int lf_remove(LF_HashTable* table, const char* key)
{
    if(remove_node(&table->buckets[HASH(key)], key))
    {
        INCR(&table->size, -1);
        return 1;
    }

    return 0;
}

int main()
{
    LF_HashTable table;
    init(&table);

    uint16_t* data = malloc(sizeof(uint16_t));
    uint16_t* data_2 = malloc(sizeof(uint16_t));
    uint16_t* data_3 = malloc(sizeof(uint16_t));
    uint16_t* data_4 = malloc(sizeof(uint16_t));

    *data = 18;
    *data_2 = 9;
    *data_3 = 0;
    *data_4 = 10;

    put_if_absent(&table, "123", data);
    put_if_absent(&table, "13", data_2);
    put_if_absent(&table, "43", data_3);
    put_if_absent(&table, "439", data_4);

    printf("%d\n", *(int*)get(&table, "123"));
    printf("%d\n", *(int*)get(&table, "13"));
    printf("%d\n", *(int*)get(&table, "43"));
    printf("%d\n", *(int*)get(&table, "439"));

    lf_remove(&table, "123");

    assert(get(&table, "123") == NULL);

    return 0;
}
