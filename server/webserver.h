/*
 * @Author       : mark
 * @Date         : 2020-06-17
 * @copyleft Apache 2.0
 */ 
#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <unordered_map>
#include <fcntl.h>       // fcntl()
#include <unistd.h>      // close()
#include <assert.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "epoller.h"
#include "../log/log.h"
#include "../timer/heaptimer.h"
#include "../pool/sqlconnpool.h"
#include "../pool/threadpool.h"
#include "../pool/sqlconnRAII.h"
#include "../http/httpconn.h"

class WebServer {
public:
    WebServer (                                                 // 构造函数声明
        int port, int trigMode, int timeoutMS, bool OptLinger,  // 端口，触发模式，超时时间，优雅关闭
        int sqlPort, const char *sqlUser, const char *sqlPwd,   // sql端口，sql用户名，sql密码
        const char *dbName, int connPoolNum, int threadNum,     // 数据库名，连接池数量，线程数量
        bool openLog, int logLevel, int logQueSize              // 开启日志，日志等级，日志队列大小
    );

    ~WebServer();                                               // 析构函数声明

    void Start();                                               // 服务启动

private:
    bool InitSocket_(); 
    void InitEventMode_(int trigMode);
    void AddClient_(int fd, sockaddr_in addr);

    void DealListen_();
    void DealWrite_(HttpConn* client);
    void DealRead_(HttpConn* client);

    void SendError_(int fd, const char*info);
    void ExtentTime_(HttpConn* client);
    void CloseConn_(HttpConn* client);

    void OnRead_(HttpConn* client);
    void OnWrite_(HttpConn* client);
    void OnProcess(HttpConn* client);

    static const int MAX_FD = 65536;            // 最大文件描述符个数
    static int SetFdNonblock(int fd);           // 设置文件描述符非阻塞

    int port_;              // 端口
    bool openLinger_;       // 优雅关闭
    int timeoutMS_;         // 超时时间
    bool isClose_;          // 是否关闭
    int listenFd_;          // 监听文件描述副
    char* srcDir_;          // 资源的目录
    
    uint32_t listenEvent_;  // 监听的文件描述符的事件
    uint32_t connEvent_;    // 连接的文件描述符的事件

    std::unique_ptr<HeapTimer> timer_;          // 定时器
    std::unique_ptr<ThreadPool> threadpool_;    // 线程池
    std::unique_ptr<Epoller> epoller_;          // epoll 对象
    std::unordered_map<int, HttpConn> users_;   // 客户端连接的连接信息<fd, 连接信息>
};


#endif //WEBSERVER_H