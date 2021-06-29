#ifndef HTTPCONNECTION_H
#define HTTPCONNECTION_H

#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <sys/stat.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <errno.h>

#include "locker.h"

class http_conn {
public:
    static const int FILENAME_LEN = 200;
    static const int READ_BUFFER_SIZE = 2048;
    static const int WRITE_BUFFER_SIZE = 1024;
    enum METHOD { GET = 0, POST, HEAD, PUT, DELETE, TRACE, OPTIONS, CONNECT, PATCH };
    enum CHECK_STATE { CHECK_STATE_REQUESTLINE = 0, CHECK_STATE_HEADER, CHECK_STATE_CONTENT };
    enum HTTP_CODE { NO_REQUEST = 0, GET_REQUEST, BAD_REQUEST, NO_RESOURCE, FORBIDDEN_REQUEST, FILE_REQUEST,
                        INTERNAL_ERROR, CLOSED_CONNECTION };
    enum LINE_STATUS {LINE_OK = 0, LINE_BAD, LINE_OPEN };
public:
    void init(int sockfd, const sockaddr_in& addr);
    void close_conn(bool real_close = true);
    void process();
    bool read();
    bool write();
private:
    void init();  // ~ 初始化连接
    HTTP_CODE process_read();  // ~ 解析 HTTP 请求
    bool process_write(HTTP_CODE ret); // ~ 填充 HTTP 应答

    HTTP_CODE parse_request_line(char *text);
    HTTP_CODE parse_headers(char *text);
    HTTP_CODE parse_content(char *text);
    HTTP_CODE do_request();
    
    char *get_line() { return m_read_buf + m_start_line; }
    LINE_STATUS parse_line();

    void unmap();
    bool add_response(const char *format, ...);
    bool add_content(const char *content);
    bool add_status_line(int status, const char *title);
    bool add_headers(int content_length);
    bool add_content_length(int content_length);
    bool add_linger();
    bool add_blank_line();
public:
    static int m_epollfd;
    static int m_user_count;
private:
    int m_sockfd;
    struct sockaddr_in m_address;

    char m_read_buf[READ_BUFFER_SIZE];
    int m_read_idx;
    int m_checked_idx;
    int m_start_line;
    char m_write_buf[WRITE_BUFFER_SIZE];
    int m_write_idx;

    CHECK_STATE m_check_state;
    METHOD m_method;

    char m_real_file[FILENAME_LEN];
    char *m_url;
    char *m_version;
    char *m_host;
    int m_content_length;
    bool m_linger;

    char *m_file_address;
    struct stat m_file_stat;

    /* 需要包含的头文件  stat 函数用来获取指定路径的文件或者文件夹的信息
    #include <sys/types.h>    
    #include <sys/stat.h> 
    函数原型  
    int ret =  stat(
        const char *filename,    //文件或者文件夹的路径
　　    struct stat *buf      //获取的信息保存在内存中
    );  
    ret = 0 正确  ret = -1 错误, 错误码记录在 errno
    struct stat  
    {   
        dev_t       st_dev;     // ID of device containing file -文件所在设备的ID  
        ino_t       st_ino;     // inode number -inode节点号   
        mode_t      st_mode;    // protection -保护模式?   
        nlink_t     st_nlink;   // number of hard links -链向此文件的连接数(硬连接)   
        uid_t       st_uid;     // user ID of owner -user id    
        gid_t       st_gid;     // group ID of owner - group id    
        dev_t       st_rdev;    // device ID (if special file) -设备号，针对设备文件 
        off_t       st_size;    // total size, in bytes -文件大小，字节为单位   
        blksize_t   st_blksize; // blocksize for filesystem I/O -系统块的大小    
        blkcnt_t    st_blocks;  // number of blocks allocated -文件所占块数    
        time_t      st_atime;   // time of last access -最近存取时间  
        time_t      st_mtime;   // time of last modification -最近修改时间    
        time_t      st_ctime;   // time of last status change     
    }; */




    struct iovec m_iv[2];
    int m_iv_count;

};


#endif