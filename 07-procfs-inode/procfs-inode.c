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
#include <linux/string.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Emily Portin <portin.emily@protonmail.com>");
MODULE_DESCRIPTION("07-procfs-inode");
MODULE_VERSION("0.1");

#define PROCFS_INODE_MODULE_NAME "procfs-inode"
#define PROCFS_INODE_FILE_NAME "procfs-inode"

#define PROCFS_INODE_FILE_SIZE 128
#define PROCFS_INODE_FILE_PERMS 0644
#define PROCFS_INODE_FILE_PARENT NULL
#define PROCFS_INODE_BUFFER_SIZE 127

static int __init procfs_inode_init(void);
static void __exit procfs_inode_exit(void);

// struct proc_ops defined in linux/proc_fs.h

static int procfs_inode_proc_open(struct inode*, struct file*);
static int procfs_inode_proc_release(struct inode*, struct file*);
static ssize_t procfs_inode_proc_read(struct file*, char __user*, size_t, loff_t*);
static ssize_t procfs_inode_proc_write(struct file*, const char __user*, size_t, loff_t*);

static const struct proc_ops procfs_inode_proc_ops = {
    .proc_open = procfs_inode_proc_open,
    .proc_release = procfs_inode_proc_release,
    .proc_read = procfs_inode_proc_read,
    .proc_write = procfs_inode_proc_write
};

// private data for procfs entry

struct procfs_inode_proc_context {
	char buffer[PROCFS_INODE_BUFFER_SIZE + 1];
    size_t size;
    struct mutex mutex;
};

// opaque pointer to procfs entry

static struct proc_dir_entry* procfs_inode_proc_file = NULL;

// pointer to private data for procfs entry

static struct procfs_inode_proc_context* procfs_inode_proc_context = NULL;

// enable debug messages

static bool procfs_inode_param_debug = false;
module_param_named(debug, procfs_inode_param_debug, bool, 0);
MODULE_PARM_DESC(debug, "enable debug messages");

int __init procfs_inode_init(void) {

    // initialize private data context

    procfs_inode_proc_context = kzalloc(sizeof(*procfs_inode_proc_context), GFP_KERNEL);

    if (!procfs_inode_proc_context) {
        return -ENOMEM;
    }

    memset(procfs_inode_proc_context->buffer, 0, sizeof(procfs_inode_proc_context->buffer));
    procfs_inode_proc_context->size = 0;
    mutex_init(&procfs_inode_proc_context->mutex);

    // create procfs entry with private data context

    procfs_inode_proc_file = proc_create_data(PROCFS_INODE_FILE_NAME, PROCFS_INODE_FILE_PERMS, PROCFS_INODE_FILE_PARENT, &procfs_inode_proc_ops, procfs_inode_proc_context);

    if (!procfs_inode_proc_file) {
        kfree(procfs_inode_proc_context);
        pr_err("[%s:%s] failed to create /proc/%s entry with permissions %04o\n", PROCFS_INODE_MODULE_NAME, __func__, PROCFS_INODE_FILE_NAME, PROCFS_INODE_FILE_PERMS);
        return -ENOMEM;
    }

    if (procfs_inode_param_debug) {
        pr_info("[%s:%s] created /proc/%s entry with permissions %04o (pdata.size = %zu, pdata.buffer = %pK)\n", PROCFS_INODE_MODULE_NAME, __func__, PROCFS_INODE_FILE_NAME, PROCFS_INODE_FILE_PERMS, procfs_inode_proc_context->size, procfs_inode_proc_context->buffer);
    }

    return 0;

}

void __exit procfs_inode_exit(void) {

    kfree(procfs_inode_proc_context);
    proc_remove(procfs_inode_proc_file);

    if (procfs_inode_param_debug) {
        pr_info("[%s:%s] removed /proc/%s\n", PROCFS_INODE_MODULE_NAME, __func__, PROCFS_INODE_FILE_NAME);
    }

    return;

}

int procfs_inode_proc_open(struct inode* inode, struct file* file) {

    // synchronization only required on read and writes

    struct procfs_inode_proc_context* local_context = pde_data(inode);

    if (!local_context) {
        pr_err("[%s:%s] failed to get private data for /proc/%s\n", PROCFS_INODE_MODULE_NAME, __func__, file->f_path.dentry->d_name.name);
        return -EINVAL;
    }

    file->private_data = local_context;

    if (procfs_inode_param_debug) {
        pr_info("[%s:%s] opening /proc/%s (pdata.size = %zu, pdata.buffer = %pK)\n", PROCFS_INODE_MODULE_NAME, __func__, file->f_path.dentry->d_name.name, local_context->size, local_context->buffer);
    }

    return 0;

}

int procfs_inode_proc_release(struct inode* inode, struct file* file) {

    struct procfs_inode_proc_context* local_context = pde_data(inode);

    if (!local_context) {
        pr_err("[%s:%s] failed to get private data for /proc/%s\n", PROCFS_INODE_MODULE_NAME, __func__, file->f_path.dentry->d_name.name);
        return -EINVAL;
    }

    if (procfs_inode_param_debug) {
        pr_info("[%s:%s] closing /proc/%s (pdata.size = %zu, pdata.buffer = %pK)\n", PROCFS_INODE_MODULE_NAME, __func__, file->f_path.dentry->d_name.name, local_context->size, local_context->buffer);
    }

    return 0;

}

ssize_t procfs_inode_proc_read(struct file* file, char __user* buffer, size_t length, loff_t* offset) {

    if (!file->private_data) {
        pr_err("[%s:%s] failed to get private data for /proc/%s\n", PROCFS_INODE_MODULE_NAME, __func__, file->f_path.dentry->d_name.name);
        return -EINVAL;
    }

    ssize_t retval = 0;
    struct procfs_inode_proc_context* local_context = file->private_data;

    // restart system call if mutex could not be acquired

    if (mutex_lock_interruptible(&local_context->mutex)) {
        return -ERESTARTSYS;
    }

    // read from the private buffer

    if (*offset < 0) {
        retval = -EINVAL;
        goto PROCFS_INODE_PROC_READ_EXIT;
    }

    if ((size_t) *offset >= local_context->size) {
        retval = 0;
        goto PROCFS_INODE_PROC_READ_EXIT;
    }

    size_t bytes_to_read = min(length, local_context->size - *offset);

    if (procfs_inode_param_debug) {
        pr_info("[%s:%s] reading private data for /proc/%s (pdata.size = %zu, pdata.buffer = %pK, requested.length = %zu, requested.offset = %lld, bytes.read = %zu)\n", PROCFS_INODE_MODULE_NAME, __func__, file->f_path.dentry->d_name.name, local_context->size, local_context->buffer, length, *offset, bytes_to_read);
    }

    if (copy_to_user(buffer, &local_context->buffer[*offset], bytes_to_read)) {
        retval = -EFAULT;
        goto PROCFS_INODE_PROC_READ_EXIT;
    }

    *offset += bytes_to_read;
    retval = bytes_to_read;

PROCFS_INODE_PROC_READ_EXIT:

    mutex_unlock(&local_context->mutex);
    return retval;

}

ssize_t procfs_inode_proc_write(struct file* file, const char __user* buffer, size_t length, loff_t* offset) {

    if (!file->private_data) {
        pr_err("[%s:%s] failed to get private data for /proc/%s\n", PROCFS_INODE_MODULE_NAME, __func__, file->f_path.dentry->d_name.name);
        return -EINVAL;
    }

    ssize_t retval = 0;
    struct procfs_inode_proc_context* local_context = file->private_data;

    // restart system call if mutex could not be acquired

    if (mutex_lock_interruptible(&local_context->mutex)) {
        return -ERESTARTSYS;
    }

    // write to the private buffer

    if (*offset < 0) {
        retval = -EINVAL;
        goto PROCFS_INODE_PROC_WRITE_EXIT;
    }

    if (*offset >= PROCFS_INODE_BUFFER_SIZE) {
        retval = -ENOSPC;
        goto PROCFS_INODE_PROC_WRITE_EXIT;
    }

    size_t bytes_available = PROCFS_INODE_BUFFER_SIZE - *offset;
    size_t bytes_to_write = min(length, bytes_available);

    if (procfs_inode_param_debug) {

        pr_info("[%s:%s] writing private data for /proc/%s (pdata.size = %zu, pdata.buffer = %pK, requested.length = %zu, requested.offset = %lld, bytes.avilable = %zu, bytes.write = %zu)\n", PROCFS_INODE_MODULE_NAME, __func__, file->f_path.dentry->d_name.name, local_context->size, local_context->buffer, length, *offset, bytes_available, bytes_to_write);
    }

    if (!bytes_to_write) {
        retval = -ENOSPC;
        goto PROCFS_INODE_PROC_WRITE_EXIT;
    }

    if (copy_from_user(local_context->buffer + *offset, buffer, bytes_to_write)) {
        retval = -EFAULT;
        goto PROCFS_INODE_PROC_WRITE_EXIT;
    }

    // truncate on write instead of growing buffer size

    *offset += bytes_to_write;
    local_context->size = *offset;
    local_context->buffer[local_context->size] = '\0';
    retval = bytes_to_write;

PROCFS_INODE_PROC_WRITE_EXIT:

    mutex_unlock(&local_context->mutex);
    return retval;

}

module_init(procfs_inode_init);
module_exit(procfs_inode_exit);
