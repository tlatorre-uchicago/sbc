#include <string.h>
#include <stdlib.h>
#include "ptrset.h"

struct ptrset *ptrset_init()
{
    /* create and return a pointer to a ptrset. */
    struct ptrset *s = malloc(sizeof (struct ptrset));
    s->values = malloc(0);
    s->size = 0;
    s->entries = 0;
    return s;
}

void ptrset_free(struct ptrset *s)
{
    free(s->values);
    free(s);
}

int ptrset_add(struct ptrset *s, void *value)
{
    /* add a pointer to the set. */
    if (ptrset_in(s, value)) return -1;
    if (s->entries == s->size) {
        /* resize array. inspired by python's list implementation */
        s->size += ((s->size + 1) >> 3) + ((s->size + 1) < 9 ? 3 : 6);
        s->values = realloc(s->values,(sizeof(void *))*s->size);
    }
    s->values[s->entries++] = value;
    return 0;
}

void *ptrset_pop(struct ptrset *s) {
    if (s->entries > 0) {
        return s->values[--s->entries];
    } else {
        return NULL;
    }
}

int ptrset_in(struct ptrset *s, void *value)
{
    /* returns 1 if value is in the ptrset, otherwise zero. */
    int i;
    for (i = 0; i < s->entries; i++) {
        if (s->values[i] == value) return 1;
    }
    return 0;
}

int ptrset_del(struct ptrset *s, void *value)
{
    /* delete a pointer from the ptrset. Returns 0 if successfully deleted,
     * else -1. */
    int i;
    for (i = 0; i < s->entries; i++) {
        if (s->values[i] == value) {
            memmove(s->values+i,s->values+i+1,
                    (s->entries - i - 1)*(sizeof (void *)));
            s->entries -= 1;
            return 0;
        }
    }
    return -1;
}
