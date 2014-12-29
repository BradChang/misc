TESTS=$(patsubst %.c,%,$(wildcard test*.c))
OBJS+=$(TESTS)
all: $(OBJS)
CFLAGS=-I..
CFLAGS+=-g
#CFLAGS+=-O2
LDFLAGS=-L.. -lcontrolport

# test apps

test%: test%.o
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS) 

test%.o: test%.c
	$(CC) $(CFLAGS) -c $< 

.PHONY: clean 

clean:
	rm -f *.o $(OBJS) 
