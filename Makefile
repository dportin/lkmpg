SUBDIRS := 01-hello-world
SUBDIRS += 02-hello-world
SUBDIRS += 03-hello-world
SUBDIRS += 04-chardev
SUBDIRS += 05-procfs-static
SUBDIRS += 06-procfs-buffer
SUBDIRS += 07-procfs-inode
SUBDIRS += 08-procfs-seqfile
SUBDIRS += 09-sysfs-attrs

BEAR ?= 0

ifeq ($(BEAR), 1)
	RUN_MAKE = bear -- $(MAKE)
else
	RUN_MAKE = $(MAKE)
endif

.PHONY: all
all:
	for DIR in $(SUBDIRS); do (cd $$DIR && $(RUN_MAKE)); done

.PHONY: clean
clean:
	for DIR in $(SUBDIRS); do (cd $$DIR && $(MAKE) clean); done
