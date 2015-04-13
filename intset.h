#ifndef INTSET_H
#define INTSET_H

struct intset {
    int *values;
    unsigned int entries;
    unsigned int size;
};

struct intset *intset_init(unsigned int size);
void intset_free(struct intset *s);
int intset_add(struct intset *s, int value);
int intset_in(struct intset *s, int value);
int intset_del(struct intset *s, int value);

#endif
