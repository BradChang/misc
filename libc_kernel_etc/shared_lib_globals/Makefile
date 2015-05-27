all: libshlglb.so useshlglb

useshlglb: useshlglb.o
	gcc -o $@ $< -L. -lshlglb

useshlglb.o: useshlglb.c
	gcc -c $< 

libshlglb.o : libshlglb.c
	gcc -fpic -c $<

libshlglb.so : libshlglb.o
	gcc -shared -o $@ $<

clean:
	rm -f *.o *.so useshlglb
