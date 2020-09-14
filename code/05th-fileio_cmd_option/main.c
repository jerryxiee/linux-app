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

    int opt_cmd;

    char buf[1024];

    printf("option test \r\n");

    if (argc < 2)
    {
        printf_error_msg();
        return -1;
    }

    while ((opt_cmd = getopt(argc, argv, "rw:h")) != -1)
    {

        switch (opt_cmd)
        {

        case 'r':
        {
            /* code */
            printf("read\n");
            fd_old = open(argv[1], O_RDWR);
            if (fd_old < 0)
            {
                printf("open error\r\n");
                return -1;
            }
            read(fd_old, buf, 100);
            close(fd_old);
            printf("%s\n", buf);

            break;
        }

        case 'w':
        {
            /* code */
            printf("write\n");
            printf("The argument of -w is %s\n\n", optarg);
            fd_old = open(argv[1], O_RDWR);
            if (fd_old < 0)
            {
                printf("open error\r\n");
                return -1;
            }
            ret_status = write(fd_old, optarg, strlen(optarg));
            if (ret_status < 0)
            {
                printf("write error\r\n");
            }
            close(fd_old);
            break;
        }

        case 'h':
        {
            /* code */
            printf("help\n");
            printf_error_msg();
            break;
        }

        default:
            printf_error_msg();
            break;
        }
    }

    return 0;
}
