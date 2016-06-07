all: driver gen

LDFLAGS=-lncurses

driver.o: driver.c
	$(CC) -c $(CFLAGS) $<

gen.o: gen.c
	$(CC) -c $(CFLAGS) $<

tpl.o: tpl.c
	$(CC) -c $(CFLAGS) $<

gen: tpl.o gen.o
	$(CC) -o $@ $^ 

driver: tpl.o driver.o
	$(CC) -o $@ $^ $(LDFLAGS)

.PHONY: clean

clean: 
	rm -f driver gen *.o
