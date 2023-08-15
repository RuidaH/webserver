#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <libgen.h>
#include <sys/epoll.h>

#include "locker.h"
#include "threadpool.h"
#include "http_conn.h"

#define MAX_FD 65535 // 最大文件描述符个数
#define MAX_EVENT_NUMBER 10000 // 最大监听的事件对象

// 添加信号捕捉
void addsig(int sig, void(handler)(int)) { 
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = handler;
    sigfillset(&sa.sa_mask);
    sigaction(sig, &sa, NULL);
}

// 添加文件描述符到 epoll 中
extern void addfd(int epollfd, int fd, bool one_shot);
// 从 epoll 中删除文件描述符
extern void removefd(int epollfd, int fd);
// 修改文件描述符
extern void modfd(int epollfd, int fd, int ev);

// argv[0]: 程序名字
int main(int argc, char* argv[]) {

    if (argc <= 1) {
      printf("please follow the format: %s port_number\n", basename(argv[0]));
      exit(-1);
    }

    int port = atoi(argv[1]);       // 获取端口号
    addsig(SIGPIPE, SIG_IGN); // 对于 SIGPIE 信号, 直接进行忽略

    // 创建并初始化线程池
    // 模拟 proactor 的模式, 主线程负责数据的读写, 然后让子线程负责业务逻辑 (被封装成任务类)
    Threadpool<http_conn> *pool = NULL;
    try {
       pool = new Threadpool<http_conn>;
    } catch(...) {
       exit(-1);
    }

    // 创建一个数组用于保存所有的客户端信息
    http_conn *users = new http_conn[MAX_FD];

    int listen_fd = socket(PF_INET, SOCK_STREAM, 0);
    if (listen_fd == -1) {
      perror("Socket");
      exit(-1);
    }

    // 设置端口复用 (要在绑定之前去设置)
    int reuse = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    // 绑定
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);
    int ret = bind(listen_fd, (struct sockaddr *)&address, sizeof(address));
    if (ret == -1) {
      perror("Bind");
      exit(-1);
    }

    // 监听
    listen(listen_fd, 5);

    // 创建 epoll 对象, 事件数组, 添加文件描述符
    epoll_event events[MAX_EVENT_NUMBER];
    int epollfd = epoll_create(5);

    // 将监听的文件描述符添加到 epoll 对象中
    addfd(epollfd, listen_fd, false);
    http_conn::m_epollfd = epollfd;

    while (true) {
      int num = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);
      if ((num < 0) && (errno != EINTR)) { // 产生信号中断
        printf("Epoll Failure...\n");
        break;
      }

      // 循环遍历事件数组
      for (int i =0; i < num; ++i) {
        int sockfd = events[i].data.fd;
        if (sockfd == listen_fd) {

          // 有客户端连接进来
          struct sockaddr_in client_address;
          socklen_t client_addrlen = sizeof(client_address);
          int conn_fd = accept(listen_fd, (struct sockaddr*)&client_address, &client_addrlen);

          if (http_conn::m_user_count >= MAX_FD) {
            // 目前连接数满了
            // 给客户端写一个信息：服务器正忙（之后写）
            close(conn_fd);
            continue;
          }

          // 给新的客户端初始化，放到数组中
          users[conn_fd].init(conn_fd, client_address);

        } else if (events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {

          // 队伍异常断开或者错误等事件, 需要关闭连接
          users[sockfd].close_conn();

        } else if (events[i].events & EPOLLIN) {

          // 读事件: 一次性把所有的事件都读出来
          if (users[sockfd].read()) {
            // 把业务逻辑交给线程池中的线程去执行
            pool->append(users + sockfd);
          } else { // 读取失败
            users[sockfd].close_conn();
          }

        } else if (events[i].events & EPOLLOUT) {

          // 一次性写完所有的数据
          if (!users[sockfd].write()) {
            users[sockfd].close_conn();
          }

        }

      }

    }
    close(epollfd);
    close(listen_fd);
    delete[] users;
    delete pool;

    return 0;
}
