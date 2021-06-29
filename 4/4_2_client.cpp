#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <iostream>
#include <assert.h>

int main(int argc, char *argv[]) {
    if (argc < 3) {
        std::cout << "usage : " << basename(argv[0]) << " ip_address port_number " << std::endl;
        return 1;
    }
    const char *ip = argv[1];
    int port = atoi(argv[2]);

    int ret = 0;
    struct sockaddr_in serv_addr;
    bzero(&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    inet_pton(AF_INET, ip, &serv_addr.sin_addr);

    int sock = socket(PF_INET, SOCK_STREAM, 0);
    assert(sock >= 0);

    ret = connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
    assert(ret != -1);

    const char *sendbuf = "hello\n";
    while (1) {        
        send(sock, sendbuf, sizeof(sendbuf), 0);
        // sleep(10);
        sleep(20);
    }
    close(sock);
    return 0;
}
