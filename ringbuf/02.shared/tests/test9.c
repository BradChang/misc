#include <stdio.h>
#include <unistd.h>
#include "shr_ring.h"

/* this test shows that partial writes are rejected; always full or error */

char *ring = "test.ring";
char *data = "abcdefghi";

char out[10];

int main() {
 struct shr *s=NULL;
 ssize_t nr;
 int rc = -1;

 unlink(ring);
 if (shr_init(ring, 4, 0) < 0) goto done; /* only 4 capacity */

 s = shr_open(ring);
 if (s == NULL) goto done;

 printf("writing ...");
 nr = shr_write(s, &data[0], 3);    /* write 3 ok */
 printf("%s\n", (nr < 0) ? "fail" : "ok");

 printf("writing ...");
 nr = shr_write(s, &data[3], 3); /* fails- can't write 3 more */
 printf("%s\n", (nr < 0) ? "fail" : "ok");

 printf("writing ...");
 nr = shr_write(s, &data[3], 2); /* fails- can't write 2 more */
 printf("%s\n", (nr < 0) ? "fail" : "ok");

 printf("writing ...");
 nr = shr_write(s, &data[3], 1); /* ok - can write 1 more */
 printf("%s\n", (nr < 0) ? "fail" : "ok");

 printf("reading ...");
 nr = shr_read(s, out, sizeof(out));
 if (nr < 0) goto done;
 printf("read %ld bytes\n", (long)nr);
 if (nr > 0) printf("%.*s\n", (int)nr, out);

 rc = 0;

done:
 printf("end\n");
 if (s) shr_close(s);
 return rc;
}
