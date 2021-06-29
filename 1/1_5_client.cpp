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
#define TCP_BUFFER_SIZE 512
#define UDP_BUFFER_SIZE 1024



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

    int tcpsock = socket(PF_INET, SOCK_STREAM, 0);
    assert(tcpsock >= 0);

    ret = connect(tcpsock, (struct sockaddr*)&address, sizeof(address));


    const char *sendbuf1 = "send01\n";
    const char *sendbuf2 = "send02\n";

    send(tcpsock, sendbuf1, sizeof(sendbuf1), 0);   
    sleep(5);
    // send(tcpsock, sendbuf2, sizeof(sendbuf2), 0);
    // sleep(10);
    close(tcpsock);

    return 0;
}