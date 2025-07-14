#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/pci.h>
#include <linux/mutex.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Emily Portin <portin.emily@protonmail.com>");
MODULE_DESCRIPTION("10-procfs-pcilist");
MODULE_VERSION("0.1");

#define PROCFS_PCILIST_MODULE_NAME "procfs-pcilist"
#define PROCFS_PCILIST_FILE_NAME "procfs-pcilist"
#define PROCFS_PCILIST_FILE_MODE 0444
#define PROCFS_PCILIST_FILE_PARENT NULL

static int __init procfs_pcilist_init(void);
static void __exit procfs_pcilist_exit(void);

// proc file operations

static int procfs_pcilist_proc_open(struct inode*, struct file*);
static ssize_t procfs_pcilist_proc_read(struct file*, char __user*, size_t, loff_t*);
static loff_t procfs_pcilist_proc_lseek(struct file*, loff_t, int);
static int procfs_pcilist_proc_release(struct inode*, struct file*);

static const struct proc_ops procfs_pcilist_proc_ops = {
    .proc_open = procfs_pcilist_proc_open,
    .proc_read = procfs_pcilist_proc_read,
    .proc_lseek = procfs_pcilist_proc_lseek,
    .proc_release = procfs_pcilist_proc_release
};

// sequence file operations

static void* procfs_pcilist_seq_start(struct seq_file*, loff_t*);
static void* procfs_pcilist_seq_next(struct seq_file*, void*, loff_t*);
static void procfs_pcilist_seq_stop(struct seq_file*, void*);
static int procfs_pcilist_seq_show(struct seq_file*, void*);

static const struct seq_operations procfs_pcilist_seq_ops = {
    .start = procfs_pcilist_seq_start,
    .next  = procfs_pcilist_seq_next,
    .stop  = procfs_pcilist_seq_stop,
    .show  = procfs_pcilist_seq_show
};

int __init procfs_pcilist_init(void) {

    struct proc_dir_entry* entry = proc_create(PROCFS_PCILIST_FILE_NAME, PROCFS_PCILIST_FILE_MODE, PROCFS_PCILIST_FILE_PARENT, &procfs_pcilist_proc_ops);

    if (!entry) {
        pr_err("[%s:%s] failed to create proc entry\n", PROCFS_PCILIST_MODULE_NAME, __func__);
        return -ENOMEM;
    }

    return 0;

}

void __exit procfs_pcilist_exit(void) {

    remove_proc_entry(PROCFS_PCILIST_FILE_NAME, NULL);

}

int procfs_pcilist_proc_open(struct inode* inode, struct file* file) {

    return seq_open(file, &procfs_pcilist_seq_ops);

}

ssize_t procfs_pcilist_proc_read(struct file* file, char __user* buffer, size_t length, loff_t* offset) {

    return seq_read(file, buffer, length, offset);

}

loff_t procfs_pcilist_proc_lseek(struct file* file, loff_t offset, int whence) {

    return seq_lseek(file, offset, whence);

}

int procfs_pcilist_proc_release(struct inode* inode, struct file* file) {

    return seq_release(inode, file);

}

void* procfs_pcilist_seq_start(struct seq_file* file, loff_t* position) {

    loff_t index = *position;
    struct pci_dev* pdev = NULL;

    // if start() returns a non-null pointer to a pci device then the reference
    // count for that device has been incremented and must be decremented via a
    // call to pci_get_device() in next() or pci_dev_put() in stop()

    while ((pdev = pci_get_device(PCI_ANY_ID, PCI_ANY_ID, pdev)) != NULL) {
        if (!index--) {
            break;
        }
    }

    return pdev;

}

void* procfs_pcilist_seq_next(struct seq_file* file, void* iter, loff_t* position) {


    struct pci_dev* pdev = iter;

    // pci_get_device() decrements the reference count of the current pci device
    // and returns either null or a non-null pointer to the next pci device; the
    // reference count of the latter must be decremented by a corresponding call
    // to next() or stop()

    ++*position;
    return pci_get_device(PCI_ANY_ID, PCI_ANY_ID, pdev);

}

void procfs_pcilist_seq_stop(struct seq_file* file, void* iter) {

    struct pci_dev* pdev = iter;

    // ensure that the reference count of the current pci device is decremented
    // properly (for instance, if the iteration did not terminate naturally)

    if (pdev) {
        pci_dev_put(pdev);
    }

}

int procfs_pcilist_seq_show(struct seq_file* file, void* iter) {

    struct pci_dev* pdev = iter;
    struct pci_driver* pdrv = pci_dev_driver(pdev);

    seq_printf(
        file,
        "%02X:%02X.%X %04X:%04X [%s]\n",
        pdev->bus->number,
        PCI_SLOT(pdev->devfn),
        PCI_FUNC(pdev->devfn),
        pdev->vendor,
        pdev->device,
        pdrv ? pdrv->name : ""
    );

    return 0;

}

module_init(procfs_pcilist_init);
module_exit(procfs_pcilist_exit);
