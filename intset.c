#include <string.h>
#include <stdlib.h>
#include "intset.h"

struct intset *intset_init()
{
    /* create and return a pointer to an intset. `size`
     * is the maximum size of the set. */
    struct intset *s = malloc(sizeof (struct intset));
    s->values = malloc(0);
    s->size = 0;
    s->entries = 0;
    return s;
}

void intset_free(struct intset *s)
{
    /* free an intset */
    free(s->values);
    free(s);
}

int intset_add(struct intset *s, int value)
{
    /* add an integer to the set. If the integer already exists,
     * returns -1.*/
    if (intset_in(s, value)) return -1;
    if (s->entries == s->size) {
	/* resize array. inspired by python's list implementation */
	s->size += ((s->size + 1) >> 3) + ((s->size + 1) < 9 ? 3 : 6);
	s->values = realloc(s->values,(sizeof(int))*s->size);
    }
    s->values[s->entries++] = value;
    return 0;
}

int intset_in(struct intset *s, int value)
{
    /* returns 1 if value is in the intset, otherwise zero. */
    int i;
    for (i = 0; i < s->entries; i++) {
        if (s->values[i] == value) return 1;
    }
    return 0;
}

int intset_del(struct intset *s, int value)
{
    /* delete an int from the intset. Returns 0 if successfully deleted,
     * else -1. */
    int i;
    for (i = 0; i < s->entries; i++) {
        if (s->values[i] == value) {
            memmove(s->values+i,s->values+i+(sizeof (int)),
                    (s->entries - i - 1)*(sizeof (int)));
	    s->entries -= 1;
            return 0;
        }
    }
    return -1;
}
