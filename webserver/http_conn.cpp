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

    init();
}

// 初始化解析 HTTP 报文所需要的变量
// 这里分开来写是因为这个初始化的函数会被调用多次    
void http_conn::init() {
    m_check_state = CHECK_STATE_REQUESTLINE;  // 初始化状态为解析请求首行
    m_checked_index = 0;
    m_start_line = 0;
    m_read_idx = 0;

    m_method = GET;
    m_url = 0;
    m_version = 0;
    m_linger = false;

    bzero(m_read_buf, READ_BUFFER_SIZE); // 清空读缓冲区的数据 
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

// 解析 HTTP 请求
http_conn::HTTP_CODE http_conn::process_read() {
    LINE_STATUS line_status = LINE_OK;
    HTTP_CODE ret = NO_REQUEST;

    char *text = 0;

    while (((m_check_state == CHECK_STATE_CONTENT) && (line_status == LINE_OK)) 
        || (line_status = parse_line()) == LINE_OK) {
            // 解析到了完整的一行, 或者解析到了请求主体 (同时也是完整数据)

            // 获取一行数据
        text = get_line();
        printf("receive 1 http line: %s\n", text);

        switch (m_check_state) {
            case CHECK_STATE_REQUESTLINE: {
                ret = parse_request_line(text);
                if (ret == BAD_REQUEST) {
                  return BAD_REQUEST;
                }
                break;
            }

            case CHECK_STATE_HEADER: {
                ret = parse_headers(text);
                if (ret == BAD_REQUEST) {
                  return BAD_REQUEST;
                } else if (ret == GET_REQUEST) {
                  // 成功扫描完成请求头部的数据
                  return do_request();
                }
            }

            case CHECK_STATE_CONTENT: {
                ret = parse_content(text);
                if (ret == GET_REQUEST) {
                  return do_request();
                }

                line_status = LINE_OPEN;
                break;
            }

            default: {
                return INTERNAL_ERROR;
            }
        }
    }

    return NO_REQUEST;
}

// 先从缓冲区中提取一行出来, 然后交给解析函数解析
http_conn::LINE_STATUS http_conn::parse_line() {
    char temp;

    for (; m_checked_index < m_read_idx; ++m_checked_index) {
        temp = m_read_buf[m_checked_index];
        if (temp == '\r') { // 扫描到了倒数第二位
            if (m_checked_index + 1 == m_read_idx) { // 没有扫描到完整的行
                return LINE_OPEN;
            } else if (m_read_buf[m_checked_index + 1] == '\n') { // 扫描到了完整的行
                m_read_buf[m_checked_index++] = '\0'; // 设置结束的位置
                m_read_buf[m_checked_index++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        } else if (temp == '\n') {
            // 出现这种情况: “\nHost: ...\r\n"
            if (m_checked_index > 1 && m_read_buf[m_checked_index - 1] == '\r') {
                // 把 “\r\n" 换成 “\0\0"
                m_read_buf[m_checked_index - 1] = '\0'; 
                m_read_buf[m_checked_index++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
    }
    return LINE_OPEN;

    /*
        解析这里没看懂的话去看游双的书第 9 章
    */
}

// 解析 HTTP 请求首行, 获得请求方法, 目标 URL, HTTP 版本
http_conn::HTTP_CODE http_conn::parse_request_line(char* text) {
    // GET /index.html HTTP/1.1
    // The function returns a pointer to the first occurence of any character from the second arg
    m_url = strpbrk(text, " \t");

    // GET\0/index.html HTTP/1.1
    *m_url++ = '\0';

    char *method = text; 
    if (strcasecmp(method, "GET") == 0) {
        m_method = GET;
    } else {
        return BAD_REQUEST;
    }

    // /index.html HTTP/1.1
    m_version = strpbrk(m_url, " \t");
    if (!m_version) {
        return BAD_REQUEST;
    }
    // /index.html\0HTTP/1.1
    *m_version++ = '\0';
    if (strcasecmp(m_version, "HTTP/1,1") != 0) {
        return BAD_REQUEST;
    }

    // http://192.168.1.1:10000/index.html
    if (strcasecmp(m_url, "http://", 7) == 0) {
        m_url += 7; // 跳过前面的http://
        m_url = strchr(m_url, '/');  // /index.html
    }

    if (!m_url || m_url[0] != '/') {
        return BAD_REQUEST;
    }

    m_check_state = CHECK_STATE_HEADER; // 主状态机检查状态: 检查请求头
    return NO_REQUEST;
}

// 解析请求头
http_conn::HTTP_CODE http_conn::parse_headers(char* text) {

}

// 解析请求体
http_conn::HTTP_CODE http_conn::parse_content(char* text) {

}

http_conn::HTTP_CODE http_conn::do_request() {

}

bool http_conn::write() {
    printf("write data all at once...\n");
    return true;
}

http_conn::http_conn() {

}

http_conn::~http_conn() {

}

// 由线程池的工作线程调用, 这是处理 HTTP 请求的入口函数
void http_conn::process() {
    // 解析 HTTP 请求

    // 检测处理完这个业务逻辑之后返回来的状态
    HTTP_CODE read_ret = process_read(); 
    if (read_ret == NO_REQUEST) { //  请求不完整, 客户端还需要继续读取数据
        // 这个时候要把 EPOLLONESHOT 重新加回来
        modfd(e_epollfd, m_sockfd, EPOLLIN);
        return;
    }

    printf("parse request, create response\n");
    // 生成响应 (将数据放入响应报文中)
}
