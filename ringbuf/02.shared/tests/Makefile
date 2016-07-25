PROGS=test1 
OBJS=$(patsubst %,%.o,$(PROGS))

CFLAGS = -I..
CFLAGS += -g
CFLAGS += -Wall -Wextra
LDFLAGS=

TEST_TARGET=run_tests
TESTS=./do_tests

all: $(OBJS) $(PROGS) $(TEST_TARGET) 

# static pattern rule: multiple targets 

$(OBJS) ../shr_ring.o: %.o: %.c
	$(CC) -o $@ -c $(CFLAGS) $< 

$(PROGS): %: %.o ../shr_ring.o
	$(CC) -o $@ $(CFLAGS) $< ../shr_ring.o $(LDFLAGS)


run_tests: $(PROGS)
	perl $(TESTS)

.PHONY: clean

clean:	
	rm -f $(PROGS) $(OBJS) *.o test*.out 
