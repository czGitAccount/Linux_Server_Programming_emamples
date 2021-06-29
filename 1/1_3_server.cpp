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

#define MAX_EVENT_NUMBER 64
#define TCP_BUFFER_SIZE 1024
#define UDP_BUFFER_SIZE 1024

int setnonblocking(int fd) {
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

void addfd(int epollfd, int fd) {
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}


int main(int argc, char *argv[]) {
    if (argc < 3) {
        std::cout << "usage : " << basename(argv[0]) << "ip_address port_number" << std::endl;
        return 1;
    }
    const char *ip = argv[1];
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

    // bzero(&address, sizeof(address));
    // address.sin_family = AF_INET;
    // inet_pton(AF_INET, ip, &address.sin_addr);
    // address.sin_port = htons(port);

    int udpfd = socket(PF_INET, SOCK_DGRAM, 0);
    assert(udpfd >=0 );
    ret = bind(udpfd, (struct sockaddr*)&address, sizeof(address));
    assert(ret != -1);

     
    epoll_event events[MAX_EVENT_NUMBER];
    int epollfd = epoll_create(5);
    assert(epollfd != -1);

    addfd(epollfd, listenfd);
    addfd(epollfd, udpfd);

    while (1) {
        int number = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);
        if (number < 0) {
            std::cout << "epoll failure" << std::endl;
            break;
        }
        for (int i = 0; i < number; i++) {
            int sockfd = events[i].data.fd;
            if (sockfd == listenfd) {
                struct sockaddr_in client_address;
                socklen_t client_addrlength = sizeof(client_address);
                int connfd = accept(listenfd, (struct sockaddr*)&client_address, &client_addrlength);
                addfd(epollfd, connfd);
            } else if (sockfd == udpfd) {
                char buf[UDP_BUFFER_SIZE];
                memset(buf, '\0', UDP_BUFFER_SIZE);
                struct sockaddr_in client_address;
                socklen_t client_addrlength = sizeof(client_addrlength);
                ret = recvfrom(udpfd, buf, UDP_BUFFER_SIZE - 1, 0, (struct sockaddr*)&client_address, &client_addrlength);
                if (ret > 0) {
                    sendto(udpfd, buf, UDP_BUFFER_SIZE - 1, 0, (struct sockaddr*)&client_address, client_addrlength);
                }
            } else if (events[i].events & EPOLLIN) {
                char buf[TCP_BUFFER_SIZE];
                while (1) {
                    memset(buf, '\0', TCP_BUFFER_SIZE);
                    ret = recv(sockfd, buf, TCP_BUFFER_SIZE - 1, 0);  // ~ 接收
                    if (ret < 0) {
                        if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
                            break;
                        }
                        close(sockfd);
                        break;
                    } else if (ret == 0) {
                        close(sockfd);
                    } else send(sockfd, buf, ret, 0);  // ~ 回射
                }
            } else std::cout << "something else happened " << std::endl;
        }
    }
    close(listenfd);
    return 0;
}
