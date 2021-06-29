#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <errno.h>
#include <fcntl.h>
#include <assert.h>
#include <signal.h>

#define BUFSIZE 1024

int main(int argc, char *argv[]) {
    if (argc < 3) {
        std::cout << "usage : " << basename(argv[0]) << " ip_address port_number " << std::endl;
        return -1;
    }
    const char* ip = argv[1];
    int port = atoi(argv[2]);

    struct sockaddr_in serv_sock;
    serv_sock.sin_family = AF_INET;
    serv_sock.sin_port = htons(port);
    inet_pton(AF_INET, ip, &serv_sock.sin_addr);

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    assert(sock >= 0);

    int option = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));

    int ret = connect(sock, (struct sockaddr*)&serv_sock, sizeof(serv_sock));
    assert(ret != -1);

    while (1);
    
    close(sock);

    return 0;
}