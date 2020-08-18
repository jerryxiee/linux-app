#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

int a = 20;
int b = 10;

void printf_error_msg(void)
{
    printf("error \r\n");
    printf("usage \r\n");
    printf("./main.bin param1 param2 \r\n");
}

int main(int argc, char const *argv[])
{
    /* code */
    int fd_old;
    int fd_new;
    int ret_status;

    char buf[1024];

    printf("file io test \r\n");

    if (argc < 3)
    {
        printf_error_msg();
        return -1;
    }

    printf("%s %s %s\r\n", argv[0], argv[1], argv[2]);

    fd_old = open(argv[1], O_RDONLY);

    if (fd_old < 0)
    {
        printf("open error\r\n");
        return -1;
    }

    fd_new = open(argv[2], O_WRONLY | O_CREAT, 777);

    while (read(fd_old, buf, 1024) > 0)
    {
        /* code */
        ret_status = write(fd_new, buf, 1024);
        if (ret_status < 0)
        {
            printf("write error\r\n");
        }
    }

    close(fd_new);
    close(fd_old);

    return 0;
}
