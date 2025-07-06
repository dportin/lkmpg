obj-m += procfs-inode.o
ccflags-y += -Wall -Wextra -Werror -Wno-unused-parameter

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	rm -rf .cache
