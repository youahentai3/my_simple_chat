#include <arpa/inet.h>
#include <cassert>
#include <cstdio>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <cstdlib>
#include <poll.h>
#include <iostream>
#include <memory>

#define USERLIMIT 5 //最大用户数量限制
#define BUFFER_SIZE 64  //读缓冲区大小
#define FD_LIMIT 65535  //文件描述符数量限制

struct client_data  //存储客户端数据
{
    sockaddr_in address;
    char* weite_buf;
    char buf[BUFFER_SIZE];
};

int setnonlocking(int fd)  //将文件描述符对应的文件设置为非阻塞
{
    int old_option=fcntl(fd,F_GETFL);
    int new_option=old_option | O_NONBLOCK;
    fcntl(fd,F_SETFL,new_option);
    return old_option;
}

int main(int argc,char** argv)
{
    if(argc<=2)
    {
        std::cout<<"error input"<<std::endl;
        return 1;
    }
    const char* ip=argv[1];
    int port=atoi(argv[2]);

    int ret=0;
    struct sockaddr_in address;
    bzero(&address,sizeof(address));
    address.sin_family=AF_INET;
    inet_pton(AF_INET,ip,&address.sin_addr);
    address.sin_port=htons(port);

    int listenfd=socket(PF_INET,SOCK_STREAM,0);
    assert(listenfd>=0);

    ret=bind(listenfd,(sockaddr*)&address,sizeof(address));
    assert(ret!=-1);

    ret=listen(listenfd,USERLIMIT);
    assert(ret!=-1);

    std::unique_ptr<client_data[]> users(new client_data[FD_LIMIT]);
    pollfd fds[USERLIMIT+1];
    int user_counter=0;
    for(int i=1;i<=USERLIMIT;i++)
    {
        fds[i].fd=-1;
        fds[i].events=0;
    }
    fds[0].fd=listenfd;
    fds[0].events=POLLIN | POLLERR;
    fds[0].revents=0;

    while(1)
    {
        ret=poll(fds,user_counter+1,-1);
        if(ret<0)
        {
            std::cout<<"poll failure"<<std::endl;
            break;
        }

        for(int i=0;i<=user_counter;i++)
        {
            if(fds[i].fd==listenfd && (fds[i].revents & POLLIN))
            {
                sockaddr_in client_address;
                socklen_t client_addrlength=sizeof(client_address);
                int confd=accept(listenfd,(sockaddr*)&client_address,&client_addrlength);
                if(confd<0)
                {
                    std::cout<<errno<<std::endl;
                    continue;
                }
                if(user_counter>=USERLIMIT)
                {
                    const char* info="too many users\n";
                    std::cout<<info<<std::endl;
                    send(confd,info,strlen(info),0);
                    close(confd);
                    continue;
                }
                user_counter++;
                users[confd].address=client_address;
                setnonlocking(confd);
                fds[user_counter].fd=confd;
                fds[user_counter].events=POLLIN | POLLRDHUP | POLLERR;
                fds[user_counter].revents=0;
                std::cout<<"comes a new user, now have "<<user_counter<<" users"<<std::endl;
            }
            else if(fds[i].revents & POLLERR)
            {
                std::cout<<"get a error from "<<fds[i].fd<<std::endl;
                char errors[100];
                memset(errors,0,100);
                socklen_t length=sizeof(errors);
                if (getsockopt(fds[i].fd,SOL_SOCKET,SO_ERROR,errors,&length)<0)
                {
                    std::cout<<"get socket option failed"<<std::endl;
                }
                continue;
            }
            else if(fds[i].revents & POLLRDHUP)
            {
                //users[fds[i].fd]=users[fds[user_counter].fd];
                close(fds[i].fd);
                fds[i]=fds[user_counter];
                i--;
                user_counter--;
                std::cout<<"a client left"<<std::endl;
            }
            else if (fds[i].revents & POLLIN)
            {
                int confd=fds[i].fd;
                memset(users[confd].buf,0,BUFFER_SIZE);
                ret=recv(confd,users[confd].buf,BUFFER_SIZE-1,0);
                std::cout<<"get "<<ret<<" bytes of client data "<<users[confd].buf<<" from "<<confd<<std::endl;
                if(ret<0)
                {
                    if(errno!=EAGAIN)
                    {
                        close(confd);
                        users[fds[i].fd]=users[fds[user_counter].fd];
                        fds[i]=fds[user_counter];
                        i--;
                        user_counter--;
                    }
                }
                else if(!ret)
                    ;
                else
                {
                    for(int j=1;j<=user_counter;j++)
                    {
                        if(fds[j].fd==confd)
                            continue;
                        fds[j].events &= ~POLLIN;
                        //std::cout<<fds[j].events<<std::endl;
                        fds[j].events |= POLLOUT;
                        users[fds[j].fd].weite_buf=users[confd].buf;
                    }
                }
            }
            else if(fds[i].revents & POLLOUT)
            {
                int confd=fds[i].fd;
                if(!users[confd].weite_buf)
                    continue;
                ret=send(confd,users[confd].weite_buf,strlen(users[confd].weite_buf),0);
                users[confd].weite_buf=nullptr;
                fds[i].events &= ~POLLOUT;
                fds[i].events |=POLLIN;
            }
        }
    }

    close(listenfd);
    return 0;
}