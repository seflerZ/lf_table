#include "lf_hash.h"

void* lf_table_find(marked_ptr_t* head, hash_key_t key, marked_ptr_t** prev, marked_ptr_t* cur)
{
    marked_ptr_t* tp_prev;
    marked_ptr_t tp_cur;
    marked_ptr_t* tp_next;

    hash_key_t cur_key;
    void* cur_value;

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

        while(1)
        {
            if (PTR_OF(tp_cur) == NULL)
            {
                if(prev){*prev = tp_prev;};
                if(cur){*cur = tp_cur;};

                return NULL;
            }

            tp_next = &PTR_OF(tp_cur)->next;

            cur_key = PTR_OF(tp_cur)->key;
            cur_value = PTR_OF(tp_cur)->value;

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

            if (key >= cur_key)
            {
                if(prev){*prev = tp_prev;};
                if(cur){*cur = tp_cur;};

                return key == cur_key ? cur_value : NULL;
            }

            tp_prev = tp_next;
            tp_cur = *tp_next;
        }
    }
}

int remove_node(marked_ptr_t* head, hash_key_t key)
{
    marked_ptr_t cur;
    marked_ptr_t* prev;

    while(1)
    {
        if(lf_table_find(head, key, &prev, &cur) == NULL)
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
            lf_table_find(head, key, NULL, NULL); // use find to remove the marked node
        }

        return 1;
    }
}

void* lf_table_get(LF_HashTable* table, void* key)
{
    return lf_table_find(&table->buckets[HASH(key)], CONSTRUCT(0,key), NULL, NULL);
}

void* lf_table_put_if_absent(LF_HashTable* table, void* key, void* value)
{
    marked_ptr_t* prev;
    marked_ptr_t cur;
    marked_ptr_t new_node;
    uint32_t index;

    index = HASH(key);
    while(1)
    {
        if(lf_table_find(&table->buckets[index], CONSTRUCT(0, key), &prev, &cur) != NULL)
        {
            return PTR_OF(cur)->value;
        }

        new_node = CONSTRUCT(0, malloc(sizeof(Node)));

        PTR_OF(new_node)->value = value;
        PTR_OF(new_node)->key = CONSTRUCT(0, key);

        PTR_OF(new_node)->next = *prev;
        if(CAS(prev, cur, CONSTRUCT(0, new_node)) == cur)
        {
            break;
        }
    }

    INCR(&table->size, 1);
    return NULL;
}

int lf_table_remove(LF_HashTable* table, void* key)
{
    if(remove_node(&table->buckets[HASH(key)], CONSTRUCT(0,key)))
    {
        INCR(&table->size, -1);
        return 1;
    }

    return 0;
}
