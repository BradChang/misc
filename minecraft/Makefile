SUBDIRS=nbtlib nbtool
all: $(SUBDIRS)

.PHONY: clean $(SUBDIRS)

$(SUBDIRS):
	$(MAKE) -C $@

clean:
	for d in $(SUBDIRS); do $(MAKE) -C $$d $@; done
