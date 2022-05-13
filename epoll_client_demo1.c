//
//  client_epoll.c
//  network_learn
//
//  Created by david on 2022/5/12.
//

#include <netinet/in.h>
#include <sys/socket.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/select.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>
#include <arpa/inet.h>

#define IPADDRESS "127.0.0.1"
#define SERV_PORT 8787

#define  MAXLINE 1024

int main(int argc,char *argv[])
{
    int sockfd;
    struct sockaddr_in servaddr;

    sockfd = socket(AF_INET,SOCK_STREAM,0);

    bzero(&servaddr,sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(SERV_PORT);
    inet_pton(AF_INET,IPADDRESS,&servaddr.sin_addr);

    int ret;
    //连接服务端
    ret = connect(sockfd,(struct sockaddr*)&servaddr,sizeof(servaddr));
    if (ret < 0)
    {
        fprintf(stderr, "connect fail,error:%s\n", strerror(errno));
        return -1;
    }


    char buf[MAXLINE] = "hello msg server";
    printf("client send to server: %s.\n",buf);

    //向服务端发送数据
    write(sockfd,buf, sizeof(buf));

    sleep(2);

    //从服务端接收数据
    ret = read(sockfd,buf,MAXLINE);
    if (ret < 0)
    {
        perror("read error:");
    }
    else if (ret == 0)
    {
        printf("client close.\n");
    }
    else
    {
        printf("receive message is : %s\n",buf);
    }
    close(sockfd);
    return 0;
}
