#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Emily Portin <portin.emily@protonmail.com>");
MODULE_DESCRIPTION("02-hello-world");
MODULE_VERSION("0.1");

static int __init init_hello(void);
static void __exit exit_hello(void);

static int data_hello __initdata = 3;

int __init init_hello(void)
{

    printk(KERN_INFO "Hello, World (%d)!\n", data_hello);
    return 0;

}

void __exit exit_hello(void)
{

    printk(KERN_INFO "Goodbye, World!\n");

}

module_init(init_hello);
module_exit(exit_hello);
