PROGS = ezip
OBJS=ezip.o eecode.o
all: $(PROGS)

CFLAGS += -g
CFLAGS += -Wall 
LDFLAGS = -lm

$(OBJS): %.o: %.c eecode.h
	$(CC) -c $(CFLAGS) $< 

ezip: $(OBJS)
	$(CC) -o $@ $(CFLAGS) $(OBJS) $(LDFLAGS)

.PHONY: clean

clean:
	rm -f $(OBJS) $(PROGS)

