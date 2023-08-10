#ifndef HTTPCONNECTION_H
#define HTTPCONNECTION_H

class http_conn {
public:
 http_conn();
 ~http_conn();

 void process(); // 解析请求报文, 并且处理客户端请求, 最后封装客户端响应

private:

};

#endif