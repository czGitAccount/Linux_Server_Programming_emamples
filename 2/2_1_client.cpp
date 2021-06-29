#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>

int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("usage: %s ip_address port_number\n", basename(argv[0]));
        return 1;
    }
    const char *ip = argv[1];
    int port = atoi(argv[2]);

    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    inet_pton(AF_INET, ip, &address.sin_addr);

    int sock = socket(PF_INET, SOCK_STREAM, 0);
    assert(sock >= 0);

    int ret = connect(sock, (struct sockaddr*)&address, sizeof(address));
    assert(ret != -1);

    const char *buf = "POST /http://example/hello.html HTTP/1.1\r\n\r\nHost:hostlocal";
    const char *buf = "GET /http://example/hello.html HTTP/1.1\r\n\r\nHost:hostlocal";
    char *recvbuf[1024];

    send(sock, buf, strlen(buf), 0);
    sleep(1);
    recv(sock, recvbuf, sizeof(recvbuf), 0);
    printf("%s\n", recvbuf);
    close(sock);
    return 0;
}