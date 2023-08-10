#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <bits/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <libgen.h>
// #include <sys/epoll.h>

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
    Threadpool<http_conn> *pool = nullptr;
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
    address.sin_addr = INADDR_ANY;
    address.sin_port = htons(port);
    int ret = bind(listen_fd, (struct sockaddr *)address, sizeof(address));
    if (ret == -1) {
      perror("Bind");
      exit(-1);
    }

    // 监听
    listen(listen_fd, 5);

    // 创建 epoll 对象, 事件数组, 添加文件描述符

    return 0;
}
