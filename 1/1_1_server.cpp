#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <iostream>

int main(int argc, char *argv[]) {
    if (argc < 3) {
        std::cout << "usage : " << basename(argv[0]) << "ip_address port_number" << std::endl;
        return 1;
    }
    // ~ 提取命令行参数 ：ip , port 
    const char *ip = argv[1];
    int port = atoi(argv[2]);
    
    int ret = 0;
    
    // ~ 根据 ip, port  初始化 sockaddr_in 结构体
    struct sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    inet_pton(AF_INET, ip, &address.sin_addr);
    
    // ~ socket, bind, listen 函数
    int listenfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(listenfd >= 0);
    ret = bind(listenfd, (struct sockaddr*)&address, sizeof(address));
    assert(ret != -1);
    ret = listen(listenfd, 5);
    assert(ret != -1);

    // ~ accept 函数
    struct sockaddr_in client_address;
    socklen_t client_addrlength = sizeof(client_address);
    int connfd = accept(listenfd, (struct sockaddr*)&client_address, &client_addrlength);
    if (connfd < 0) {
        std::cout << "errno is : " << errno << std::endl;
        close(listenfd);
    }


    char buf[1024];

    // ~ 初始化select事件集
    fd_set read_fds;
    fd_set exception_fds;
    FD_ZERO(&read_fds);
    FD_ZERO(&exception_fds);

    while (1) {
        memset(buf, '\0', sizeof(buf));
        FD_SET(connfd, &read_fds);
        FD_SET(connfd, &exception_fds);
        // ~ 监听读事件和异常事件， 最后一个参数 NULL 表示阻塞
        ret = select(connfd + 1, &read_fds, NULL, &exception_fds, NULL);
        if (ret < 0) {
            std::cout << "selection failure" << std::endl;
            break;
        }
        // ~ 对于可读时间，采用普通的 recv 函数读取数据
        if (FD_ISSET(connfd, &read_fds)) {
            ret = recv(connfd, buf, sizeof(buf) - 1, 0);
            if (ret <= 0) break;
            std::cout << "get " << ret << " bytes of normal data : " << buf << std::endl;
        } 
        // ~ 对于异常事件，采用带 MSG_OOB 标志的 recv 函数读取数据
        else if (FD_ISSET(connfd, &exception_fds)) {
            ret = recv(connfd, buf, sizeof(buf) - 1, MSG_OOB);
            if (ret <= 0) break;
            std::cout << "get " << ret << " bytes of obb data : " << buf << std::endl;
        }
    }
    close(connfd);
    close(listenfd);
    return 0;
}