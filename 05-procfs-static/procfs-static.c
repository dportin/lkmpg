#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/errno.h>
#include <linux/version.h>
#include <linux/moduleparam.h>
#include <linux/minmax.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Emily Portin <portin.emily@protonmail.com>");
MODULE_DESCRIPTION("05-procfs-static");
MODULE_VERSION("0.1");

#define PROCFS_STATIC_MODULE_NAME "procfs-static"
#define PROCFS_STATIC_FILE_NAME "procfs-static"
#define PROCFS_STATIC_FILE_PERMS 0444

static int __init procfs_static_init(void);
static void __exit procfs_static_exit(void);

// static buffer with read-only access

static const char procfs_static_buffer[] = "Hello, World!";
static const size_t procfs_static_buffer_length = sizeof(procfs_static_buffer) - 1;

// struct proc_ops defined in linux/proc_fs.h

static ssize_t procfs_static_proc_read(struct file*, char __user*, size_t, loff_t*);
static int procfs_static_proc_open(struct inode*, struct file*);
static int procfs_static_proc_release(struct inode*, struct file*);

static const struct proc_ops procfs_static_proc_ops = {
    .proc_read = procfs_static_proc_read,
    .proc_open = procfs_static_proc_open,
    .proc_release = procfs_static_proc_release
};

// opaque pointer to a struct proc_dir_entry

static struct proc_dir_entry* procfs_static_proc_file = NULL;

// enable debug messages

static bool debug = false;
module_param(debug, bool, 0);
MODULE_PARM_DESC(debug, "Enable debug messages");

int __init procfs_static_init(void) {

    procfs_static_proc_file = proc_create(PROCFS_STATIC_FILE_NAME, PROCFS_STATIC_FILE_PERMS, NULL, &procfs_static_proc_ops);

    if (!procfs_static_proc_file) {
        pr_err("[%s:%s] failed to create /proc/%s with permissions %o\n", PROCFS_STATIC_MODULE_NAME, __func__, PROCFS_STATIC_FILE_NAME, PROCFS_STATIC_FILE_PERMS);
        return -ENOMEM;
    }

    if (debug) {
        pr_info("[%s:%s] created /proc/%s with permissions %04o\n", PROCFS_STATIC_MODULE_NAME, __func__, PROCFS_STATIC_FILE_NAME, PROCFS_STATIC_FILE_PERMS);
    }

    return 0;

}

void __exit procfs_static_exit(void) {

    proc_remove(procfs_static_proc_file);

    if (debug) {
        pr_info("[%s:%s] removed /proc/%s\n", PROCFS_STATIC_MODULE_NAME, __func__, PROCFS_STATIC_FILE_NAME);
    }

    return;

}

int procfs_static_proc_open(struct inode* inode, struct file* file) {

    if (debug) {
        pr_info("[%s:%s] opening /proc/%s (procfs_buffer = \"%s\", buffer.size = %zu)\n", PROCFS_STATIC_MODULE_NAME, __func__, file->f_path.dentry->d_name.name,
                procfs_static_buffer, procfs_static_buffer_length);
    }

    return 0;

}

int procfs_static_proc_release(struct inode* inode, struct file* file) {

    if (debug) {
        pr_info("[%s:%s] closing /proc/%s (buffer = \"%s\", buffer.size = %zu)\n", PROCFS_STATIC_MODULE_NAME, __func__, file->f_path.dentry->d_name.name, procfs_static_buffer, procfs_static_buffer_length);
    }

    return 0;

}

ssize_t procfs_static_proc_read(struct file* file, char __user* buffer, size_t length, loff_t* offset) {

    if (*offset < 0) {
        return -EINVAL;
    }

    if ((size_t) *offset >= procfs_static_buffer_length) {
        return 0;
    }

    size_t bytes_to_read = min(length, procfs_static_buffer_length - *offset);

    if (debug) {
        pr_info("[%s:%s] reading /proc/%s (message.size = %zu, message.offset = %lld, requested.length = %zu, bytes.read = %zu)\n", PROCFS_STATIC_MODULE_NAME, __func__, file->f_path.dentry->d_name.name, procfs_static_buffer_length, *offset, length, bytes_to_read);
    }

    if (copy_to_user(buffer, &procfs_static_buffer[*offset], bytes_to_read)) {
        return -EFAULT;
    }

    *offset += bytes_to_read;
    return bytes_to_read;

}

module_init(procfs_static_init);
module_exit(procfs_static_exit);
