/*******************************************************************************
 * @Description: 大文件操作 2GB以上，创建 读 写 
 * @Author: Stream
 * @Version: V0.0.1
 * @Date: 2020-09-14 14:45:06
 * @LastEditors: Stream
 * @LastEditTime: 2020-09-14 15:40:30
 * @FilePath: \code\06th-fileio_big\main.c
 * @ChangeLog: ChangeLog
*******************************************************************************/
#define _FILE_OFFSET_BITS 64

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

int a = 20;
int b = 10;

void printf_error_msg(void)
{
    printf("usage \r\n");
    printf("./main.bin param1 -r -w -h\r\n");
}
//char * const argv[]
int main(int argc, char *const argv[])
{
    /* code */
    int fd_old;
    int fd_new;
    int ret_status;
    off_t fd_offset;
    char buf[1024];

    printf("hello world\r\n");

    fd_old = open(argv[1], O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    if (fd_old < 0)
    {
        printf("open error\r\n");
        return -1;
    }

    ret_status = write(fd_old, "ABC123 head\n", 6);
    if (ret_status < 0)
    {
        printf("write error\r\n");
    }

    fd_offset = 3221225472; //3GB  3221225472

    ret_status = lseek(fd_old, fd_offset, SEEK_SET);
    if (ret_status == -1)
    {
        printf("lseek error\r\n");
        return -1;
    }

    ret_status = write(fd_old, "\nABC123 end\n", 6);
    if (ret_status < 0)
    {
        printf("write error\r\n");
    }

    close(fd_old);

    return 0;
}
