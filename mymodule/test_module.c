/* 使用ioctl来控制内核模块的进程到目前为止，
 * 我们可以使用cat作为输入和输出。
 * 但是现在我们需要执行ioctl，这需要编写我们自己的进程。
 * 设备细节，如ioctl号和主要设备文件。
 */
#include "chardev.h"

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/ioctl.h>

/*ioctl调用函数 */
/*函数用于将一条消息设置为设备的消息。它使用ioctl函数将消息发送到设备，并将返回值存储在ret_val变量中*/
int ioctl_set_msg(int file_desc, char *message)
{
    int ret_val;

    ret_val = ioctl(file_desc, IOCTL_SET_MSG, message);

    if (ret_val < 0) {
        printf("ioctl_set_msg失败:%d\n", ret_val);
    }

    return ret_val;
}
/*函数用于从设备中读取消息。它使用ioctl函数从设备中读取消息，并将读取到的消息打印出来*/
int ioctl_get_msg(int file_desc)
{
    int ret_val;
    char message[100] = { 0 };
    ret_val = ioctl(file_desc, IOCTL_GET_MSG, message);/*参数：文件描述符、IOCTL命令和缓冲区*/

    if (ret_val < 0) {
        printf("ioctl_get_msg失败:%d\n", ret_val);
    }
    printf("get_msg message:%s", message);
    putchar('\n');
    return ret_val;
}
/*函数用于从设备中读取指定位置的字节。它使用一个循环来读取指定位置的字节，并将读取到的字节打印出来*/
int ioctl_get_nth_byte(int file_desc)
{
    int i, c;

    printf("get_nth_byte message:");

    i = 0;
    do {
        c = ioctl(file_desc, IOCTL_GET_NTH_BYTE, i++);

        if (c < 0) {
            printf("\nioctl_get_nth_byte failed at the %d'th byte:\n", i);
            return c;
        }

        putchar(c);
    } while (c != 0);
    putchar('\n');
    return 0;
}

/* main调用ioctl函数*/
int main(int argc,char *argv[])
{
    int file_desc, ret_val;
    char *msg = argv[1];

    file_desc = open(DEVICE_PATH, O_RDWR);/*打开设备文件*/
    if (file_desc < 0) {
        printf("Can't open device file: %s, error:%d\n", DEVICE_PATH,
               file_desc);
        exit(EXIT_FAILURE);
    }
    /*将一条消息设置为设备的消息*/
    ret_val = ioctl_set_msg(file_desc, msg);
    if (ret_val)
        goto error;/*如果ioctl命令执行失败，程序将跳转到error标签处，打印错误信息并退出程序*/
    /*从设备中读取指定位置的字节*/
    ret_val = ioctl_get_nth_byte(file_desc);
    if (ret_val)
        goto error;
    /*从设备中读取消息*/
    ret_val = ioctl_get_msg(file_desc);
    if (ret_val)
        goto error;

    close(file_desc);
    return 0;
    error:
    close(file_desc);
    exit(EXIT_FAILURE);
}