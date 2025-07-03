#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Emily Portin <portin.emily@protonmail.com>");
MODULE_DESCRIPTION("01-hello-world");
MODULE_VERSION("0.1");

static int init_hello(void);
static void exit_hello(void);

int init_hello(void)
{

    pr_info("Hello, World!\n");

    return 0;
}

void exit_hello(void)
{

    printk(KERN_INFO "Goodbye, World!\n");

    return;

}

module_init(init_hello);
module_exit(exit_hello);
