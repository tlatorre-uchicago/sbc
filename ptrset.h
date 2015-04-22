#ifndef INTSET_H
#define INTSET_H

/* A not-so efficient implementation of a pointer set. Uses a dynamic
 * array to store a set of pointers.
 *
 * Example:
 *
 * char a[100];
 * char b[100];
 *
 * struct ptrset *s = ptrset_init();
 * ptrset_add(s,a);
 * ptrset_in(s,a);      # returns 1
 * ptrset_in(s,b);      # returns 0
 * ptrset_add(s,b);
 * ptrset_del(s,a);
 * ptrset_in(s,a);      # returns 0
 * ptrset_in(s,b);      # returns 1
 * ptrset_free(s);      # make sure to free it!
 *
 */

struct ptrset {
    void **values;
    unsigned int entries;
    unsigned int size;
};

struct ptrset *ptrset_init();
void ptrset_free(struct ptrset *s);
void ptrset_free_all(struct ptrset *s);
int ptrset_add(struct ptrset *s, void *value);
void *ptrset_pop(struct ptrset *s);
int ptrset_in(struct ptrset *s, void *value);
int ptrset_del(struct ptrset *s, void *value);

#endif
