/*
  chardev2.c :创建输入输出字符设备
 */

#include <linux/atomic.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/module.h> /*动态添加和卸载模块*/
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/uaccess.h> /*包含get_user从用户空间读取数据 put_user将数据写入用户空间 */
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

/*设备现在是否打开?用于防止并发访问同一设备*/
static atomic_t already_open = ATOMIC_INIT(CDEV_NOT_USED);

/*当被请求时，设备将给出的消息*/
static char message[BUF_LEN + 1];

static struct class *cls;

/*当进程试图打开设备文件时调用*/
static int device_open(struct inode *inode, struct file *file)
{
    pr_info("mymodule:打开文件(%p)\n", file);

    try_module_get(THIS_MODULE);
    return SUCCESS;
}
/*当进程试图释放设备文件时调用*/
static int device_release(struct inode *inode, struct file *file)
{
    pr_info("mymodule:关闭文件(%p,%p)\n", inode, file);

    module_put(THIS_MODULE);
    return SUCCESS;
}

/*当一个进程打开设备文件试图读取时，这个函数被调用。函数用于从设备中读取数据并将其放入用户提供的缓冲区中*/
static ssize_t device_read(struct file *file, /* 参见头文件include/linux/fs.h  */
                           char __user *buffer, /*要填充的缓冲区 */
                           size_t length, /* 缓冲区长度 */
                           loff_t *offset)
{
    /*实际写入缓冲区的字节数*/
    int bytes_read = 0;
    /*读取消息的进程走了多远?如果消息大于device_read中填充的缓冲区的大小，则有用。*/
    const char *message_ptr = message;

    if (!*(message_ptr + *offset)) { /* 判断当前是否到达消息的末尾 */
        *offset = 0; /* 释放偏移量 */
        return 0; /* 读取完毕*/
    }
    /*如果消息还未读取完毕，函数会根据当前的偏移量offset获取消息的指针message_ptr。这个指针将用于逐个字符地将消息复制到缓冲区中*/
    message_ptr += *offset;

    /* 使用一个循环来将消息中的字符逐个复制到用户提供的缓冲区中*/
    while (length && *message_ptr) {
        /*因为缓冲区在用户数据段中，而不是内核数据段中，所以赋值不能工作。相反，我们必须使用put_user，它将数据从内核数据段复制到用户数据段。*/
        put_user(*(message_ptr++), buffer++);/*复制函数*/
        length--;
        bytes_read++;
    }
    /*打印出实际读取的字节数和剩余未读取的长度*/
    pr_info("mymodule:用户读取设备文件 %d 字节,剩余 %ld 字节\n", bytes_read, length);

    /*更新偏移量offset，将已读取的字节数添加到偏移量中*/
    *offset += bytes_read;

    /* Read函数应该返回实际插入缓冲区的字节数。*/
    return bytes_read;
}

/*试图写入设备文件时调用。*/
static ssize_t device_write(struct file *file, const char __user *buffer,
                            size_t length, loff_t *offset)
{
    int i;

    pr_info("mymodule:写入文件(%p,%p,%ld)", file, buffer, length);

    for (i = 0; i < length && i < BUF_LEN; i++)
        get_user(message[i], buffer + i);
    pr_info("mymodule:用户写入设备文件：%s",message);
    /*再次返回所使用的输入字符数。*/
    return i;
}

/* 当一个进程试图对设备文件执行ioctl操作时，这个函数被调用。得到了两个额外的参数(除了所有设备函数都得到的inode和文件结构之外):被调用的ioctl的编号和给ioctl函数的参数。如果ioctl是写或读/写(意味着输出返回给调用进程)，ioctl调用返回该函数的输出。
 */
static long device_ioctl(struct file *file, /* ditto */
                         unsigned int ioctl_num, /* number and param for ioctl */
                         unsigned long ioctl_param)
{
    int i;
    long ret = SUCCESS;

    /* 避免进程冲突*/
    if (atomic_cmpxchg(&already_open, CDEV_NOT_USED, CDEV_EXCLUSIVE_OPEN))
        return -EBUSY;

    /* 根据传递的ioctl_num的值进行分支判断，以执行相应的操作 */
    switch (ioctl_num) {
        /*从用户空间接收一个指向消息的指针(ioctl_param)，并将该消息设置为设备的消息。函数使用get_user函数来获取消息字符，并将其存储在内核空间的message数组中。然后，函数调用device_write函数将消息写入设备*/
        case IOCTL_SET_MSG: {
            /* 接收一个指向消息的指针(在用户空间)，并将其设置为*作为设备的消息。获取给ioctl的参数。*/
            char __user *tmp = (char __user *)ioctl_param;
            char ch;

            /* 查找消息的长度 */
            get_user(ch, tmp);
            for (i = 0; ch && i < BUF_LEN; i++, tmp++)
                get_user(ch, tmp);

            device_write(file, (char __user *)ioctl_param, i, NULL);
            break;
        }
            /*函数初始化了一个偏移量(offset)为0，并调用device_read函数从设备中读取消息。函数将读取到的消息存储在传递的ioctl_param指针指向的用户空间缓冲区中。*/
        case IOCTL_GET_MSG: {
            loff_t offset = 0;

            /* 给出当前消息给调用进程——我们得到的参数*是一个指针，填充它。*/
            i = device_read(file, (char __user *)ioctl_param, 99, &offset);

            /*在缓冲区的末尾放一个零，这样它就会正确*终止。*/
            put_user('\0', (char __user *)ioctl_param + i);
            break;
        }
            /*函数根据传递的ioctl_param参数作为索引，从message数组中获取相应位置的字节，并将其作为返回值返回*/
        case IOCTL_GET_NTH_BYTE:
            /* 这个ioctl既是输入(ioctl_param)又是输出(这个函数的返回值)。
             */
            ret = (long)message[ioctl_param];
            break;
    }

    /* 使用原子设置将already_open变量重置为CDEV_NOT_USED，表示设备已准备好接受下一个调用者 */
    atomic_set(&already_open, CDEV_NOT_USED);

    return ret;
}

/*模块声明*/
/*当进程对我们创建的设备进行操作时，这个结构体将保存调用的函数。由于指向该结构的指针保存在设备表中，因此它不可能是init_module的本地指针。NULL表示未实现的函数。*/
static struct file_operations fops = {
        .read = device_read,
        .write = device_write,
        .unlocked_ioctl = device_ioctl,
        .open = device_open,
        .release = device_release,
};

/*初始化模块-注册字符设备*/
static int __init chardev2_init(void)
{
    /* 使用register_chrdev函数注册字符设备，这样内核就知道有一个新的设备驱动程序 */
    int ret_val = register_chrdev(MAJOR_NUM, DEVICE_NAME, &fops);

    /* 负值表示错误*/
    if (ret_val < 0) {
        pr_alert("%s failed with %d\n",
                 "mymodule:注册字符设备失败", ret_val);
        return ret_val;
    }
/*如果 Kernel 的版本在 6.4.0 以上，它会使用class_create函数来创建一个设备类，否则，它会传递一个模块的指针和设备名称到class_create函数中*/
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 4, 0)
    cls = class_create(DEVICE_FILE_NAME);
#else
    cls = class_create(THIS_MODULE, DEVICE_FILE_NAME);
#endif
    /*创建一个字符设备并注册，在/dev/下会创建一个设备节点*/
    device_create(cls, NULL, MKDEV(MAJOR_NUM, 0), NULL, DEVICE_FILE_NAME);

    pr_info("mymodule：设备创建于：/dev/%s\n", DEVICE_FILE_NAME);

    return 0;
}

/*清除-从/proc中注销相应的文件*/
static void __exit chardev2_exit(void)
{
    device_destroy(cls, MKDEV(MAJOR_NUM, 0));/*移除之前device_create函数创建的设备*/
    class_destroy(cls);/*移除之前class_create函数创建的设备类*/
    pr_info("mymodule:从/proc中注销相应的文件/dev/%s\n", DEVICE_FILE_NAME);
    /* 注销先前注册的字符设备 */
    unregister_chrdev(MAJOR_NUM, DEVICE_NAME);
}
/*模块出入口函数*/
module_init(chardev2_init);
module_exit(chardev2_exit);
/*声明模块许可*/
MODULE_LICENSE("GPL");