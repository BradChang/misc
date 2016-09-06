PROGS=test1 test2 test3 test4 test5 test6 test7 test8 test9 test10
OBJS=$(patsubst %,%.o,$(PROGS))

CFLAGS = -I..
CFLAGS += -g
CFLAGS += -Wall -Wextra
LDFLAGS=

TEST_TARGET=run_tests
TESTS=./do_tests

all: $(OBJS) $(PROGS) $(TEST_TARGET) 

# static pattern rule: multiple targets 

$(OBJS) ../ringbuf.o: %.o: %.c
	$(CC) -c $(CFLAGS) $< 

$(PROGS): %: %.o ../ringbuf.o
	$(CC) -o $@ $(CFLAGS) $< ringbuf.o $(LDFLAGS)


run_tests: $(PROGS)
	perl $(TESTS)

.PHONY: clean

clean:	
	rm -f $(PROGS) $(OBJS) *.o test*.out 
