#include <linux/atomic.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/version.h>
#include <linux/errno.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Emily Portin <portin.emily@protonmail.com>");
MODULE_DESCRIPTION("04-chardev");
MODULE_VERSION("0.1");

#define CHARDEV_BUFFER_LEN 128
#define CHARDEV_DEVICE_NAME "chardev"

static int __init init_chardev(void);
static void __exit exit_chardev(void);

static int chardev_device_open(struct inode*, struct file*);
static int chardev_device_release(struct inode*, struct file*);
static ssize_t chardev_device_read(struct file*, char*, size_t, loff_t*);

enum {
    CHARDEV_NOT_OPEN = 0,
    CHARDEV_OPEN,
};

static char chardev_message_buffer[CHARDEV_BUFFER_LEN + 1];
static atomic_t chardev_already_open = ATOMIC_INIT(CHARDEV_NOT_OPEN);

static dev_t chardev_number = 0;
static struct class* chardev_class = NULL;
static struct device* chardev_device = NULL;
static struct cdev chardev_cdev = {};

static struct file_operations chardev_file_operations = {
    .read = chardev_device_read,
    .open = chardev_device_open,
    .release = chardev_device_release
};

// enable debug output

static bool debug = false;
module_param(debug, bool, 0);
MODULE_PARM_DESC(debug, "Enable debug messages");

int __init init_chardev(void) {

    int rc = 0;

    if (debug) {
        pr_info("[%s] Initializing character device\n", CHARDEV_DEVICE_NAME);
    }

    // allocate a range of device numbers

    if ((rc = alloc_chrdev_region(&chardev_number, 0, 1, CHARDEV_DEVICE_NAME)) < 0) {
        pr_alert("[%s] Failed to allocate device numbers for character device with error code %d\n", CHARDEV_DEVICE_NAME, rc);
        return rc;
    }

    if (debug) {
        pr_info("[%s] Allocated device numbers for character device\n", CHARDEV_DEVICE_NAME);
    }

    // create a device class for export to userspace

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 4, 0)
    chardev_class = class_create(CHARDEV_DEVICE_NAME);
#else
    chardev_class = class_create(THIS_MODULE, CHARDEV_DEVICE_NAME);
#endif

    if (IS_ERR(chardev_class)) {
        unregister_chrdev_region(chardev_number, 1);
        pr_alert("[%s] Failed to create device class for character device\n", CHARDEV_DEVICE_NAME);
        return PTR_ERR(chardev_class);
    }

    if (debug) {
        pr_info("[%s] Created device class for character device\n", CHARDEV_DEVICE_NAME);
    }

    // initialize and register internal representation of character device

    cdev_init(&chardev_cdev, &chardev_file_operations);
    chardev_cdev.owner = THIS_MODULE;

    if ((rc = cdev_add(&chardev_cdev, chardev_number, 1)) < 0) {
        class_destroy(chardev_class);
        unregister_chrdev_region(chardev_number, 1);
        pr_alert("[%s] Failed to initialize or register cdev structure for character device\n", CHARDEV_DEVICE_NAME);
        return rc;
    }

    if (debug) {
        pr_info("[%s] Initialized and registered cdev struct for character device\n", CHARDEV_DEVICE_NAME);
    }

    // create device node and register it with sysfs

    chardev_device = device_create(chardev_class, NULL, chardev_number, NULL, CHARDEV_DEVICE_NAME);

    if (IS_ERR(chardev_device)) {
        cdev_del(&chardev_cdev);
        class_destroy(chardev_class);
        unregister_chrdev_region(chardev_number, 1);
        pr_alert("[%s] Failed to create or register character device\n", CHARDEV_DEVICE_NAME);
        return PTR_ERR(chardev_device);
    }

    if (debug) {
        pr_info("[%s] Created and registered character device\n", CHARDEV_DEVICE_NAME);
    }

    return rc;

}

void __exit exit_chardev(void) {

    if (debug) {
        pr_info("[%s] Destroying character device\n", CHARDEV_DEVICE_NAME);
    }

    // most cleanup functions do not require checking for null

    cdev_del(&chardev_cdev);
    device_destroy(chardev_class, chardev_number);
    class_destroy(chardev_class);
    unregister_chrdev_region(chardev_number, 1);

}

int chardev_device_open(struct inode* inode, struct file* filp) {

    if (debug) {
        pr_info("[%s] Opening character device file\n", CHARDEV_DEVICE_NAME);
    }

    // called when a process opens the device file

    static unsigned counter = 0;

    // if chardev_already_open is CHARDEV_NOT_OPEN then atomically set it to
    // CHARDEV_OPEN and return CHARDEV_NOT_OPEN (in which case the conditional
    // statement will not execute); otherwise, return CHARDEV_OPEN (in which
    // case the conditional statement will execute);

    // this means no queueing

    if (atomic_cmpxchg(&chardev_already_open, CHARDEV_NOT_OPEN, CHARDEV_OPEN)) {
        pr_alert("[%s] Failed to open character device file\n", CHARDEV_DEVICE_NAME);
        return -EBUSY;
    }

    // sprintf is safe because the atomic lock ensures only one opener

    snprintf(chardev_message_buffer, sizeof(chardev_message_buffer), "[%s] Character device file has been opened %d times\n", CHARDEV_DEVICE_NAME, ++counter);

    // this is safe because the atomic lock ensures only one opener
    // sprintf is safe because the message will not overrun the buffer by construction

    sprintf(chardev_message_buffer, "[%s] Character device file has been opened %d times\n", CHARDEV_DEVICE_NAME, ++counter);

    // attempt to increment reference count

    if (!try_module_get(THIS_MODULE)) {
        pr_alert("[%s] Failed to increment reference count for character device file\n", CHARDEV_DEVICE_NAME);
        return -ENODEV;
    }

    return 0;

}

int chardev_device_release(struct inode* inode, struct file* file) {

    if (debug) {
        pr_info("[%s] Closing character device file\n", CHARDEV_DEVICE_NAME);
    }

    // called when a process closes the device file

    atomic_set(&chardev_already_open, CHARDEV_NOT_OPEN);

    // decrement reference count

    module_put(THIS_MODULE);

    return 0;

}

ssize_t chardev_device_read(struct file* file, char* buffer, size_t length, loff_t* offset) {

    if (debug) {
        pr_info("[%s] Reading character device file\n", CHARDEV_DEVICE_NAME);
    }

    // called when a process reads from an open device file

    ssize_t message_length = strlen(chardev_message_buffer);

    // return EOF if nothing to read (null terminator not counted)

    if (message_length <= *offset) {
        return 0;
    }

    // number of bytes to read from message buffer

    size_t bytes_to_read = message_length - *offset;

    if (bytes_to_read > length) {
        bytes_to_read = length;
    }

    // return EOF if nothing to read

    if (!bytes_to_read) {
        return 0;
    }

    // copy specified number of bytes to userspace

    if (copy_to_user(buffer, &chardev_message_buffer[*offset], bytes_to_read)) {
        return -EFAULT;
    }

    *offset += bytes_to_read;

    return bytes_to_read;

}

module_init(init_chardev);
module_exit(exit_chardev);
