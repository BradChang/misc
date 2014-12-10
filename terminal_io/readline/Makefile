PROGS = readline select-readline
CFLAGS=-g
all: $(PROGS)

readline: readline.c
	$(CC) $(CFLAGS) -o $@ $< -lreadline

select-readline: select-readline.c
	$(CC) $(CFLAGS) -o $@ $< -lreadline

.PHONY: clean
clean:
	rm -f $(PROGS) *.o
