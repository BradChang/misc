OBJS=libcontrolport.a 
OBJS+=controlsh
all: $(OBJS)
CFLAGS=-I./include -I./tpl
CFLAGS+=-g
#CFLAGS+=-O2

controlsh.o: controlsh.c
	$(CC) $(CFLAGS) -c $< 

controlsh: controlsh.o tpl/tpl.o
	$(CC) $(CFLAGS) -o $@ $^ -lreadline 

libcontrolport.a: libcontrolport.o tpl/tpl.o
	ar r $@ $^

libcontrolport.o: libcontrolport.c libcontrolport.h
	$(CC) $(CFLAGS) -c $< 

.PHONY: clean tests

tests:
	make -C tests

clean:
	rm -f *.o $(OBJS) tpl/*.o
	make -C tests clean
