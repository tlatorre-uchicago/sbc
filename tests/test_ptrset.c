#include <assert.h>
#include <stdlib.h>
#include "../src/ptrset.h"

int main()
{
    char a[100];
    char b[100];
    struct ptrset *s = ptrset_init();

    ptrset_add(s, a);
    assert(ptrset_in(s,b) == 0);
    assert(ptrset_in(s,a) == 1);
    ptrset_del(s, a);
    ptrset_add(s, b);
    assert(ptrset_in(s,b) == 1);
    assert(ptrset_in(s,a) == 0);
    assert(ptrset_add(s, b) == -1);

    void *ptr;
    /* pop all the elements and we should get NULL back */
    while ((ptr = ptrset_pop(s)));
    assert(s->entries == 0);

    assert(ptrset_add(s,a) == 0);
    assert(ptrset_add(s,b) == 0);
    assert(ptrset_popleft(s) == a);
    assert(s->entries == 1);
    assert(ptrset_popleft(s) == b);
    assert(s->entries == 0);
    assert(ptrset_popleft(s) == NULL);

    /* now we just malloc a bunch of stuff and make sure it gets free'd */
    int i;
    for (i = 0; i < 1000; i++) {
        ptrset_add(s, malloc(256));
    }

    assert(s->entries = 1000);
    ptrset_free_all(s);
    return 0;
}
