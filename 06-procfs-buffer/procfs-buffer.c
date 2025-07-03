#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/errno.h>
#include <linux/version.h>
#include <linux/moduleparam.h>
#include <linux/minmax.h>
#include <linux/mutex.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Emily Portin <portin.emily@protonmail.com>");
MODULE_DESCRIPTION("06-procfs-buffer");
MODULE_VERSION("0.1");

#define PROCFS_BUFFER_MODULE_NAME "procfs-buffer"
#define PROCFS_BUFFER_FILE_NAME "procfs-buffer"
#define PROCFS_BUFFER_SIZE 1024
#define PROCFS_BUFFER_FILE_PERMS 0644

static int __init procfs_buffer_init(void);
static void __exit procfs_buffer_exit(void);

// struct proc_ops defined in linux/proc_fs.h

static int procfs_buffer_proc_open(struct inode*, struct file*);
static int procfs_buffer_proc_release(struct inode*, struct file*);
static ssize_t procfs_buffer_proc_read(struct file*, char __user*, size_t, loff_t*);
static ssize_t procfs_buffer_proc_write(struct file*, const char __user*, size_t, loff_t*);

static const struct proc_ops procfs_buffer_proc_ops = {
    .proc_open = procfs_buffer_proc_open,
    .proc_release = procfs_buffer_proc_release,
    .proc_read = procfs_buffer_proc_read,
    .proc_write = procfs_buffer_proc_write
};

// opaque pointer to a struct proc_dir_entry

static struct proc_dir_entry* procfs_buffer_proc_file = NULL;

// buffer to read and write (null-terminated string)

static size_t procfs_buffer_size = 0;
static char procfs_buffer[PROCFS_BUFFER_SIZE + 1] = {};

// mutex protecting procfs_buffer and procfs_buffer size

static DEFINE_MUTEX(procfs_buffer_mutex);

// enable debug messages

static bool debug = false;
module_param(debug, bool, 0);
MODULE_PARM_DESC(debug, "enable debug messages");

int __init procfs_buffer_init(void) {

    procfs_buffer_proc_file = proc_create(PROCFS_BUFFER_FILE_NAME, PROCFS_BUFFER_FILE_PERMS, NULL, &procfs_buffer_proc_ops);

    if (!procfs_buffer_proc_file) {
        pr_err("[%s:%s] failed to create /proc/%s with permissions %o\n", PROCFS_BUFFER_MODULE_NAME, __func__, PROCFS_BUFFER_FILE_NAME, PROCFS_BUFFER_FILE_PERMS);
        return -ENOMEM;
    }

    if (debug) {
        pr_info("[%s:%s] created /proc/%s with permissions %04o\n", PROCFS_BUFFER_MODULE_NAME, __func__, PROCFS_BUFFER_FILE_NAME, PROCFS_BUFFER_FILE_PERMS);
    }

    return 0;

}

void __exit procfs_buffer_exit(void) {

    proc_remove(procfs_buffer_proc_file);

    if (debug) {
        pr_info("[%s:%s] removed /proc/%s\n", PROCFS_BUFFER_MODULE_NAME, __func__, PROCFS_BUFFER_FILE_NAME);
    }

    return;

}

int procfs_buffer_proc_open(struct inode* inode, struct file* file) {

    if (debug) {
        pr_info("[%s:%s] opening /proc/%s (procfs_buffer = \"%s\", buffer.size = %zu)\n", PROCFS_BUFFER_MODULE_NAME, __func__, file->f_path.dentry->d_name.name, procfs_buffer, procfs_buffer_size);
    }

    return 0;

}

int procfs_buffer_proc_release(struct inode* inode, struct file* file) {

    if (debug) {
        pr_info("[%s:%s] closing /proc/%s (buffer = \"%s\", buffer.size = %zu)\n", PROCFS_BUFFER_MODULE_NAME, __func__, file->f_path.dentry->d_name.name, procfs_buffer, procfs_buffer_size);
    }

    return 0;

}

ssize_t procfs_buffer_proc_read(struct file* file, char __user* buffer, size_t length, loff_t* offset) {

    ssize_t retval = 0;

    // return zero if the mutex was acquired or sleep until the mutex is available

    if (mutex_lock_interruptible(&procfs_buffer_mutex)) {
        return -ERESTARTSYS;
    }

    if (*offset < 0) {
        retval = -EINVAL;
        goto PROCFS_BUFFER_PROC_READ_EXIT;
    }

    if ((size_t) *offset >= procfs_buffer_size) {
        retval = 0;
        goto PROCFS_BUFFER_PROC_READ_EXIT;
    }

    size_t bytes_to_read = min(length, procfs_buffer_size - *offset);

    if (debug) {
        pr_info("[%s:%s] reading /proc/%s (message.size = %zu, message.offset = %lld, requested.length = %zu, bytes.read = %zu)\n", PROCFS_BUFFER_MODULE_NAME, __func__, file->f_path.dentry->d_name.name, procfs_buffer_size, *offset, length, bytes_to_read);
    }

    if (copy_to_user(buffer, &procfs_buffer[*offset], bytes_to_read)) {
        retval = -EFAULT;
        goto PROCFS_BUFFER_PROC_READ_EXIT;
    }

    *offset += bytes_to_read;
    retval = bytes_to_read;

PROCFS_BUFFER_PROC_READ_EXIT:

    mutex_unlock(&procfs_buffer_mutex);
    return retval;

}

ssize_t procfs_buffer_proc_write(struct file* file, const char __user* buffer, size_t length, loff_t* offset) {

    ssize_t retval = 0;

    // return zero if the mutex was acquired or sleep until the mutex is available

    if (mutex_lock_interruptible(&procfs_buffer_mutex)) {
        return -ERESTARTSYS;
    }

    if (*offset < 0) {
        retval = -EINVAL;
        goto PROCFS_BUFFER_PROC_WRITE_EXIT;
    }

    if (*offset >= PROCFS_BUFFER_SIZE) {
        retval = -ENOSPC;
        goto PROCFS_BUFFER_PROC_WRITE_EXIT;
    }

    size_t bytes_available = PROCFS_BUFFER_SIZE - *offset;
    size_t bytes_to_write = min(length, bytes_available);

    if (debug) {
        pr_info("[%s:%s] writing /proc/%s (message.size = %zu, message.offset = %lld, requested.length = %zu, bytes.available = %zu, bytes.write = %zu)\n", PROCFS_BUFFER_MODULE_NAME, __func__, file->f_path.dentry->d_name.name, procfs_buffer_size, *offset, length, bytes_available, bytes_to_write);
    }

    if (!bytes_to_write) {
        retval = -ENOSPC;
        goto PROCFS_BUFFER_PROC_WRITE_EXIT;
    }

    if (copy_from_user(procfs_buffer + *offset, buffer, bytes_to_write)) {
        retval = -EFAULT;
        goto PROCFS_BUFFER_PROC_WRITE_EXIT;
    }

    // truncate on write instead of growing buffer size

    *offset += bytes_to_write;
    procfs_buffer_size = *offset;
    procfs_buffer[procfs_buffer_size] = '\0';
    retval = bytes_to_write;

PROCFS_BUFFER_PROC_WRITE_EXIT:

    mutex_unlock(&procfs_buffer_mutex);
    return retval;

}

module_init(procfs_buffer_init);
module_exit(procfs_buffer_exit);
