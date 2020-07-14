#define _GNU_SOURCE 1 //使用POLLRDHUP需要在代码最开始处定义_GNU_SOURCE
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cassert>
#include <cstdio>
#include <unistd.h>
#include <cstring>
#include <cstdlib>
#include <poll.h>
#include <fcntl.h>
#include <iostream>

#define BUFFER_SIZE 64

int mian(int argc,char** argv)
{
    if(argc<=2)
    {
        std::cout<<"error input"<<std::endl;
        return 1;
    }
    const char* ip=argv[1];
    int port=atoi(argv[2]);

    struct sockaddr_in server_address;
    bzero(&server_address,sizeof(server_address));
    server_address.sin_family=AF_INET;
    inet_pton(AF_INET,ip,&server_address.sin_addr);
    server_address.sin_port=htons(port);

    int sockfd=socket(PF_INET,SOCK_STREAM,0);
    assert(socket>=0);
    if(connect(sockfd,(struct sockaddr*)&server_address,sizeof(server_address))<0)
    {
        std::cout<<"connection failed"<<std::endl;
        close(sockfd);
        return 1;
    }

    pollfd fds[2];
    fds[0].fd=0;
    fds[0].events=POLLIN;
    fds[0].revents=0;
    fds[1].fd=sockfd;
    fds[1].events=POLLIN | POLLRDHUP;
    fds[1].revents=0;

    char read_buf[BUFFER_SIZE];
    int pipefd[2];
    int ret=pipe(pipefd);
    assert(ret!=-1);

    while(1)
    {
        ret=poll(fds,2,-1);
        if(ret<0)
        {
            std::cout<<"poll failure"<<std::endl;
            break;
        }

        if(fds[1].revents & POLLRDHUP)
        {
            std::cout<<"server close the connection!"<<std::endl;
            break;
        }
        else if(fds[1].revents & POLLIN)
        {
            memset(read_buf,0,BUFFER_SIZE);
            recv(fds[1].fd,read_buf,BUFFER_SIZE-1,0);
            std::cout<<read_buf<<std::endl;
        }

        if(fds[0].revents & POLLIN)
        {
            ret=splice(0,NULL,pipefd[1],NULL,32768,SPLICE_F_MORE | SPLICE_F_MOVE);
            ret=splice(pipefd[0],NULL,sockfd,NULL,32768,SPLICE_F_MORE | SPLICE_F_MOVE);
        }
    }

    close(sockfd);
    return 0;
}