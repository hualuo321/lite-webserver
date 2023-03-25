#ifndef HTTP_CONN_H
#define HTTP_CONN_H
#include <sys/types.h>
#include <sys/uio.h>     // readv/writev
#include <arpa/inet.h>   // sockaddr_in
#include <stdlib.h>      // atoi()
#include <errno.h>      
#include "../log/log.h"
#include "../pool/sqlconnRAII.h"
#include "../buffer/buffer.h"
#include "httprequest.h"
#include "httpresponse.h"


/* 用户连接信息 */
//! 客户端主要包含：fd，地址信息，是否关闭，读写缓冲区 等属性
//! 客户端主要包含：初始化，读写数据，获取fd、地址信息，端口 等操作
class HttpConn {
public:
    HttpConn();

    ~HttpConn();

    void init(int sockFd, const sockaddr_in& addr);

    ssize_t read(int* saveErrno);

    ssize_t write(int* saveErrno);

    void Close();

    int GetFd() const;

    int GetPort() const;

    const char* GetIP() const;
    
    sockaddr_in GetAddr() const;
    
    bool process();

    int ToWriteBytes() { 
        return iov_[0].iov_len + iov_[1].iov_len; 
    }

    bool IsKeepAlive() const {
        return request_.IsKeepAlive();
    }

    // 静态变量被共享
    static bool isET;                       // 边缘触发
    static const char* srcDir;              // 资源目录
    static std::atomic<int> userCount;      // 当前客户端连接数

private:
    int fd_;                                // fd
    struct  sockaddr_in addr_;              // 连接信息
    bool isClose_;                          // 是否关闭

    int iovCnt_;
    struct iovec iov_[2];
    
    Buffer readBuff_; // 读缓冲区，保存请求数据的内存
    Buffer writeBuff_; // 写缓冲区，保存响应数据的内存

    HttpRequest request_;
    HttpResponse response_;
};

#endif //HTTP_CONN_H