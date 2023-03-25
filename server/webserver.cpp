#include "webserver.h"
using namespace std;

// 服务器构造函数
WebServer::WebServer(
        int port, int trigMode, int timeoutMS, bool OptLinger,
        int sqlPort, const char* sqlUser, const  char* sqlPwd,
        const char* dbName, int connPoolNum, int threadNum,
        bool openLog, int logLevel, int logQueSize):
    port_(port), openLinger_(OptLinger), timeoutMS_(timeoutMS), isClose_(false),
    timer_(new HeapTimer()), threadpool_(new ThreadPool(threadNum)), epoller_(new Epoller()) {
        srcDir_ = getcwd(nullptr, 256);                 // 获取当前工作路径 
        assert(srcDir_);                                // 断言
        strncat(srcDir_, "/resources/", 16);            // 资源路径 
        HttpConn::userCount = 0;                        // 用户数
        HttpConn::srcDir = srcDir_;                     // 资源路径赋值
        SqlConnPool::Instance()->Init("localhost", sqlPort, sqlUser, sqlPwd, dbName, connPoolNum);  // 初始化连接池

        InitEventMode_(trigMode);                          // 初始化事件模式
        if(!InitSocket_()) { isClose_ = true;}             // 初始化套接子，如果没有成功，关闭服务器

        // 日志相关
        if(openLog) {
            Log::Instance()->init(logLevel, "./log", ".log", logQueSize);
            if(isClose_) { LOG_ERROR("========== Server init error!=========="); }
            else {
                LOG_INFO("========== Server init ==========");
                LOG_INFO("Port:%d, OpenLinger: %s", port_, OptLinger? "true":"false");
                LOG_INFO("Listen Mode: %s, OpenConn Mode: %s",
                                (listenEvent_ & EPOLLET ? "ET": "LT"),
                                (connEvent_ & EPOLLET ? "ET": "LT"));
                LOG_INFO("LogSys level: %d", logLevel);
                LOG_INFO("srcDir: %s", HttpConn::srcDir);
                LOG_INFO("SqlConnPool num: %d, ThreadPool num: %d", connPoolNum, threadNum);
            }
        }
    }

// 服务器析构函数
WebServer::~WebServer() {
    close(listenFd_);
    isClose_ = true;
    free(srcDir_);
    SqlConnPool::Instance()->ClosePool();
}

/* 设置监听的文件描述符和通信的文件描述法符的模式 */
void WebServer::InitEventMode_(int trigMode) {
    listenEvent_ = EPOLLRDHUP;                  // 监听事件，检测读关闭
    connEvent_ = EPOLLONESHOT | EPOLLRDHUP;     // 连接事件，
    switch (trigMode)
    {
    case 0: 
        break;
    case 1:
        connEvent_ |= EPOLLET;                  // 加上 ET 模式，默认 LT 模式
        break;
    case 2:
        listenEvent_ |= EPOLLET;                // 其他的情况
        break;
    case 3:
        listenEvent_ |= EPOLLET;
        connEvent_ |= EPOLLET;
        break;
    default:
        listenEvent_ |= EPOLLET;
        connEvent_ |= EPOLLET;
        break;
    }
    HttpConn::isET = (connEvent_ & EPOLLET);    // 是否是 ET 模式
}

// 服务器开始启动
void WebServer::Start() {                       //? (1) 服务器在这里启动
    int timeMS = -1;                            /* epoll wait timeout == -1 无事件将阻塞 */
    if(!isClose_) { LOG_INFO("========== Server start =========="); }
    while(!isClose_) {                          // 如果开启
        if(timeoutMS_ > 0) {                    
            timeMS = timer_->GetNextTick();     // 解决超时链接
        }
        int eventCnt = epoller_->Wait(timeMS);  // 检测有多少个     //? (2) 然后不断地监听事件到达
        for(int i = 0; i < eventCnt; i++) {
            /* 处理事件 */
            int fd = epoller_->GetEventFd(i);           // 获取fd
            uint32_t events = epoller_->GetEvents(i);   // 获取事件列表
            if(fd == listenFd_) {                                   //? (3) 如果监听到事件，并且为连接事件，那么就连接新客户端
                DealListen_();                      // 处理监听事件，接收客户端连接
            }
            else if(events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
                assert(users_.count(fd) > 0);
                CloseConn_(&users_[fd]);            // 出错关闭连接
            }
            else if(events & EPOLLIN) {
                assert(users_.count(fd) > 0);
                DealRead_(&users_[fd]);             // 处理读操作   //? (5) 如果监听到事件，并且为读事件，那么读任务添加到线程池里面
            }
            else if(events & EPOLLOUT) {
                assert(users_.count(fd) > 0);
                DealWrite_(&users_[fd]);            // 处理写操作   //? (10） 写完修改fd为写后继续监听，如果监听到事件，并且为写事件，那么写任务添加到线程池里
            }
        }
    }
}

void WebServer::SendError_(int fd, const char*info) {
    assert(fd > 0);
    int ret = send(fd, info, strlen(info), 0);
    if(ret < 0) {
        LOG_WARN("send error to client[%d] error!", fd);
    }
    close(fd);
}

// 关闭客户端连接
void WebServer::CloseConn_(HttpConn* client) {
    assert(client);
    LOG_INFO("Client[%d] quit!", client->GetFd());
    epoller_->DelFd(client->GetFd());   // 删除 fd
    client->Close();                    // 关闭
}

// 添加客户端连接
void WebServer::AddClient_(int fd, sockaddr_in addr) {
    assert(fd > 0);
    users_[fd].init(fd, addr);              // 保存用户信息<fd，对应信息>
    if(timeoutMS_ > 0) {
        timer_->add(fd, timeoutMS_, std::bind(&WebServer::CloseConn_, this, &users_[fd]));
    }
    epoller_->AddFd(fd, EPOLLIN | connEvent_);      // 新连接的 fd 加入到 epoll，读事件
    SetFdNonblock(fd);                              // 非阻塞
    LOG_INFO("Client[%d] in!", users_[fd].GetFd());
}

// 处理监听事件
void WebServer::DealListen_() {
    struct sockaddr_in addr;                            // 保存连接客户端的信息
    socklen_t len = sizeof(addr);                      
    do {
        int fd = accept(listenFd_, (struct sockaddr *)&addr, &len); // 接收连接
        if(fd <= 0) { return;}
        else if(HttpConn::userCount >= MAX_FD) {                    // 连接数量太多
            SendError_(fd, "Server busy!");                         // 发送错误信息
            LOG_WARN("Clients is full!");   
            return;
        }
        AddClient_(fd, addr);                                       // 添加客户端       //? (4) 连接上新客户端后，记得把新的 fd 添加到 epoll 里面
    } while(listenEvent_ & EPOLLET);            // 判断监听事件是否为 ET，那么需要一次性把数据都读完，一直 do
}

// 处理读操作，将读任务添加到线程池
void WebServer::DealRead_(HttpConn* client) {
    assert(client);
    ExtentTime_(client);
    threadpool_->AddTask(std::bind(&WebServer::OnRead_, this, client)); // 将读任务添加到线程池 //? (6) 池子里添加的有任务，就让条件变量通知工作线程，让他们干活
}

// 子线程中处理写操作
void WebServer::DealWrite_(HttpConn* client) {
    assert(client);
    ExtentTime_(client);
    threadpool_->AddTask(std::bind(&WebServer::OnWrite_, this, client)); // 将写任务添加到线程池 //? (11) 池子里添加的有任务，就让条件变量通知工作线程，让他们干活
}

void WebServer::ExtentTime_(HttpConn* client) {
    assert(client);
    if(timeoutMS_ > 0) { timer_->adjust(client->GetFd(), timeoutMS_); }
}

// 子线程中处理读操作
void WebServer::OnRead_(HttpConn* client) {                 //? (7） 工作线程进行读操作
    assert(client);
    int ret = -1;
    int readErrno = 0;
    ret = client->read(&readErrno);         // 读取客户端的数据，读到 readbuffer 缓冲区
    if(ret <= 0 && readErrno != EAGAIN) {   // 出现错误，关闭客户端
        CloseConn_(client);
        return;
    }
    OnProcess(client);                      // 业务逻辑的处理 //? (8） 读完后工作线程进行业务处理，解析HTTP请求，产生响应
}

// 处理业务逻辑，客户端来处理
void WebServer::OnProcess(HttpConn* client) {
    if(client->process()) {
        epoller_->ModFd(client->GetFd(), connEvent_ | EPOLLOUT);    //? (9） 解析完HTTP获得响应后，将之前的读事件改成写事件。
    } else {
        epoller_->ModFd(client->GetFd(), connEvent_ | EPOLLIN);     // 监听是否可读
    }
}

// 子线程中处理读操作
void WebServer::OnWrite_(HttpConn* client) {
    assert(client);
    int ret = -1;
    int writeErrno = 0;
    ret = client->write(&writeErrno);
    if(client->ToWriteBytes() == 0) {
        /* 传输完成 */
        if(client->IsKeepAlive()) {
            OnProcess(client);
            return;
        }
    }
    else if(ret < 0) {
        if(writeErrno == EAGAIN) {
            /* 继续传输 */
            epoller_->ModFd(client->GetFd(), connEvent_ | EPOLLOUT);
            return;
        }
    }
    CloseConn_(client);
}

/* Create listenFd */
bool WebServer::InitSocket_() {
    int ret;
    struct sockaddr_in addr;                    // 套接字地址
    if(port_ > 65535 || port_ < 1024) {         // 端口号要正确
        LOG_ERROR("Port:%d error!",  port_);
        return false;
    }
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);   // 字节序转换
    addr.sin_port = htons(port_);               // 端口号转换
    struct linger optLinger = { 0 };
    if(openLinger_) {
        /* 优雅关闭: 直到所剩数据发送完毕或超时 */
        optLinger.l_onoff = 1;
        optLinger.l_linger = 1;
    }

    listenFd_ = socket(AF_INET, SOCK_STREAM, 0);    // 创建文件描述符的socket
    if(listenFd_ < 0) {
        LOG_ERROR("Create socket error!", port_);
        return false;
    }
 
    ret = setsockopt(listenFd_, SOL_SOCKET, SO_LINGER, &optLinger, sizeof(optLinger));
    if(ret < 0) {
        close(listenFd_);
        LOG_ERROR("Init linger error!", port_);
        return false;
    }

    int optval = 1;
    /* 端口复用 */
    /* 只有最后一个套接字会正常接收数据。 */
    ret = setsockopt(listenFd_, SOL_SOCKET, SO_REUSEADDR, (const void*)&optval, sizeof(int));
    if(ret == -1) {
        LOG_ERROR("set socket setsockopt error !");
        close(listenFd_);
        return false;
    }

    ret = bind(listenFd_, (struct sockaddr *)&addr, sizeof(addr));  // 绑定IP,端口
    if(ret < 0) {
        LOG_ERROR("Bind Port:%d error!", port_);
        close(listenFd_);
        return false;
    }

    ret = listen(listenFd_, 6);     // 监听ing
    if(ret < 0) {
        LOG_ERROR("Listen port:%d error!", port_);
        close(listenFd_);
        return false;
    }
    ret = epoller_->AddFd(listenFd_,  listenEvent_ | EPOLLIN);  // ET模式，添加fd
    if(ret == 0) {
        LOG_ERROR("Add listen error!");
        close(listenFd_);
        return false;
    }
    SetFdNonblock(listenFd_);           // 设置为非阻塞
    LOG_INFO("Server port:%d", port_);
    return true;
}

// 设置 fd 非阻塞
int WebServer::SetFdNonblock(int fd) {      // 获取原先的值
    assert(fd > 0);
    return fcntl(fd, F_SETFL, fcntl(fd, F_GETFD, 0) | O_NONBLOCK);  // 设置一下
}


