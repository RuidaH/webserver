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
#include <string.h>
#include "locker.h"

class http_conn {
public:
    static int m_epollfd;     // 所有的 socket 上的事件都被注册到同一个 epollfd 中
    static int m_user_count;  // 统计用户的数量
    static const int READ_BUFFER_SIZE = 2048;  // 读缓冲区的大小
    static const int WRITE_BUFFER_SIZE = 2048; // 写缓冲区的大小

    // HTTP 请求方法, 现在只支持 GET
    enum METHOD {
      GET = 0,
      POST,
      HEAD,
      PUT,
      DELETE,
      TRACE,
      OPTIONS,
      CONNECT
    }

    /*
        解析客户端请求时, 主状态机的状态
        CHECK_STATE_REQUESTLINE: 当前正在分析请求行
        CHECK_STATE_HEADER: 当前正在分析头部字段
        CHECK_STATE_CONTENT: 当前正在分析请求体
    */ 
    enum CHECK_STATE {
    CHECK_STATE_REQUESTLINE = 0,
    CHECK_STATE_HEADER,
    CHECK_STATE_CONTENT
    }

   /*
        服务器处理 HTTP 请求的可能结果, 报文解析的结果
        NO_REQUEST: 请求不完整, 需要继续读取客户端数据
        GET_REQUEST: 表示获得了一个完整的客户请求
        BAD_REQUEST: 表示客户请求的语法错误
        NO_RESOURCE: 表示客户端没有资源
        FORBIDDEN_REQUEST: 表示客户对请求的资源没有足够的权限
        FILE_REQUEST: 文件请求, 获取文件成功
        INTERNAL_ERROR: 表示服务器内部错误
        CLOSE_CONNECTION: 表示客户端已经关闭连接了
   */
    enum HTTP_CODE {
    NO_REQUEST,
    GET_REQUEST,
    BAD_REQUEST,
    NO_RESOURCE,
    FORBIDDEN_REQUEST,
    FILE_REQUEST,
    INTERNAL_ERROR,
    CLOSE_CONNECTION
    }

    // 从状态机的三种可能状态, 即行的读取状态
    // 1. 读取到一个完整的行
    // 2. 行出错
    // 3. 行数据尚且不完整
    enum LINE_STATUS {
        LINE_OK = 0,
        LINE_BAD,
        LINE_OPEN
    }
    
    http_conn();
    ~http_conn(); 

    void process(); // 解析请求报文, 并且处理客户端请求, 最后封装客户端响应
    void init(int sockfd, const sockaddr_in& addr); // 初始化新的连接
    void close_conn(); // 关闭连接
    bool read();  // 非阻塞读 (因为你需要把所有的数据都读出来)
    bool write(); // 非阻塞写    

private:
    int m_sockfd;                       // 该 HTTP 连接的 socket
    sockaddr_in m_address;              // 通信的 socket 地址

    char m_read_buf[READ_BUFFER_SIZE];  // 读缓冲区
    int m_read_idx;                     // 标识读缓冲区中读入的客户数据的最后一个字节的下一位
    int m_checked_index;                // 当前正在分析的字符在读缓冲区的位置
    int m_start_line;                   // 当前正在解析的行的起始位置
    CHECK_STATE m_check_state;          // 主状态机当前所处的状态

    //  将获取到的 HTTP 报头的信息存在这里
    char* m_url;     // 请求目标文件的文件名
    char* m_version; // 协议版本, 只支持 HTTP 1.1
    METHOD m_method; // 请求方法
    char *m_host;    // 主机名
    bool m_linger;   // HTTP 请求是否要保持连接

    void init();                        // 初始化连接其余的信息
    HTTP_CODE process_read();           // 解析 HTTP 请求
    LINE_STATUS parse_line();           // 先从缓冲区中提取一行出来, 然后交给下面的函数解析
    HTTP_CODE parse_request_line();     // 解析请求首行
    HTTP_CODE parse_headers();          // 解析请求头
    HTTP_CODE parse_content();          // 解析请求体
    char *get_line() { return m_read_buf + m_start_line; };
    HTTP_CODE do_request();
};

#endif