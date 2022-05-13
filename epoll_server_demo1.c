//
//  server_epoll.c
//  network_learn
//
//  Created by david on 2022/5/12.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>

#define IP   "127.0.0.1"
#define PORT        8787
#define MAXSIZE     1024
#define LISTENQ     5
#define FDSIZE      1000
#define EPOLLEVENTS 100

//设置套接字为非阻塞模式
static void setnonblocking(int sock);

//IO多路复用epoll
static void do_epoll(int listenfd);

//处理接收到的连接
static void handle_accpet(int epollfd,int listenfd);
//读处理
static void do_read(int epollfd,int fd,char *buf);
//写处理
static void do_write(int epollfd,int fd,char *buf);
//添加事件
static void add_event(int epollfd,int fd,int state);
//修改事件
static void modify_event(int epollfd,int fd,int state);
//删除事件
static void delete_event(int epollfd,int fd,int state);

int main(int argc,char *argv[])
{
    int  listenfd;
    struct sockaddr_in servaddr;
    listenfd = socket(AF_INET,SOCK_STREAM,0);
    if (listenfd == -1)
    {
        perror("socket error:");
        exit(1);
    }
    //设置监听套接字为非阻塞模式
    setnonblocking(listenfd);

    bzero(&servaddr,sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    inet_pton(AF_INET,IP,&servaddr.sin_addr);
    servaddr.sin_port = htons(PORT);

    //绑定接口
    if (bind(listenfd,(struct sockaddr*)&servaddr,sizeof(servaddr)) == -1)
    {
        perror("bind error: ");
        exit(1);
    }

    //监听端口
    if (listen(listenfd,LISTENQ) == -1)
    {
        perror("listen error: ");
        exit(1);
    }
    do_epoll(listenfd);    //执行epoll
    return 0;
}

static void do_epoll(int listenfd)
{
    int i,fd,epollfd;
    struct epoll_event events[EPOLLEVENTS];
    int ret;
    char buf[MAXSIZE];
    memset(buf,0,MAXSIZE);

    //创建一个描述符
    epollfd = epoll_create(FDSIZE);

    //添加监听描述符事件
    add_event(epollfd,listenfd,EPOLLIN);
    for ( ; ; )
    {
        //获取已经准备好的描述符事件
        printf("waiting on epoll_wait...\n");
        ret = epoll_wait(epollfd,events,EPOLLEVENTS,3000);
        if (ret < 0)
        {
            perror("epoll_wait() error\n");
            break;
        }
        if (ret==0)
        {
            printf("epoll_wait time out.\n");
        }
        for (i = 0;i < ret;i++)
        {
            fd = events[i].data.fd;
            //根据描述符的类型和事件类型进行处理
            if ((fd == listenfd) &&(events[i].events & EPOLLIN))   //连接请求
                handle_accpet(epollfd,listenfd);
            else if (events[i].events & EPOLLIN)     //读事件
                do_read(epollfd,fd,buf);
            else if (events[i].events & EPOLLOUT)    //写事件
                do_write(epollfd,fd,buf);
        }
    }
    close(epollfd);
}

static void handle_accpet(int epollfd,int listenfd)
{
    int new_fd;
    struct sockaddr_in cliaddr;
    socklen_t  cliaddrlen;
    new_fd = accept(listenfd,(struct sockaddr*)&cliaddr,&cliaddrlen);
    if (new_fd == -1)
        perror("accpet error:");
    else
    {
        //设置为阻塞模式
        setnonblocking(new_fd);

        printf("accept a new client: %s:%d\n",inet_ntoa(cliaddr.sin_addr),cliaddr.sin_port);
        //添加一个客户描述符和事件
        add_event(epollfd,new_fd,EPOLLIN);
    }
}

static void do_read(int epollfd,int fd,char *buf)
{
    int nread;
    nread = read(fd,buf,MAXSIZE);
    if (nread == -1)
    {
        perror("read error.\n");
        //关闭并删除注册的文件描述符
        close(fd);
        delete_event(epollfd,fd,EPOLLIN);
    }
    else if (nread == 0)
    {
        printf("client close.\n");
        close(fd);
        delete_event(epollfd,fd,EPOLLIN);
    }
    else
    {
        printf("receive message: %s\n",buf);
        //修改描述符对应的事件，由读改为写
        modify_event(epollfd,fd,EPOLLOUT|EPOLLET);
    }
}

static void do_write(int epollfd,int fd,char *buf)
{
    int nwrite;
    nwrite = write(fd,buf,strlen(buf));
    if (nwrite == -1)
    {
        perror("write error:");
        close(fd);
        delete_event(epollfd,fd,EPOLLOUT);
    }
    else
    {
        //修改事件为读
        modify_event(epollfd,fd,EPOLLIN|EPOLLET);
    }
    memset(buf,0,MAXSIZE);
}

static void add_event(int epollfd,int fd,int state)
{
    struct epoll_event ev;
    ev.events = state;
    ev.data.fd = fd;
    epoll_ctl(epollfd,EPOLL_CTL_ADD,fd,&ev);
}

static void delete_event(int epollfd,int fd,int state)
{
    struct epoll_event ev;
    ev.events = state;
    ev.data.fd = fd;
    epoll_ctl(epollfd,EPOLL_CTL_DEL,fd,&ev);
}

static void modify_event(int epollfd,int fd,int state)
{
    struct epoll_event ev;
    ev.events = state;
    ev.data.fd = fd;
    epoll_ctl(epollfd,EPOLL_CTL_MOD,fd,&ev);
}

static void setnonblocking(int sock)
{
    int opts;
    opts = fcntl(sock, F_GETFL);
    if(opts < 0) {
        perror("fcntl(sock, GETFL)");
        exit(1);
    }

    opts = opts | O_NONBLOCK;

    if(fcntl(sock, F_SETFL, opts) < 0) {
        perror("fcntl(sock, SETFL, opts)");
        exit(1);
    }
}
