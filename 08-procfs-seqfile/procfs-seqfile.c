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
#include <linux/seq_file.h>

// read four entries from the sequence file starting at offset ten
// - sed -n '10,14p' /proc/procfs-seqfile
// - tail -n +11 /proc/procfs-seqfile | head -n4
// - dd if=/proc/procfs-seqfile bs=4 skip=10 count=4

// overwrite first eleven entries of sequence file with 10..20
// - seq 10 20 | dd of=/proc/procfs-seqfile

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Emily Portin <portin.emily@protonmail.com>");
MODULE_DESCRIPTION("08-procfs-seqfile");
MODULE_VERSION("0.1");

#define PROCFS_SEQFILE_MODULE_NAME "procfs-seqfile"
#define PROCFS_SEQFILE_FILE_NAME "procfs-seqfile"

#define PROCFS_SEQFILE_DATA_SIZE 256
#define PROCFS_SEQFILE_FILE_PERMS 0666
#define PROCFS_SEQFILE_FILE_PARENT NULL

struct procfs_seqfile_data {
    u8 buffer[PROCFS_SEQFILE_DATA_SIZE];
    struct mutex mutex;
};

static int __init procfs_seqfile_init(void);
static void __exit procfs_seqfile_exit(void);

static void* procfs_seqfile_seq_start(struct seq_file*, loff_t*);
static void procfs_seqfile_seq_stop(struct seq_file*, void*);
static void* procfs_seqfile_seq_next(struct seq_file*, void*, loff_t*);
static int procfs_seqfile_seq_show(struct seq_file*, void*);

static const struct seq_operations procfs_seqfile_seq_ops = {
    .start = procfs_seqfile_seq_start,
    .stop = procfs_seqfile_seq_stop,
    .next = procfs_seqfile_seq_next,
    .show = procfs_seqfile_seq_show
};

int procfs_seqfile_proc_open(struct inode*, struct file*);
ssize_t procfs_seqfile_proc_read(struct file*, char __user*, size_t, loff_t*);
ssize_t procfs_seqfile_proc_write(struct file*, const char __user*, size_t, loff_t*);
loff_t procfs_seqfile_proc_lseek(struct file*, loff_t, int);
int procfs_seqfile_proc_release(struct inode*, struct file*);

static const struct proc_ops procfs_seqfile_proc_ops = {
    .proc_open = procfs_seqfile_proc_open,
    .proc_read = procfs_seqfile_proc_read,
    .proc_write = procfs_seqfile_proc_write,
    .proc_lseek = seq_lseek,
    .proc_release = procfs_seqfile_proc_release
};

static struct procfs_seqfile_data* procfs_seqfile_data = NULL;
static struct proc_dir_entry* procfs_seqfile_file = NULL;

static bool procfs_seqfile_param_debug = false;
module_param_named(debug, procfs_seqfile_param_debug, bool, 0);
MODULE_PARM_DESC(debug, "enable debug messages");

int __init procfs_seqfile_init(void) {

    // initialize private data for sequence file

    procfs_seqfile_data = kzalloc(sizeof(*procfs_seqfile_data), GFP_KERNEL);

    if (!procfs_seqfile_data) {
        pr_err("[%s:%s] failed to allocate private data for seqfile\n", PROCFS_SEQFILE_MODULE_NAME, __func__);
        return -ENOMEM;
    }

    mutex_init(&procfs_seqfile_data->mutex);

    for (int i = 0; i < PROCFS_SEQFILE_DATA_SIZE; ++i) {
        procfs_seqfile_data->buffer[i] = i;
    }

    // initialize sequence file in proc file system

    procfs_seqfile_file = proc_create_data(PROCFS_SEQFILE_FILE_NAME, PROCFS_SEQFILE_FILE_PERMS, PROCFS_SEQFILE_FILE_PARENT, &procfs_seqfile_proc_ops, procfs_seqfile_data);

    if (!procfs_seqfile_file) {
        kfree(procfs_seqfile_data);
        pr_err("[%s:%s] failed to allocate seqfile\n", PROCFS_SEQFILE_MODULE_NAME, __func__);
        return -ENOMEM;
    }

    if (procfs_seqfile_param_debug) {
        pr_info("[%s:%s] created seqfile \"%s\" in procfs with permissions %04o\n", PROCFS_SEQFILE_MODULE_NAME, __func__, PROCFS_SEQFILE_FILE_NAME, PROCFS_SEQFILE_FILE_PERMS);
    }

    return 0;

}

void __exit procfs_seqfile_exit(void) {

    // free private data and remove sequence file in proc file system

    kfree(procfs_seqfile_data);
    proc_remove(procfs_seqfile_file);

    if (procfs_seqfile_param_debug) {
        pr_info("[%s:%s] removed seqfile \"%s\" in procfs\n", PROCFS_SEQFILE_MODULE_NAME, __func__, PROCFS_SEQFILE_FILE_NAME);
    }

    return;

}

int procfs_seqfile_proc_open(struct inode* inode, struct file* file) {

    // open sequence file in proc file system with given sequence operations

    if (procfs_seqfile_param_debug) {
        pr_info("[%s:%s] opening seqfile \"%s\" in procfs\n", PROCFS_SEQFILE_MODULE_NAME, __func__, file->f_path.dentry->d_name.name);
    }

    return seq_open(file, &procfs_seqfile_seq_ops);

}

ssize_t procfs_seqfile_proc_read(struct file* file, char __user* buffer, size_t length, loff_t* offset) {

    // could set .proc_read to seq_read in proc_ops struct instead

    if (!offset) {
        pr_err("[%s:%s] invalid offset: %pK\n", PROCFS_SEQFILE_MODULE_NAME, __func__, offset);
        return -EINVAL;
    }

    if (procfs_seqfile_param_debug) {
        pr_info("[%s:%s] reading seqfile \"%s\" in procfs (buffer.length = %zu, offset = %lld)\n", PROCFS_SEQFILE_MODULE_NAME, __func__, PROCFS_SEQFILE_FILE_NAME, length, *offset);
    }

    return seq_read(file, buffer, length, offset);

}

ssize_t procfs_seqfile_proc_write(struct file* file, const char __user* buffer, size_t length, loff_t* offset) {

    ssize_t retval = 0;
    struct procfs_seqfile_data* context = pde_data(file_inode(file));

    if (!context) {
        pr_err("[%s:%s] failed to get private data for seqfile \"%s\" in procfs\n", PROCFS_SEQFILE_MODULE_NAME, __func__, file->f_path.dentry->d_name.name);
        retval = -EINVAL;
        goto PROCFS_SEQFILE_PROC_WRITE_EXIT;
    }

    char* kbuffer = kzalloc(length + 1, GFP_KERNEL);

    if (!kbuffer) {
        pr_err("[%s:%s] failed to allocate kernel buffer (buffer.length = %zu)\n", PROCFS_SEQFILE_MODULE_NAME, __func__, length);
        retval = -ENOMEM;
        goto PROCFS_SEQFILE_PROC_WRITE_EXIT;
    }

    if (copy_from_user(kbuffer, buffer, length)) {
        pr_err("[%s:%s] failed to copy data to kernel buffer (buffer.length = %zu)\n", PROCFS_SEQFILE_MODULE_NAME, __func__, length);
        retval = -EFAULT;
        goto PROCFS_SEQFILE_PROC_WRITE_EXIT_FREE;
    }

    kbuffer[length] = '\0';
    mutex_lock(&context->mutex);

    // always write from buffer[0]

    int parsed = 0;
    char* token = NULL;
    char* iterator = kbuffer;

    for (size_t i = 0; i < PROCFS_SEQFILE_DATA_SIZE; ++i) {

        if (!(token = strsep(&iterator, " ,\n"))) {
            break;
        }

        if (token[0] == '\0') {
            continue;
        }

        if ((retval = kstrtoint(token, 0, &parsed)) != 0) {
            pr_err("[%s:%s] failed to parse token \"%s\" with error code %zd\n", PROCFS_SEQFILE_MODULE_NAME, __func__, token, retval);
            goto PROCFS_SEQFILE_PROC_WRITE_EXIT_UNLOCK;
        }

        parsed = clamp(parsed, 0, U8_MAX);
        context->buffer[i] = parsed;

    }

    retval = length;

PROCFS_SEQFILE_PROC_WRITE_EXIT_UNLOCK:

    mutex_unlock(&context->mutex);

PROCFS_SEQFILE_PROC_WRITE_EXIT_FREE:

    kfree(kbuffer);

PROCFS_SEQFILE_PROC_WRITE_EXIT:

    return retval;

}

loff_t procfs_seqfile_proc_lseek(struct file* file, loff_t offset, int whence) {

    // could set .proc_lseek to seq_lseek in proc_ops struct instead

    if (procfs_seqfile_param_debug) {
        pr_info("[%s:%s] seeking in seqfile \"%s\" in procfs (offset = %lld, whence = %d)\n", PROCFS_SEQFILE_MODULE_NAME, __func__, file->f_path.dentry->d_name.name, offset, whence);
    }

    return seq_lseek(file, offset, whence);

}

int procfs_seqfile_proc_release(struct inode* inode, struct file* file) {

    // could set .proc_release to seq_release in proc_ops struct instead

    if (procfs_seqfile_param_debug) {
        pr_info("[%s:%s] releasing seqfile \"%s\" in procfs\n", PROCFS_SEQFILE_MODULE_NAME, __func__, file->f_path.dentry->d_name.name);
    }

    return seq_release(inode, file);

}

static void* procfs_seqfile_seq_start(struct seq_file* seq, loff_t *pos) {

    // called at the beginning of a read operation

    if (!pos) {
        pr_err("[%s:%s] invalid offset: %pK\n", PROCFS_SEQFILE_MODULE_NAME, __func__, pos);
        return NULL;
    }

    if (*pos < 0) {
        pr_err("[%s:%s] invalid offset: %lld\n", PROCFS_SEQFILE_MODULE_NAME, __func__, *pos);
        return NULL;
    }

    if (procfs_seqfile_param_debug) {
        pr_info("[%s:%s] starting sequence (offset = %lld)\n", PROCFS_SEQFILE_MODULE_NAME, __func__, *pos);
    }

    // return null when the iterator is exhausted

    if (*pos >= PROCFS_SEQFILE_DATA_SIZE) {
        return NULL;
    }

    struct procfs_seqfile_data* context = pde_data(file_inode(seq->file));

    if (!context) {
        pr_err("[%s:%s] failed to get private data for seqfile \"%s\" in procfs\n", PROCFS_SEQFILE_MODULE_NAME, __func__, seq->file->f_path.dentry->d_name.name);
        return NULL;
    }

    mutex_lock(&context->mutex);
    seq->private = context;

    return pos;

}

static void procfs_seqfile_seq_stop(struct seq_file* seq, void *iter) {

    // called at the end of a read operation

    if (procfs_seqfile_param_debug) {
        pr_info("[%s:%s] stopping sequence\n", PROCFS_SEQFILE_MODULE_NAME, __func__);
    }

    struct procfs_seqfile_data* context = seq->private;

    if (!context) {
        pr_err("[%s:%s] failed to get private data for sequence file \"%s\" in proc file system\n", PROCFS_SEQFILE_MODULE_NAME, __func__, seq->file->f_path.dentry->d_name.name);
        return;
    }

    mutex_unlock(&context->mutex);

    return;

}

static void* procfs_seqfile_seq_next(struct seq_file* seq, void *iter, loff_t* pos) {

    // advance the iterator based on current position

    if (!pos) {
        pr_err("[%s:%s] invalid offset: %pK\n", PROCFS_SEQFILE_MODULE_NAME, __func__, pos);
        return NULL;
    }

    if (*pos < 0 || *pos >= PROCFS_SEQFILE_DATA_SIZE) {
        pr_err("[%s:%s] invalid offset: %lld\n", PROCFS_SEQFILE_MODULE_NAME, __func__, *pos);
        return NULL;
    }

    if (procfs_seqfile_param_debug) {
        pr_info("[%s:%s] advancing iterator (position = %lld)\n", PROCFS_SEQFILE_MODULE_NAME, __func__, *pos);
    }

    struct procfs_seqfile_data* context = seq->private;

    if (!context) {
        pr_err("[%s:%s] failed to get private data for seqfile \"%s\" in procfs\n", PROCFS_SEQFILE_MODULE_NAME, __func__, seq->file->f_path.dentry->d_name.name);
        return NULL;
    }

    if (++*pos >= PROCFS_SEQFILE_DATA_SIZE) {
        return NULL;
    }

    return pos;

}

static int procfs_seqfile_seq_show(struct seq_file* seq, void *iter) {

    loff_t* pos = iter;

    if (!pos) {
        pr_err("[%s:%s] invalid iterator: %pK\n", PROCFS_SEQFILE_MODULE_NAME, __func__, pos);
        return -EINVAL;
    }

    if (*pos < 0 || *pos >= PROCFS_SEQFILE_DATA_SIZE) {
        pr_err("[%s:%s] invalid iterator: %lld\n", PROCFS_SEQFILE_MODULE_NAME, __func__, *pos);
        return -EINVAL;
    }

	struct procfs_seqfile_data* context = seq->private;

    if (!context) {
        pr_err("[%s:%s] failed to get private data for seqfile \"%s\" in procfs\n", PROCFS_SEQFILE_MODULE_NAME, __func__, seq->file->f_path.dentry->d_name.name);
        return -EINVAL;
    }

    // always show four bytes for easy testing with dd

    seq_printf(seq, "%03u\n", context->buffer[*pos]);

    return 0;

}

module_init(procfs_seqfile_init);
module_exit(procfs_seqfile_exit);
