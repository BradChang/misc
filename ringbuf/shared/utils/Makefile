PROGS = $(patsubst %.c,%,$(wildcard *.c))
all: $(PROGS)

CFLAGS += -g
CFLAGS += -Wall 
CFLAGS += -I..
LDFLAGS = -L.. -lshr_ring

$(OBJS): %.o: %.c
	$(CC) -c $(CFLAGS) $< 

$(PROGS): %: %.o ../libshr_ring.a
	$(CC) -o $@ $(CFLAGS) $< $(LDFLAGS)

../libshr_ring.a:
	@echo "please run make from parent directory"
	@/bin/false

.PHONY: clean tests

tests:
	make -C tests 

clean:
	rm -f *.o $(PROGS) ring.*
	make -C tests $@

