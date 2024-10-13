#include <linux/atomic.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/seq_file.h>
#include <linux/mutex.h> // Include the mutex header
#include <linux/kobject.h> // Include kobject header for sysfs interaction
#include <asm/errno.h>
#include "chardevice.h"

#define SUCCESS 0
#define DEVICE_NAME "chardevice"
#define BUF_LEN 1024

static DEFINE_MUTEX(mymutex); // Define the mutex
static struct kobject *device_kobj; // Create a kobject to be used with sysfs

static loff_t read_position = 0;
static char memory[BUF_LEN + 1];
static int memory_end = 0;
static struct class *cls;
static uint32_t encrpytion_key = 0;

module_param(encrpytion_key, uint, 0660);

static ssize_t device_size_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf) {
    return sprintf(buf, "%d\n", memory_end);
}

static struct kobj_attribute device_size_attr = __ATTR(size, 0444, device_size_show, NULL);

static void *device_seq_start(struct seq_file *s, loff_t *pos) {
    read_position = *pos;
    return (*pos < BUF_LEN && memory[*pos] != '\0') ? pos : NULL;
}

static void *device_seq_next(struct seq_file *s, void *v, loff_t *pos) {
    char *ptr = &memory[*pos];
    while (*ptr != '\n' && *ptr != '\0') {
        (*pos)++;
        ptr++;
    }
    if (*ptr == '\n') {
        (*pos)++;
    }
    return (*pos < BUF_LEN && memory[*pos] != '\0') ? pos : NULL;
}

static void device_seq_stop(struct seq_file *s, void *v) {}

static int device_seq_show(struct seq_file *s, void *v) {
    loff_t *pos = v;
    char *ptr = &memory[*pos];
    while (*ptr != '\n' && *ptr != '\0') {
        seq_putc(s, *ptr);
        ptr++;
    }
    seq_putc(s, '\n');
    return 0;
}

static struct seq_operations device_seq_ops = {
        .start = device_seq_start,
        .next = device_seq_next,
        .stop = device_seq_stop,
        .show = device_seq_show
};

static int device_open(struct inode *inode, struct file *file) {
    if (!try_module_get(THIS_MODULE))
        return -ENODEV;
    return seq_open(file, &device_seq_ops);
}

static int device_release(struct inode *inode, struct file *file) {
    pr_info("device_release(%p)\n", file);
    seq_release(inode, file);
    module_put(THIS_MODULE);
    return SUCCESS;
}

static ssize_t device_write(struct file *file, const char __user *buffer, size_t length, loff_t *offset) {
int i;
mutex_lock(&mymutex); // Acquire the mutex lock
pr_info("Write Locked\n");
pr_info("device_write(%p,%p,%ld)", file, buffer, length);
if (memory_end > 0 && memory_end < BUF_LEN) {
// Add a newline between messages
memory[memory_end] = '\n';
memory_end++;
}
for (i = 0; i < length && memory_end < BUF_LEN; i++) {
char c;
get_user(c, buffer + i);
if (c != '\n') {
memory[memory_end] = c ^ encrpytion_key;
} else {
memory[memory_end] = c;
}
memory_end++; // Increase the end of the written data
// Ensure the buffer is null-terminated
if (memory_end < BUF_LEN) {
memory[memory_end] = '\0';
} else {
memory[BUF_LEN] = '\0';
}
read_position = 0;
}
mutex_unlock(&mymutex); // Release the mutex lock
pr_info("Write Unlocked\n");
return i;
}

static void reencrypt_data(uint32_t old_key, uint32_t new_key) {
    int i;
    // Go through each character in the memory buffer
    for (i = 0; i < memory_end; i++) {
        // Decrypt data with old key
        memory[i] = memory[i] ^ old_key;
        // Encrypt data again with new key
        memory[i] = memory[i] ^ new_key;
    }
}

static long device_ioctl(struct file *file, unsigned int ioctl_num, unsigned long ioctl_param) {
    int ret = SUCCESS;
    uint32_t old_key, new_key;
    mutex_lock(&mymutex); // Acquire the mutex lock
    pr_info("Write Locked\n");
    switch (ioctl_num) {
        case IOCTL_SET_KEY: {
            if (!access_ok((void __user *)ioctl_param, sizeof(uint32_t)))
            return -EACCES;
            // Save the old key before we overwrite it
            old_key = encrpytion_key;
            get_user(new_key, (unsigned int __user *)ioctl_param);
            // If the key has changed, reencrypt the data with the new key
            if (new_key != old_key) {
                encrpytion_key = new_key;
                reencrypt_data(old_key, new_key);
            }
            break;
        }
        default:
            break;
    }
    mutex_unlock(&mymutex); // Release the mutex lock
    pr_info("Write Unlocked\n");
    return ret;
}

static struct file_operations device_fops = {
        .owner = THIS_MODULE,
        .open = device_open,
        .read = seq_read,
        .write = device_write,
        .unlocked_ioctl = device_ioctl,
        .release = device_release,
        .llseek = seq_lseek,
};

static int __init chardev_init(void) {
    int ret_val = register_chrdev(MAJOR_NUM, DEVICE_NAME, &device_fops);
    if (ret_val < 0) {
        pr_alert("Failed to register char device\n");
        return ret_val;
    }
    cls = class_create(THIS_MODULE, DEVICE_FILE_NAME);
    device_create(cls, NULL, MKDEV(MAJOR_NUM, 0), NULL, DEVICE_FILE_NAME);
    pr_info("Device created on /dev/%s\n", DEVICE_FILE_NAME);

    // Create a kobject and add it to the sysfs
    device_kobj = kobject_create_and_add("chardevice", kernel_kobj);
    if(!device_kobj)
        return -ENOMEM;
    if(sysfs_create_file(device_kobj, &device_size_attr.attr)){
        kobject_put(device_kobj);
        return -ENOMEM;
    }

    return 0;
}

static void __exit chardev_exit(void) {
    sysfs_remove_file(device_kobj, &device_size_attr.attr); // Remove the sysfs file
    kobject_put(device_kobj); // Remove the kobject
    device_destroy(cls, MKDEV(MAJOR_NUM, 0));
    class_destroy(cls);
    unregister_chrdev(MAJOR_NUM, DEVICE_NAME);
}

module_init(chardev_init);
module_exit(chardev_exit);

MODULE_LICENSE("GPL");