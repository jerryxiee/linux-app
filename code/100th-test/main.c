/*******************************************************************************
 * @Description: 中间测试
 * @Author: Stream
 * @Version: V0.0.1
 * @Date: 2020-09-14 14:45:06
 * @LastEditors: Stream
 * @LastEditTime: 2020-09-14 16:50:12
 * @FilePath: \code\100th-test\main.c
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
    int fd_new2;

    off_t offset_old;

    char buffer[1024];

    printf("hello world\r\n");

    fd_old = open(argv[1], O_RDWR | O_CREAT | O_TRUNC, S_IWUSR | S_IWUSR);
    fd_new = dup(fd_old);
    fd_new2 = open(argv[1], O_RDWR);

    /*
hello,
hello,world
HELLO,world
FD333,world
*/
    write(fd_old, "hello,", 6);
    //
    memset(buffer, 0, 1024);
    offset_old = lseek(fd_old, 0, SEEK_CUR);
    lseek(fd_old, 0, SEEK_SET);
    read(fd_old, buffer, 100);
    lseek(fd_old, offset_old, SEEK_SET);
    printf("%s---\n", buffer);
    //

    write(fd_new, "world", 6);
    //
    memset(buffer, 0, 1024);
    offset_old = lseek(fd_new, 0, SEEK_CUR);
    lseek(fd_new, 0, SEEK_SET);
    read(fd_new, buffer, 100);
    lseek(fd_new, offset_old, SEEK_SET);
    printf("%s---\n", buffer);
    //

    lseek(fd_new, 0, SEEK_SET);

    write(fd_old, "HELLO,", 6);
    //
    memset(buffer, 0, 1024);
    offset_old = lseek(fd_old, 0, SEEK_CUR);
    lseek(fd_old, 0, SEEK_SET);
    read(fd_old, buffer, 100);
    lseek(fd_old, offset_old, SEEK_SET);
    printf("%s---\n", buffer);
    //

    write(fd_new2, "FD333,", 6);
    //
    memset(buffer, 0, 1024);
    offset_old = lseek(fd_new2, 0, SEEK_CUR);
    lseek(fd_new2, 0, SEEK_SET);
    read(fd_new2, buffer, 100);
    lseek(fd_new2, offset_old, SEEK_SET);
    printf("%s---\n", buffer);
    //

    close(fd_old);
    close(fd_new);
    close(fd_new2);

    return 0;
}
