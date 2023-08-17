#include "http_conn.h"

int http_conn::m_epollfd = -1;
int http_conn::m_user_count = 0;

// 设置文件描述符非阻塞
void setnonblocking(int fd) {
    int old_flag = fcntl(fd, F_GETFL);
    int new_flag = old_flag | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_flag);
}

// 添加文件描述符到 epoll 中
void addfd(int epollfd, int fd, bool one_shot) {
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLRDHUP;

    // 使用 one shot 之后， 每次事件被触发都需要重新注册
    if (one_shot) {
        event.events | EPOLLONESHOT;
    }
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);

    // 设置文件描述符非阻塞
    setnonblocking(fd);
}

// 从 epoll 中删除文件描述符
void removefd(int epollfd, int fd) {
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
    close(fd);
}

// 修改文件描述符, 重置 socket 上 EPOLLONESHOT 事件， 以确保下一次可读时， EPOLLIN 事件能被触发
void modfd(int epollfd, int fd, int ev) {
    epoll_event event;
    event.data.fd = fd;
    // event.events = ev | EPOLLONESHOT | EPOLLRDHUP;
    event.events = ev | EPOLLONESHOT | EPOLLET | EPOLLRDHUP;
    epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
}

// 初始化连接
void http_conn::init(int sockfd, const sockaddr_in& addr) {
    m_sockfd = sockfd;
    m_address = addr;

    // 设置端口复用
    int reuse = 1;
    setsockopt(m_sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    // 添加到 epoll 对象中
    addfd(m_epollfd, sockfd, true);
    m_user_count++; // 总用户数+1
}

// 关闭连接
void http_conn::close_conn() {
    if (m_sockfd != -1) {
        removefd(m_epollfd, m_sockfd);
        m_sockfd = -1;
        m_user_count--; // 客户数量 - 1 
    }
}

bool http_conn::read() {
    // printf("read data all at once...\n");

    if (m_read_idx >= READ_BUFFER_SIZE) {
        return false;
    }

    // 读取到的字节
    int bytes_read = 0;
    while (true) {
        // 把 m_read_idx 之后的数据一次读取出来
        bytes_read = recv(m_sockfd, m_read_buf + m_read_idx, READ_BUFFER_SIZE - m_read_idx, 0);
        if (bytes_read == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // 没有数据
                break;
            }
            return false;
        } else if (bytes_read == 0) {
            // 对方关闭连接
            return false;
        }
        printf("Data read: %s\n", m_read_buf);
        m_read_idx += bytes_read;
    }
    return true;
}

bool http_conn::write() {
    printf("write data all at once...\n");
    return true;
}

http_conn::http_conn()
{
}

http_conn::~http_conn()
{
}

// 由线程池的工作线程调用, 这是处理 HTTP 请求的入口函数
void http_conn::process() {
    // 解析 HTTP 请求
    printf("parse request, create response\n");
    // 生成响应 (将数据放入响应报文中)
}
