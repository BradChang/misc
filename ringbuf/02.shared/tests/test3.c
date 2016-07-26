#include <stdio.h>
#include <unistd.h>
#include "shr_ring.h"

char *ring = "test.ring";
char *data = "abcdefghi";

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

 printf("writing ...");
 if (shr_write(s, &data[3], 3) < 0) goto done;
 printf("ok\n");

 /* this should fail */
 printf("writing ...");
 if (shr_write(s, &data[6], 1) < 0) goto done;
 printf("ok\n");

 rc = 0;

done:
 printf("end\n");
 if (s) shr_close(s);
 return rc;
}
