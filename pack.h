/*
 *  serialize data for sending over a socket.
 *
 *  from http://beej.us/guide/bgnet/examples/pack2b.c
 *
 *  Example:
 *
 *      unsigned char buf[256];
 *      unsigned int size;
 *
 *      size = pack(buf,"HL",2,4);
 *      assert (size == 6);
 *
 *      uint16_t a;
 *      uint32_t b;
 *
 *      unpack(buf,"HL",&a,&b);
 *      printf("unpacked: %i %i\n", a, b);
 */

/*
** pack() -- store data dictated by the format string in the buffer
**
**   bits |signed   unsigned   float   string
**   -----+----------------------------------
**      8 |   c        C         
**     16 |   h        H         f
**     32 |   l        L         d
**     64 |   q        Q         g
**      - |                               s
**
**  (16-bit unsigned length is automatically prepended to strings)
*/ 
unsigned int pack(unsigned char *buf, char *format, ...);
/*
** unpack() -- unpack data dictated by the format string into the buffer
**
**   bits |signed   unsigned   float   string
**   -----+----------------------------------
**      8 |   c        C         
**     16 |   h        H         f
**     32 |   l        L         d
**     64 |   q        Q         g
**      - |                               s
**
**  (string is extracted based on its stored length, but 's' can be
**  prepended with a max length)
*/
void unpack(unsigned char *buf, char *format, ...);
