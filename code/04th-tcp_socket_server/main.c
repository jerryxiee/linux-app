#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <signal.h>

void printf_error_msg(void)
{
    printf("error \r\n");
    printf("usage \r\n");
    printf("./main.bin param1 param2 \r\n");
}

#define server_ip "192.168.123.107" //"127.0.0.1" "192.168.123.107"
#define server_port 5555

int fd_server;
int fd_client;
struct sockaddr_in server_addr;
struct sockaddr_in client_addr;

socklen_t client_addr_len;

int main(int argc, char const *argv[])
{
    /* code */

    printf("socket server test \r\n");

    /*
    创建
    绑定
    监听
    接受连接
    收发数据
    关闭
    */

    //创建
    fd_server = socket(AF_INET, SOCK_STREAM, 0);

    if (fd_server == -1)
    {
        printf("socket create error:%s\n", strerror(errno));
        return -1;
    }
    else
    {
        printf("socket create success \r\n");
    }

    //绑定
    memset(&server_addr, 0, sizeof(struct sockaddr_in));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(server_ip);
    server_addr.sin_port = htons(server_port);

    if (-1 == bind(fd_server, (struct sockaddr *)&server_addr, sizeof(struct sockaddr)))
    {
        printf("socket bind error:%s\n", strerror(errno));
    }
    else
    {
        printf("socket bind success \r\n");
    }

    //监听
    if (-1 == listen(fd_server, 5))
    {
        printf("socket listen error:%s\n", strerror(errno));
    }
    else
    {
        printf("socket listen success \r\n");
    }

    while (1)
    {
        //接收连接
        client_addr_len = sizeof(struct sockaddr);
        fd_client = accept(fd_server, (struct sockaddr *)&client_addr, &client_addr_len);

        if (-1 == fd_client)
        {
            printf("socket accept error:%s\n", strerror(errno));
            continue;
        }
        else
        {
            printf("socket accept success \r\n");
            printf("get a client, ip:%s, port:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
        }

        pid_t pid = fork();

        if (pid > 0)
        {
            printf("close process id%d", pid);
            close(fd_client); //关闭子进程的客户端socket
        }
        else if (pid == 0)
        {
            if (fork() > 0)
            {
                exit(0);
            }

            close(fd_server); //子进程不需要监听

            uint8_t buffer_recv[4096] = {0x00};
            ssize_t ret;

            while (1)
            {
                /* code */
                ret = recv(fd_client, &buffer_recv, 4096, 0);

                printf("recv client, ip:%s, port:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

                if (-1 == ret)
                {
                    printf("socket recv error:%s\n", strerror(errno));
                    break;
                }
                else if (0 == ret)
                {
                    break;
                }
                else
                {
                    printf("socket recv success \r\n");

                    if (send(fd_client, &buffer_recv, ret, 0) == -1)
                    {
                        printf("socket send error:%s\n", strerror(errno));
                        break;
                    }
                    else
                    {
                        printf("socket send success \r\n");
                    }
                }
            }
            printf("client disconnet\r\n");
            close(fd_client); //关闭子进程的客户端socket
            kill(pid, SIGUSR1);
            exit(0);
        }
        else
        {
            /* code */
            printf("socket fork error:%s\n", strerror(errno));
            return 2;
        }
    }

    return 0;
}
