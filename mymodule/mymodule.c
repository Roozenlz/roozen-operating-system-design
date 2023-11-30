/*
 * chardev2.c - Create an input/output character device
 */

#include <linux/atomic.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/module.h> /* Specifically, a module */
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/uaccess.h> /* for get_user and put_user */
#include <linux/version.h>

#include <asm/errno.h>

#include "chardev.h"
#define SUCCESS 0
#define DEVICE_NAME "char_dev"
#define BUF_LEN 80

enum {
    CDEV_NOT_USED = 0,
    CDEV_EXCLUSIVE_OPEN = 1,
};

/* 设备现在是否打开？用于防止并发访问同一设备 */
static atomic_t already_open = ATOMIC_INIT(CDEV_NOT_USED);

/* 设备在询问时会发出的消息 */
static char message[BUF_LEN + 1];

static struct class *cls;

/* 每当进程尝试打开设备文件时，都会调用此函数 */
static int device_open(struct inode *inode, struct file *file)
{
    pr_info("mymodule:打开文件(%p)\n", file);

    try_module_get(THIS_MODULE);
    return SUCCESS;
}

static int device_release(struct inode *inode, struct file *file)
{
    pr_info("mymodule:关闭文件(%p,%p)\n", inode, file);

    module_put(THIS_MODULE);
    return SUCCESS;
}

/* 每当一个进程已经打开了
 * 设备文件尝试从中读取。
 */
static ssize_t device_read(struct file *file, /*参见 include/linux/fs.h  */
                           char __user *buffer, /* 要填充的缓冲区  */
                           size_t length, /* 缓冲区的长度 */
                           loff_t *offset)
{
    /* 实际写入缓冲区的字节数 */
    int bytes_read = 0;
    /* How far did the process reading the message get? Useful if the message
     * is larger than the size of the buffer we get to fill in device_read.
     */
    const char *message_ptr = message;

    if (!*(message_ptr + *offset)) { /* we are at the end of message */
        *offset = 0; /* reset the offset */
        return 0; /* signify end of file */
    }

    message_ptr += *offset;

    /* Actually put the data into the buffer */
    while (length && *message_ptr) {
        /* Because the buffer is in the user data segment, not the kernel
         * data segment, assignment would not work. Instead, we have to
         * use put_user which copies data from the kernel data segment to
         * the user data segment.
         */
        put_user(*(message_ptr++), buffer++);
        length--;
        bytes_read++;
    }

    pr_info("mymodule:用户读取设备文件 %d 字节,剩余 %ld 字节\n", bytes_read, length);

    *offset += bytes_read;

    /* Read functions are supposed to return the number of bytes actually
     * inserted into the buffer.
     */
    return bytes_read;
}

/* called when somebody tries to write into our device file. */
static ssize_t device_write(struct file *file, const char __user *buffer,
                            size_t length, loff_t *offset)
{
    int i;

    pr_info("mymodule:写入文件(%p,%p,%ld)", file, buffer, length);

    for (i = 0; i < length && i < BUF_LEN; i++)
        get_user(message[i], buffer + i);

    pr_info("mymodule:用户写入设备文件：%s",message);
    /* Again, return the number of input characters used. */
    return i;
}

/* This function is called whenever a process tries to do an ioctl on our
 * device file. We get two extra parameters (additional to the inode and file
 * structures, which all device functions get): the number of the ioctl called
 * and the parameter given to the ioctl function.
 *
 * If the ioctl is write or read/write (meaning output is returned to the
 * calling process), the ioctl call returns the output of this function.
 */
static long device_ioctl(struct file *file, /* ditto */
             unsigned int ioctl_num, /* number and param for ioctl */
             unsigned long ioctl_param)
{
    int i;
    long ret = SUCCESS;

    /* We don't want to talk to two processes at the same time. */
    if (atomic_cmpxchg(&already_open, CDEV_NOT_USED, CDEV_EXCLUSIVE_OPEN))
        return -EBUSY;

    /* Switch according to the ioctl called */
    switch (ioctl_num) {
        case IOCTL_SET_MSG: {
            /* Receive a pointer to a message (in user space) and set that to
             * be the device's message. Get the parameter given to ioctl by
             * the process.
             */
            char __user *tmp = (char __user *)ioctl_param;
            char ch;

            /* Find the length of the message */
            get_user(ch, tmp);
            for (i = 0; ch && i < BUF_LEN; i++, tmp++)
                get_user(ch, tmp);

            device_write(file, (char __user *)ioctl_param, i, NULL);
            break;
        }
        case IOCTL_GET_MSG: {
            loff_t offset = 0;

            /* Give the current message to the calling process - the parameter
             * we got is a pointer, fill it.
             */
            i = device_read(file, (char __user *)ioctl_param, 99, &offset);

            /* Put a zero at the end of the buffer, so it will be properly
             * terminated.
             */
            put_user('\0', (char __user *)ioctl_param + i);
            break;
        }
        case IOCTL_GET_NTH_BYTE:
            /* This ioctl is both input (ioctl_param) and output (the return
             * value of this function).
             */
            ret = (long)message[ioctl_param];
            break;
    }

    /* We're now ready for our next caller */
    atomic_set(&already_open, CDEV_NOT_USED);

    return ret;
}

/* Module Declarations */

/* 此结构将保存进程对我们创建的设备执行某些操作时要调用的函数。
 * 由于指向此结构的指针保留在 devices 表中，
 * 它不能是本地的init_module. NULL 用于未实现的函数。
 */
static struct file_operations fops = {
        .read = device_read,
        .write = device_write,
        .unlocked_ioctl = device_ioctl,
        .open = device_open,
        .release = device_release, /* a.k.a. close */
};

/* 初始化模块 - 注册字符设备 */
static int __init chardev2_init(void)
{
    /* 注册字符设备（至少尝试） */
    int ret_val = register_chrdev(MAJOR_NUM, DEVICE_NAME, &fops);

    /* 负值表示错误 */
    if (ret_val < 0) {
        pr_alert("%s failed with %d\n",
                 "Sorry, registering the character device ", ret_val);
        return ret_val;
    }

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 4, 0)
    cls = class_create(DEVICE_FILE_NAME);
#else
    cls = class_create(THIS_MODULE, DEVICE_FILE_NAME);
#endif
    device_create(cls, NULL, MKDEV(MAJOR_NUM, 0), NULL, DEVICE_FILE_NAME);

    pr_info("mymodule:Device created on /dev/%s\n", DEVICE_FILE_NAME);

    return 0;
}

/* 清理 - 从 /proc 中注销相应的文件 */
static void __exit chardev2_exit(void)
{
    device_destroy(cls, MKDEV(MAJOR_NUM, 0));
    class_destroy(cls);

    /* 注销设备 */
    unregister_chrdev(MAJOR_NUM, DEVICE_NAME);
}

module_init(chardev2_init);
module_exit(chardev2_exit);

MODULE_LICENSE("GPL");