TEST_TARGET=run_tests
TESTS=./do_tests

all: $(TEST_TARGET) 

run_tests: $(PROGS)
	perl $(TESTS)

.PHONY: clean

clean:
	rm -f *.out 
