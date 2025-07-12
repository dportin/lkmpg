#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/moduleparam.h>
#include <linux/mutex.h>
#include <linux/kobject.h>
#include <linux/string.h>
#include <linux/sysfs.h>
#include <linux/minmax.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Emily Portin <portin.emily@protonmail.com>");
MODULE_DESCRIPTION("09-sysfs-attrs");
MODULE_VERSION("0.1");

#define SYSFS_ATTRS_MODULE_NAME "sysfs-attrs"

static int __init sysfs_attrs_init(void);
static void __exit sysfs_attrs_exit(void);

static struct kobject* sysfs_attrs_kobj = NULL;

// boolean, signed integer and string attributes

#define SYSFS_ATTRS_ATTR_BOOL_NAME attr-bool
#define SYSFS_ATTRS_ATTR_INT_NAME attr-int
#define SYSFS_ATTRS_ATTR_STRING_NAME attr-string

#define SYSFS_ATTRS_ATTR_INT_MODE 0664
#define SYSFS_ATTRS_ATTR_STRING_MODE 0664
#define SYSFS_ATTRS_ATTR_BOOL_MODE 0664

#define SYSFS_ATTRS_ATTR_BOOL_INIT false
#define SYSFS_ATTRS_ATTR_INT_INIT 0
#define SYSFS_ATTRS_ATTR_STRING_INIT ""
#define SYSFS_ATTRS_ATTR_STRING_SIZE 1024

static ssize_t sysfs_attrs_attr_bool_show(struct kobject* kobj, struct kobj_attribute* attr, char* buf);
static ssize_t sysfs_attrs_attr_bool_store(struct kobject* kobj, struct kobj_attribute* attr, const char* buf, size_t count);

static ssize_t sysfs_attrs_attr_int_show(struct kobject* kobj, struct kobj_attribute* attr, char* buf);
static ssize_t sysfs_attrs_attr_int_store(struct kobject* kobj, struct kobj_attribute* attr, const char* buf, size_t count);

static ssize_t sysfs_attrs_attr_string_show(struct kobject* kobj, struct kobj_attribute* attr, char* buf);
static ssize_t sysfs_attrs_attr_string_store(struct kobject* kobj, struct kobj_attribute* attr, const char* buf, size_t count);

struct sysfs_attrs_attr_bool {
    bool value;
    struct mutex mutex;
    struct kobj_attribute kattr;
};

struct sysfs_attrs_attr_int {
    int value;
    struct mutex mutex;
    struct kobj_attribute kattr;
};

struct sysfs_attrs_attr_string {
    char value[SYSFS_ATTRS_ATTR_STRING_SIZE];
    struct mutex mutex;
    struct kobj_attribute kattr;
};

static struct sysfs_attrs_attr_bool sysfs_attrs_attr_bool = {
    .value = SYSFS_ATTRS_ATTR_BOOL_INIT,
    .mutex = __MUTEX_INITIALIZER(sysfs_attrs_attr_bool.mutex),
    .kattr = __ATTR(
        SYSFS_ATTRS_ATTR_BOOL_NAME,
        SYSFS_ATTRS_ATTR_BOOL_MODE,
        sysfs_attrs_attr_bool_show,
        sysfs_attrs_attr_bool_store
    )
};

static struct sysfs_attrs_attr_int sysfs_attrs_attr_int = {
    .value = SYSFS_ATTRS_ATTR_INT_INIT,
    .mutex = __MUTEX_INITIALIZER(sysfs_attrs_attr_int.mutex),
    .kattr = __ATTR(
        SYSFS_ATTRS_ATTR_INT_NAME,
        SYSFS_ATTRS_ATTR_INT_MODE,
        sysfs_attrs_attr_int_show,
        sysfs_attrs_attr_int_store
    )
};

static struct sysfs_attrs_attr_string sysfs_attrs_attr_string = {
    .value = SYSFS_ATTRS_ATTR_STRING_INIT,
    .mutex = __MUTEX_INITIALIZER(sysfs_attrs_attr_string.mutex),
    .kattr = __ATTR(
        SYSFS_ATTRS_ATTR_STRING_NAME,
        SYSFS_ATTRS_ATTR_STRING_MODE,
        sysfs_attrs_attr_string_show,
        sysfs_attrs_attr_string_store
    )
};

// attribute group for boolean, signed integer and string attributes

static struct attribute* sysfs_attrs_array[] = {
    &sysfs_attrs_attr_bool.kattr.attr,
    &sysfs_attrs_attr_int.kattr.attr,
    &sysfs_attrs_attr_string.kattr.attr,
    NULL,
};

static const struct attribute_group sysfs_attrs_group = {
    .attrs = sysfs_attrs_array,
};

int __init sysfs_attrs_init(void) {

    int retval = 0;

    sysfs_attrs_kobj = kobject_create_and_add(SYSFS_ATTRS_MODULE_NAME, kernel_kobj);

    if (!sysfs_attrs_kobj) {
        pr_err("[%s:%s] failed to create or add kobject: %pK\n", SYSFS_ATTRS_MODULE_NAME, __func__, sysfs_attrs_kobj);
        return -ENOMEM;
    }

    retval = sysfs_create_group(sysfs_attrs_kobj, &sysfs_attrs_group);

    if (retval) {
        pr_err("[%s:%s] failed to create attribute group: %d\n", SYSFS_ATTRS_MODULE_NAME, __func__, retval);
        kobject_put(sysfs_attrs_kobj);
        return retval;
    }

    return retval;

}

void __exit sysfs_attrs_exit(void) {

    sysfs_remove_group(sysfs_attrs_kobj, &sysfs_attrs_group);
    kobject_put(sysfs_attrs_kobj);

    return;

}

ssize_t sysfs_attrs_attr_bool_show(struct kobject* kobj, struct kobj_attribute* kattr, char* buffer) {

    ssize_t bytes = 0;
    struct sysfs_attrs_attr_bool* self = container_of(kattr, struct sysfs_attrs_attr_bool, kattr);

    mutex_lock(&self->mutex);
    bytes = sysfs_emit(buffer, "%d\n", self->value);
    mutex_unlock(&self->mutex);

    return bytes;

}

ssize_t sysfs_attrs_attr_bool_store(struct kobject* kobj, struct kobj_attribute* kattr, const char* buffer, size_t bytes) {

    ssize_t error = 0;
    bool value = false;
    struct sysfs_attrs_attr_bool* self = container_of(kattr, struct sysfs_attrs_attr_bool, kattr);

    if ((error = kstrtobool(buffer, &value))) {
        pr_err("[%s:%s] failed to parse input as bool (bytes = %zu)\n", SYSFS_ATTRS_MODULE_NAME, __func__, bytes);
        return error;
    }

    mutex_lock(&self->mutex);
    self->value = value;
    mutex_unlock(&self->mutex);

    return bytes;

}

ssize_t sysfs_attrs_attr_int_show(struct kobject* kobj, struct kobj_attribute* kattr, char* buffer) {

    ssize_t bytes = 0;
    struct sysfs_attrs_attr_int* self = container_of(kattr, struct sysfs_attrs_attr_int, kattr);

    mutex_lock(&self->mutex);
    bytes = sysfs_emit(buffer, "%d\n", self->value);
    mutex_unlock(&self->mutex);

    return bytes;

}

ssize_t sysfs_attrs_attr_int_store(struct kobject* kobj, struct kobj_attribute* kattr, const char* buffer, size_t bytes) {

    ssize_t error = 0;
    int value = 0;
    struct sysfs_attrs_attr_int* self = container_of(kattr, struct sysfs_attrs_attr_int, kattr);

    if ((error = kstrtoint(buffer, 0, &value))) {
        pr_err("[%s:%s] failed to parse input as signed integer (bytes = %zu)\n", SYSFS_ATTRS_MODULE_NAME, __func__, bytes);
        return error;
    }

    mutex_lock(&self->mutex);
    self->value = value;
    mutex_unlock(&self->mutex);

    return bytes;

}

ssize_t sysfs_attrs_attr_string_show(struct kobject* kobj, struct kobj_attribute* kattr, char* buffer) {

    ssize_t bytes = 0;
    struct sysfs_attrs_attr_string* self = container_of(kattr, struct sysfs_attrs_attr_string, kattr);

    mutex_lock(&self->mutex);
    bytes = sysfs_emit(buffer, "%s\n", self->value);
    mutex_unlock(&self->mutex);

    return bytes;

}

ssize_t sysfs_attrs_attr_string_store(struct kobject* kobj, struct kobj_attribute* kattr, const char* buffer, size_t bytes) {

    ssize_t copied = 0;
    struct sysfs_attrs_attr_string* self = container_of(kattr, struct sysfs_attrs_attr_string, kattr);

    mutex_lock(&self->mutex);
    copied = strscpy(self->value, buffer, SYSFS_ATTRS_ATTR_STRING_SIZE);
    mutex_unlock(&self->mutex);

    return copied == bytes ? bytes : copied;

}

module_init(sysfs_attrs_init);
module_exit(sysfs_attrs_exit);
