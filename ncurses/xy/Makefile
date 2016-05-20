all: driver gen

CFLAGS=-I../
LDFLAGS=-lncurses

driver.o: driver.c
	$(CC) -c $(CFLAGS) $<

gen.o: gen.c
	$(CC) -c $(CFLAGS) $<

tpl.o: ../tpl.c
	$(CC) -c $(CFLAGS) $<

gen: gen.o tpl.o
	$(CC) -o $@ $^ 

driver: driver.o tpl.o
	$(CC) -o $@ $^ $(LDFLAGS)

.PHONY: clean

clean: 
	rm -f driver gen *.o
