#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>

void printf_error_msg(void)
{
    printf("error \r\n");
    printf("usage \r\n");
    printf("./main.bin param1 param2 \r\n");
}

#define server_ip "192.168.123.149"
#define server_port 5555

int fd_client;
struct sockaddr_in server_addr;

int main(int argc, char const *argv[])
{
    /* code */

    for (int i = 0; i < 100; i++)
    {
        /* code */

        printf("socket test \r\n");

        fd_client = socket(AF_INET, SOCK_STREAM, 0);

        if (fd_client == -1)
        {
            printf("socket create error \r\n");

            return -1;
        }
        printf("socket create success \r\n");

        memset(&server_addr, 0, sizeof(struct sockaddr_in));
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = inet_addr(server_ip);
        server_addr.sin_port = htons(server_port);
        if (-1 == connect(fd_client, (struct sockaddr *)&server_addr, sizeof(struct sockaddr)))
        {
            printf("connect error:%s\n", strerror(errno));
        }

        char send_str[] = "http://c.biancheng.net/socket/\r\n";

        if (send(fd_client, send_str, sizeof(send_str) - 1, 0) == -1)
        {
            printf("send error:%s\n", strerror(errno));
            sleep(1);
        }
        sleep(3);
        close(fd_client);
        printf("close\r\n");
    }

    return 0;
}
