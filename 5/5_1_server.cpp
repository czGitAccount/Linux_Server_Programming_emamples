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
#include <signal.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/shm.h>
#include <iostream>

#define USER_LIMIT 5
#define BUFFER_SIZE 1024
#define FD_LIMIT 65535
#define MAX_EVENT_NUMBER 1024
#define PROCESS_LIMIT 65536

struct client_data {
    sockaddr_in address;    // ~ 客户端 socket 地址
    int connfd;             // ~ socket 文件描述符
    pid_t pid;              // ~ 处理这个连接的子进程的 PID
    int pipefd[2];          // ~ 和父进程通信的管道
};

static const char* shm_name = "/my_shm";
int sig_pipefd[2];
int epollfd;
int listenfd;
int shmfd;
char *share_mem = 0;

int key;
int shmid;


client_data *users = 0;

int *sub_process = 0;

int user_count = 0;
bool stop_child = false;

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

void sig_handler(int sig) {
    int save_errno = errno;
    int msg = sig;
    send(sig_pipefd[1], (char*)&msg, 1, 0);
    errno = save_errno;
}

void addsig(int sig, void(*handler)(int), bool restart = true) {
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = handler;
    if (restart) {
        sa.sa_flags |= SA_RESTART;
    }
    sigfillset(&sa.sa_mask);
    assert(sigaction(sig, &sa, NULL) != -1);
}

void del_resource() {
    close(sig_pipefd[0]);
    close(sig_pipefd[1]);
    close(listenfd);
    close(epollfd);
    // shm_unlink(shm_name);
    delete [] users;
    delete [] sub_process;
}


void child_term_handler(int sig) {
    stop_child = true;
}

int run_child(int idx, client_data* users, char* share_mem) {
    epoll_event events[MAX_EVENT_NUMBER];
    int child_epollfd = epoll_create(5);
    assert(child_epollfd != -1);
    int connfd = users[idx].connfd;
    addfd(child_epollfd, connfd);
    int pipefd = users[idx].pipefd[1];
    addfd(child_epollfd, pipefd);
    int ret;

    addsig(SIGTERM, child_term_handler, false);

    while (!stop_child) {
        int number = epoll_wait(child_epollfd, events, MAX_EVENT_NUMBER, -1);
        if ((number < 0) && (errno != EINTR)) {
            std::cout << "epoll failure" << std::endl;
            break;
        }
        for (int i = 0; i < number; i++) {
            int sockfd = events[i].data.fd;
            // ~ 本子进程负责的客户连接有数据到达
            if ((sockfd == connfd) && (events[i].events & EPOLLIN)) {
                memset(share_mem + idx * BUFFER_SIZE, '\0', BUFFER_SIZE);
                ret = recv(connfd, share_mem + idx * BUFFER_SIZE, BUFFER_SIZE - 1, 0); // ~ 接收数据
                if (ret < 0) {
                    if (errno != EAGAIN) stop_child = true;
                } else if (ret == 0) stop_child = true;
                else send(pipefd, (char*)&idx, sizeof(idx), 0); // ~ 通知主进程处理
            } else if ((sockfd == pipefd) && (events[i].events & EPOLLIN)) { // ~ 子进程被主进程通知
                int client = 0;                                              // ~ 发送数据给哪个客户端
                ret = recv(sockfd, (char*)&client, sizeof(client), 0);
                if (ret < 0) {
                    if (errno != EAGAIN) stop_child = true;
                } else if (ret == 0) stop_child = true;
                else send(connfd, share_mem + client * BUFFER_SIZE, BUFFER_SIZE, 0);
            } else continue;
        }
    }
    close(connfd);
    close(pipefd);
    close(child_epollfd);
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        std::cout << "usage: " << basename(argv[0]) << " ip_address port_number " << std::endl;
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

    listenfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(listenfd >= 0);

    int option = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));

    ret = bind(listenfd, (struct sockaddr*)&address, sizeof(address));
    assert(ret != -1);

    ret = listen(listenfd, 5);
    assert(ret != -1);

    user_count = 0;
    users = new client_data[USER_LIMIT + 1]; // ~ 客户数据
    sub_process = new int[PROCESS_LIMIT];  // ~ 按照子进程 pid 索引存 client 索引（[0...user_count]）
    for (int i = 0; i < PROCESS_LIMIT; ++i) {
        sub_process[i] = -1; // ~ 初始化
    }
    epoll_event events[MAX_EVENT_NUMBER];
    epollfd = epoll_create(5);
    assert(epollfd != -1);
    addfd(epollfd, listenfd);

    ret = socketpair(PF_UNIX, SOCK_STREAM, 0, sig_pipefd);  // ~ 接收信号所用的管道
    assert(ret != -1);
    setnonblocking(sig_pipefd[1]);
    addfd(epollfd, sig_pipefd[0]);  // ~ 监听 sig_pipefd[0]

    addsig(SIGCHLD, sig_handler);
    addsig(SIGTERM, sig_handler);
    addsig(SIGINT, sig_handler);
    addsig(SIGPIPE, SIG_IGN);
    bool stop_server = false;
    bool terminate = false;

    // shmfd = shm_open(shm_name, O_CREAT | O_RDWR, 0666); // ~ 创建共享内存
    // assert(shmfd != -1);
    // ret = ftruncate(shmfd, USER_LIMIT * BUFFER_SIZE);  // ~ 修改文件大小
    // assert(ret != -1);
    // // ~ 申请一段内存空间，可以作为进程间通信的共享内存
    // share_mem = (char*)mmap(NULL, USER_LIMIT * BUFFER_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shmfd, 0);
    // assert(share_mem != MAP_FAILED);
    // close(shmfd);

    key = ftok("/a.c", 'c');
    shmid = shmget(key, USER_LIMIT * BUFFER_SIZE, O_CREAT | O_RDWR | 0666);
    share_mem = (char *)shmat(shmid, NULL, 0);

    while (!stop_server) {
        int number = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);
        if ((number < 0) && (errno != EINTR)) {
            std::cout << "epoll failure" << std::endl;
            break;
        }
        for (int i = 0; i < number; i++) {
            int sockfd = events[i].data.fd;
            if (sockfd == listenfd) {  // ~ 监听连接时间
                struct sockaddr_in client_address;
                socklen_t client_addrlength = sizeof(client_address);
                int connfd = accept(listenfd, (struct sockaddr*)&client_address, &client_addrlength);
                if (connfd < 0) {
                    std::cout << "errno is : " << std::endl;
                    continue;
                }
                if (user_count >= USER_LIMIT) {
                    const char* info = "too many users\n";
                    std::cout << info << std::endl;
                    send(connfd, info, strlen(info), 0);
                    close(connfd);
                    continue;
                }
                users[user_count].address = client_address;
                users[user_count].connfd = connfd;
                // ~ 创建父子进程之间通信的管道
                ret = socketpair(PF_UNIX, SOCK_STREAM, 0, users[user_count].pipefd);
                assert(ret != -1);
                pid_t pid = fork();
                if (pid < 0) {
                    close(connfd);
                    continue;
                } else if (pid == 0) { // ~ 子进程
                    // ~ 关闭 子进程 epoll, listenfd
                    // ~ 关闭客户端 pipefd[0], 信号管道
                    close(epollfd);
                    close(listenfd);
                    close(users[user_count].pipefd[0]);
                    close(sig_pipefd[0]);
                    close(sig_pipefd[1]);
                    run_child(user_count, users, share_mem);
                    // munmap((void*)share_mem, USER_LIMIT * BUFFER_SIZE);
                    shmdt(share_mem);
                    exit(0);
                } else { // ~ 父进程
                    close(connfd);
                    close(users[user_count].pipefd[1]);
                    // ~ 父进程用来监听子进程通知自己处理数据
                    addfd(epollfd, users[user_count].pipefd[0]);
                    users[user_count].pid = pid;
                    sub_process[pid] = user_count;
                    user_count++;
                }
            } else if ((sockfd == sig_pipefd[0]) && (events[i].events & EPOLLIN)) { // ~ 处理信号
                int sig;
                char signals[1024];
                ret = recv(sig_pipefd[0], signals, sizeof(signals), 0);
                if (ret == -1) continue;
                else if (ret == 0) continue;
                else {
                    for (int i = 0; i < ret; ++i) {
                        switch(signals[i]) {
                            case SIGCHLD: // ~ 子进程退出
                            {
                                pid_t pid;
                                int stat;
                                while ((pid = waitpid(-1, &stat, WNOHANG)) > 0) {
                                    int del_user = sub_process[pid];  // ~ 找到退出的客户
                                    sub_process[pid] = -1; // ~ 重置索引表格
                                    if ((del_user < 0) || (del_user > USER_LIMIT)) {
                                        continue;
                                    }
                                    // ~ 从epollfd 中删除，客户的父子进程通信管道注册信息
                                    epoll_ctl(epollfd, EPOLL_CTL_DEL, users[del_user].pipefd[0], 0);
                                    close(users[del_user].pipefd[0]); // ~ 关闭
                                    // ~ 更新 users 数组（一直保持，新来的客户端位于 users 数组后部分）
                                    users[del_user] = users[--user_count];
                                    sub_process[users[del_user].pid] = del_user;
                                }
                                if (terminate && user_count == 0) stop_server = true;
                                break;
                            }
                            case SIGTERM:
                            case SIGINT: {
                                std::cout << "kill all the child now " << std::endl;
                                if (user_count == 0) {
                                    stop_server = true;
                                    break;
                                }
                                for (int i = 0; i < user_count; ++i) {
                                    int pid = users[i].pid;
                                    kill(pid, SIGTERM);
                                }
                                terminate = true;
                                break;
                            }
                            default: break;
                        }
                    }
                }
            }
            // ~ 某个子进程向父进程写入数据 
            else if (events[i].events & EPOLLIN) {
                int child = 0; 
                ret = recv(sockfd, (char*)&child, sizeof(child), 0);
                std::cout << "read data from child accross pipe" << std::endl;
                if (ret == -1) continue;
                else if (ret == 0) continue;
                else {
                    // ~ 向除负责处理第 child 个客户连接的子进程之外的其他子进程发送消息
                    for (int j = 0; j < user_count; ++j) {
                        if (users[j].pipefd[0] != sockfd) {
                            std::cout << "send data to child: " << child << " , accross pipe" <<  std::endl;
                            send(users[j].pipefd[0], (char*)&child, sizeof(child), 0);
                        }
                    }
                }
            }
        }
    }
    shmctl(shmid, IPC_RMID, NULL);
    del_resource();
    return 0;
}
