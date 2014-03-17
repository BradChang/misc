SRCS=$(wildcard *.c)
OBJS=$(patsubst %.c,%,$(SRCS))
all: $(OBJS)
LDFLAGS=-lpcap

compile_filter: compile_filter.c
	$(CC) -o $@ $< -lpcap

decode_tcp_headers: decode_tcp_headers.c
	$(CC) -o $@ $< -lpcap

hardcoded_filter: hardcoded_filter.c
	$(CC) -o $@ $< -lpcap

iface_stats: iface_stats.c
	$(CC) -o $@ $< -lpcap

multi_mode: multi_mode.c
	$(CC) -o $@ $< -lpcap

pkt_lengths: pkt_lengths.c
	$(CC) -o $@ $< -lpcap

pkt_lengths_file: pkt_lengths_file.c
	$(CC) -o $@ $< -lpcap

signal_driven_capture: signal_driven_capture.c
	$(CC) -o $@ $< -lpcap


.PHONY: clean

clean:
	rm -f $(OBJS) *.o
