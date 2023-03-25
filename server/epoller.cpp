#include "epoller.h"
// 构造函数，初始化 epoll
// 调用epoll_create创建epoll，初始化最大事件数
Epoller::Epoller(int maxEvent):epollFd_(epoll_create(512)), events_(maxEvent){
    assert(epollFd_ >= 0 && events_.size() > 0);        // 断言是否合规
}

// 析构函数，释放epoll
Epoller::~Epoller() {
    close(epollFd_);   
}

// 添加fd，返回是否添加成功
bool Epoller::AddFd(int fd, uint32_t events) {  // 添加 fd
    if(fd < 0) return false;
    epoll_event ev = {0};                       // 创建一个 epoll 事件
    ev.data.fd = fd;                            // 文件描述符
    ev.events = events;                         // 事件
    return 0 == epoll_ctl(epollFd_, EPOLL_CTL_ADD, fd, &ev);    // （树根，添加方式，fd，绑定的ev）
}

// 修改fd，返回是否修改成功
bool Epoller::ModFd(int fd, uint32_t events) {
    if(fd < 0) return false;
    epoll_event ev = {0};
    ev.data.fd = fd;
    ev.events = events;                         // 事件
    return 0 == epoll_ctl(epollFd_, EPOLL_CTL_MOD, fd, &ev);     // （树根，添加方式，fd，绑定的ev）
}

// 删除fd，都是调用epoll_ctl
bool Epoller::DelFd(int fd) {
    if(fd < 0) return false;
    epoll_event ev = {0};
    return 0 == epoll_ctl(epollFd_, EPOLL_CTL_DEL, fd, &ev);
}

// 等待事件发生，主要调用 epoll_wait
int Epoller::Wait(int timeoutMs) {
    return epoll_wait(epollFd_, &events_[0], static_cast<int>(events_.size()), timeoutMs);
}

// 获取事件列表中对应的fd
int Epoller::GetEventFd(size_t i) const {
    assert(i < events_.size() && i >= 0);
    return events_[i].data.fd;
}

// 获取事件列表中对应的事件
uint32_t Epoller::GetEvents(size_t i) const {
    assert(i < events_.size() && i >= 0);
    return events_[i].events;
}