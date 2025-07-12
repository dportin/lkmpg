obj-m += procfs-buffer.o
ccflags-y += -Wall -Wextra -Werror -Wno-unused-parameter

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	rm -rf .cache
	rm -f .gdb_history
	rm -f .gdb_history
