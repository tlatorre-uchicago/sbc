#ifndef INTSET_H
#define INTSET_H

/* A not-so efficient implementation of an integer set. Uses a dynamic
 * array to store a set of integers.
 *
 * Example:
 *
 * struct intset *s = intset_init();
 * intset_add(s,10);
 * intset_in(s,10);     # returns 1
 * intset_in(s,1);      # returns 0
 * intset_del(s,10);
 * intset_in(s,10);     # returns 0
 * intset_free(s);      # make sure to free it!
 *
 */

struct intset {
    int *values;
    unsigned int entries;
    unsigned int size;
};

struct intset *intset_init();
void intset_free(struct intset *s);
int intset_add(struct intset *s, int value);
int intset_in(struct intset *s, int value);
int intset_del(struct intset *s, int value);

#endif
