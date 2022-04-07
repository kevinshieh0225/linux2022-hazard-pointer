#define atomic_load(src) __atomic_load_n(src, __ATOMIC_SEQ_CST)
#define atomic_store(dst, val) __atomic_store(dst, val, __ATOMIC_SEQ_CST)
#define atomic_exchange(ptr, val) \
    __atomic_exchange_n(ptr, val, __ATOMIC_SEQ_CST)
#define atomic_cas(dst, expected, desired)                                 \
    __atomic_compare_exchange(dst, expected, desired, 0, __ATOMIC_SEQ_CST, \
                              __ATOMIC_SEQ_CST)

#include <stdint.h>

#define LIST_ITER(head, node) \
    for (node = atomic_load(head); node; node = atomic_load(&node->next))

/* hazard ptr struct
 * 
 * @ptr: resouce currently used by thread
 * @next: linked-list structure
 */
typedef struct __hp {
    uintptr_t ptr;
    struct __hp *next;
} hp_t;

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

/* Allocate a new node with specified value and append to list
 * 
 * @head: hplist head
 * @ptr: add tracted resource
 * @return: the node containing the newly added value.
 */
static hp_t *list_append(hp_t **head, uintptr_t ptr)
{
    hp_t *new = calloc(1, sizeof(hp_t));
    if (!new)
        return NULL;

    new->ptr = ptr;
    hp_t *old = atomic_load(head);

    do {
        new->next = old;
    } while (!atomic_cas(head, &old, &new));

    return new;
}

/* Attempt to find an empty node to store value, otherwise append a new node.
 *
 * @head: hplist head
 * @ptr: add tracted resource
 * @return: the node containing the newly added value.
 */
hp_t *list_insert_or_append(hp_t **head, uintptr_t ptr)
{
    hp_t *node;
    bool need_alloc = true;

    LIST_ITER (head, node) {
        uintptr_t expected = atomic_load(&node->ptr);
        if (expected == 0 && atomic_cas(&node->ptr, &expected, &ptr)) {
            need_alloc = false;
            break;
        }
    }

    if (need_alloc)
        node = list_append(head, ptr);

    return node;
}

/* Remove a node from the hplist with the specified value
 * 
 * @head: hplist head
 * @ptr: removed target
 * @return: if successfully remove return true,
 *          else if [1. ptr not in list 2. ptr has been remove return] false
 */
bool list_remove(hp_t **head, uintptr_t ptr)
{
    hp_t *node;
    const uintptr_t nullptr = 0;

    LIST_ITER (head, node) {
        uintptr_t expected = atomic_load(&node->ptr);
        if (expected == ptr && atomic_cas(&node->ptr, &expected, &nullptr))
            return true;
    }

    return false;
}

/* Returns 1 if the list currently contains an node with the specified value
 * 
 * @head: hplist head
 * @ptr: tracted resource
 * @return: if successfully remove return true,
 *          else if [1. ptr not in list 2. ptr has been remove return] false
 */
bool list_contains(hp_t **head, uintptr_t ptr)
{
    hp_t *node;

    LIST_ITER (head, node) {
        if (atomic_load(&node->ptr) == ptr)
            return true;
    }

    return false;
}

/* Frees all the nodes in a list - NOT THREAD SAFE */
void list_free(hp_t **head)
{
    hp_t *cur = *head;
    while (cur) {
        hp_t *old = cur;
        cur = cur->next;
        free(old);
    }
}

