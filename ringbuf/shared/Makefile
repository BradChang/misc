OBJS = libshr_ring.a

all: $(OBJS) utils

shr_ring.o: shr_ring.c shr_ring.h
	$(CC) $(CFLAGS) -c $< 

libshr_ring.a: shr_ring.o
	ar rf $@ $<

.PHONY: clean tests utils

clean:
	rm -f $(OBJS) *.o
	make -C tests $@
	make -C utils $@

tests utils:
	make -C $@
