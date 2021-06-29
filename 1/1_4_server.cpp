#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <pthread.h>
#include <iostream>

#define MAX_EVENT_NUMBER 1024
#define BUFFER_SIZE 10

int setnonblocking(int fd) {
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

void addfd(int epollfd, int fd, bool enable_et) {
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN;
    if (enable_et) {
        event.events |= EPOLLET;
    }
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}

void lt(epoll_event* events, int number, int epollfd, int listenfd) {
    char buf[BUFFER_SIZE];
    for (int i = 0; i < number; i++) {
        int sockfd = events[i].data.fd;
        if (sockfd == listenfd) {
            struct sockaddr_in client_address;
            socklen_t client_addrlenght = sizeof(client_address);
            int connfd = accept(sockfd, (struct sockaddr*)&client_address, &client_addrlenght);
            addfd(epollfd, connfd, false);  // ~ connfd 不能使用 ET 模式            
        }
        else if (events[i].events & EPOLLIN) {
            std::cout << "event trigger once " << std::endl;
            memset(buf, '\0', BUFFER_SIZE);
            int ret = recv(sockfd, buf, BUFFER_SIZE - 1, 0);
            if (ret < 0) {
                close(sockfd);
                continue;
            }
            std::cout << "get " << ret << " bytes of content: " << buf << std::endl;            
        }
        else std::cout << "something else happened " << std::endl;
    }    
}

void et(epoll_event* events, int number, int epollfd, int listenfd) {
    char buf[BUFFER_SIZE];
    for (int i = 0; i < number; i++) {
        int sockfd = events[i].data.fd;
        if (sockfd == listenfd) {
            struct sockaddr_in client_address;
            socklen_t client_addrlength = sizeof(client_address);
            int connfd = accept(listenfd, (struct sockaddr*)&client_address, &client_addrlength);
            addfd(epollfd, connfd, true);            
        } else if (events[i].events & EPOLLIN) {
            std::cout << "event trigger once" << std::endl;
            while (1) {
                memset(buf, '\0', BUFFER_SIZE);
                int ret = recv(sockfd, buf, BUFFER_SIZE - 1, 0);
                if (ret < 0) {
                    if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
                        std::cout << "read later" << std::endl;
                        break;
                    }
                    close(sockfd);
                    break;
                } else if (ret == 0) {
                    close(sockfd);
                } else {
                    std::cout << "get " << ret << " bytes of content: " << buf << std::endl; 
                }
            }
        } else {
            std::cout << "something else happened " << std::endl;
        }
    }

}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("usage : %s ip_address port_nunber\n", basename(argv[0]));
        return 1;        
    }
    const char* ip = argv[1];
    int port = atoi(argv[2]);

    int ret = 0;
    struct sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    inet_pton(AF_INET, ip, &address.sin_addr);

    int listenfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(listenfd >= 0);

    int option = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));

    ret = bind(listenfd, (struct sockaddr*)&address, sizeof(address));
    assert(ret != -1);

    ret = listen(listenfd, 5);
    assert(ret != -1);

    epoll_event events[MAX_EVENT_NUMBER];
    int epollfd = epoll_create(5);
    assert(epollfd != -1);
    addfd(epollfd, listenfd, true);

    while (1) {
        int ret = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);
        if (ret < 0) {
            std::cout << "epoll failure" << std::endl;
            break;
        }
        lt(events, ret, epollfd, listenfd); // ~ 使用 LT 模式
        // et(events, ret, epollfd, listenfd);  // ~ 使用 ET 模式
    }
    close(listenfd);
    return 0;
}