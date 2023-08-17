#ifndef HTTPCONNECTION_H
#define HTTPCONNECTION_H

#include <sys/epoll.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <errno.h>
#include <sys/uio.h>
#include "locker.h"

class http_conn {
public:
    static int m_epollfd;     // 所有的 socket 上的事件都被注册到同一个 epollfd 中
    static int m_user_count;  // 统计用户的数量
    static const int READ_BUFFER_SIZE = 2048;  // 读缓冲区的大小
    static const int WRITE_BUFFER_SIZE = 2048; // 写缓冲区的大小

    http_conn();
    ~http_conn(); 

    void process(); // 解析请求报文, 并且处理客户端请求, 最后封装客户端响应
    void init(int sockfd, const sockaddr_in& addr); // 初始化新的连接
    void close_conn(); // 关闭连接
    bool read();  // 非阻塞读 (因为你需要把所有的数据都读出来)
    bool write(); // 非阻塞写 

   private:
    int m_sockfd;          // 该 HTTP 连接的 socket
    sockaddr_in m_address; // 通信的 socket 地址
    char m_read_buf[READ_BUFFER_SIZE]; // 读缓冲区
    int m_read_idx; // 标识读缓冲区中读入的客户数据的最后一个字节的下一位
};

#endif