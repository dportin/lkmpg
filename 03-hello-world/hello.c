#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/stat.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Emily Portin <portin.emily@protonmail.com>");
MODULE_DESCRIPTION("03-hello-world");
MODULE_VERSION("0.1");

static int __init init_hello(void);
static void __exit exit_hello(void);

// a int parameter

static int param_int = 1;
module_param(param_int, int, 0);
MODULE_PARM_DESC(param_int, "int parameter");

// a named int parameter

static int param_int_named = 1;
module_param_named(param_int_named, param_int_named, int, 0);
MODULE_PARM_DESC(param_int_named, "named int parameter");

// a string parameter

static char* param_str = "value";
module_param(param_str, charp, 0);
MODULE_PARM_DESC(param_str, "string parameter");

// a named string parameter

static char param_str_named[32] = "value";
module_param_string(param_str_named, param_str_named, 32, 0);
MODULE_PARM_DESC(param_str_named, "named string parameter");

// an array parameter

static int param_arr[2] = {0, 0};
static int param_arr_count = 0;
module_param_array(param_arr, int, &param_arr_count, 0);
MODULE_PARM_DESC(param_arr, "array parameter");

// a named array parameter

static int param_arr_named[2] = {0, 0};
static int param_arr_named_count = 0;
module_param_array_named(param_arr_named, param_arr_named, int, &param_arr_named_count, 0);
MODULE_PARM_DESC(param_arr_named, "named array parameter");

int __init init_hello(void) {

    printk(KERN_INFO "Hello, World!\n");
    printk(KERN_INFO "param_int = %d\n", param_int);
    printk(KERN_INFO "param_int_named = %d\n", param_int_named);
    printk(KERN_INFO "param_str = \"%s\"\n", param_str);
    printk(KERN_INFO "param_str_named = \"%s\"\n", param_str_named);

    int param_arr_size = sizeof(param_arr) / sizeof(int);
    int param_arr_named_size = sizeof(param_arr_named) / sizeof(int);

    for (int i = 0; i < param_arr_size; ++i) {
        printk(KERN_INFO "param_arr[%d] = %d\n", i, param_arr[i]);
    }
    printk(KERN_INFO "param_arr received %d arguments\n", param_arr_count);

    for (int i = 0; i < param_arr_named_size; ++i) {
        printk(KERN_INFO "param_arr_named[%d] = %d\n", i, param_arr_named[i]);
    }
    printk(KERN_INFO "param_arr_named received %d arguments\n", param_arr_named_count);

    return 0;

}

void __exit exit_hello(void) {

    printk(KERN_INFO "Goodbye, World!\n");

}

module_init(init_hello);
module_exit(exit_hello);

