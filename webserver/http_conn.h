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

    http_conn();
    ~http_conn();

    void process(); // 解析请求报文, 并且处理客户端请求, 最后封装客户端响应
    void init(int sockfd, const sockaddr_in& addr); // 初始化新的连接

private:
    int m_sockfd;          // 该 HTTP 连接的 socket 
    sockaddr_in m_address; // 通信的 socket 地址

};

#endif