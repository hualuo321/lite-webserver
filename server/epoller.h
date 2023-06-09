#ifndef EPOLLER_H
#define EPOLLER_H

#include <sys/epoll.h> //epoll_ctl()
#include <fcntl.h>  // fcntl()
#include <unistd.h> // close()
#include <assert.h> // close()
#include <vector>
#include <errno.h>

class Epoller {
public:
    explicit Epoller(int maxEvent = 1024);  // 最大检测的事件数

    ~Epoller();
    // 从epoll中添加fd
    bool AddFd(int fd, uint32_t events);
    // 修改fd
    bool ModFd(int fd, uint32_t events);
    // 删除fd
    bool DelFd(int fd);
    // 调用内核帮我们检测
    int Wait(int timeoutMs = -1);

    int GetEventFd(size_t i) const;

    uint32_t GetEvents(size_t i) const;
        
private:
    int epollFd_;                               // epoll_create()创建一个epoll对象，返回一个文件描述符 fd

    std::vector<struct epoll_event> events_;    // 监测到的事件集合
};

#endif //EPOLLER_H