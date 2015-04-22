#include <assert.h>
#include <string.h>
#include "../src/buf.h"

int main()
{
    char a[256] = "Hello world!";
    char b[256];

    struct buffer *buf = buf_init(1000);

    assert(buf_write(buf, a, sizeof a) == 0);
    assert(buf_read(buf, b, (sizeof a)*2) == -1);
    assert(buf_read(buf, b, sizeof a) == (sizeof a));
    assert(strcmp(a,b) == 0);
    assert(BUF_LEN(buf) == 0);

    char c[1001];
    assert(buf_write(buf, c, sizeof c) == -1);
    assert(BUF_LEN(buf) == 0);
    buf_free(buf);
    return 0;
}
