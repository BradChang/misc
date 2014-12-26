TESTS=$(patsubst %.c,%,$(wildcard test*.c))
OBJS=libcontrolport.a 
OBJS+=$(TESTS)
OBJS+=controlsh
all: $(OBJS)
CFLAGS=-I./include -I./tpl
CFLAGS+=-g
#CFLAGS+=-O2
TESTS_LDFLAGS=-L. -lcontrolport

controlsh.o: controlsh.c
	$(CC) $(CFLAGS) -c $< 

controlsh: controlsh.o tpl/tpl.o
	$(CC) $(CFLAGS) -o $@ $^ -lreadline 

libcontrolport.a: libcontrolport.o tpl/tpl.o
	ar r $@ $^

libcontrolport.o: libcontrolport.c libcontrolport.h
	$(CC) $(CFLAGS) -c $< 

# test apps

test%: test%.o libcontrolport.a
	$(CC) $(CFLAGS) -o $@ $< $(TESTS_LDFLAGS) $(LDFLAGS) 

test%.o: test%.c
	$(CC) $(CFLAGS) -c $< 

.PHONY: clean 

clean:
	rm -f *.o $(OBJS) tpl/*.o
