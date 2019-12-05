COMPILER = g++
FILESYSTEM_FILES = fuse.c cache.cc cache.hh

build: $(FILESYSTEM_FILES)
	$(COMPILER) $(FILESYSTEM_FILES) -o fuse `pkg-config fuse --cflags --libs` -pg
clean:
	rm fuse
	rm *~

