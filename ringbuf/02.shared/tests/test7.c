#include <stdio.h>
#include <unistd.h>
#include "shr_ring.h"

char *ring = "test.ring";
char *data = "abcdefghi";

char out[10];

int main() {
 struct shr *s=NULL;
 int rc = -1;

 unlink(ring);
 if (shr_init(ring, 6, 0) < 0) goto done;

 s = shr_open(ring);
 if (s == NULL) goto done;

 printf("writing ...");
 if (shr_write(s, &data[0], 3) < 0) goto done;
 printf("ok\n");

 printf("reading ...");
 ssize_t nr;
 nr = shr_read(s, out, sizeof(out));
 if (nr < 0) goto done;
 printf("read %ld bytes\n", (long)nr);
 if (nr > 0) printf("%.*s\n", (int)nr, out);

 printf("writing ...");
 if (shr_write(s, &data[3], 3) < 0) goto done;
 printf("ok\n");

 printf("reading ...");
 nr = shr_read(s, out, sizeof(out));
 if (nr < 0) goto done;
 printf("read %ld bytes\n", (long)nr);
 if (nr > 0) printf("%.*s\n", (int)nr, out);

 printf("writing ...");
 if (shr_write(s, &data[6], 3) < 0) goto done;
 printf("ok\n");

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
