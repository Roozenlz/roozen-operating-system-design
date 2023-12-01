/*
chardev.h -带有ioctl定义的头文件。这里的声明必须在头文件中，因为内核模块(在chardev2.c中)和调用ioctl()的进程(在userspace_ioctl.c中)需要。
 */

#ifndef CHARDEV_H
#define CHARDEV_H

#include <linux/ioctl.h>

/* 主设备号。不依赖动态注册，因为ioctls需要获取。
 */
#define MAJOR_NUM 100

/*设置设备驱动的消息*/
#define IOCTL_SET_MSG _IOW(MAJOR_NUM, 0, char *)
/* _IOw表示正在创建一个用于传递的ioctl命令号从用户进程到内核模块的信息。第一个参数MAJOR_NUM是正在使用的主设备号。第二个参数是命令的编号(可能有几个不同的含义)。第三个参数是想要从进程传递给内核的类型。
 */

/*获取设备驱动程序的消息*/
#define IOCTL_GET_MSG _IOR(MAJOR_NUM, 1, char *)
/*这个IOCTL用于输出，获取设备驱动程序的消息。然而，仍然需要缓冲区来放置要输入的消息，当它被进程分配时。获取消息的第n个字节*/
#define IOCTL_GET_NTH_BYTE _IOWR(MAJOR_NUM, 2, int)
/* IOCTL用于输入和输出。它从用户那里接收到一个数字n，并返回消息。设备文件名*/

/* 设备文件名称 */
#define DEVICE_FILE_NAME "char_dev"
#define DEVICE_PATH "/dev/char_dev"

#endif