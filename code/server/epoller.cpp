#include "epoller.h"
// epoll_create：创建epoll对象
Epoller::Epoller(int maxEvent):epollFd_(epoll_create(512)), events_(maxEvent){  // 构造函数，创建一个epoll，返回文件描述符，设置event叔祖大小
    assert(epollFd_ >= 0 && events_.size() > 0);
}

Epoller::~Epoller() {
    close(epollFd_);
}

bool Epoller::AddFd(int fd, uint32_t events) {  // 添加 fd
    if(fd < 0) return false;
    epoll_event ev = {0};                       // 创建一个 epoll 事件
    ev.data.fd = fd;                            // 文件描述符
    ev.events = events;                         // 事件
    return 0 == epoll_ctl(epollFd_, EPOLL_CTL_ADD, fd, &ev);    // （树根，添加方式，fd，绑定的ev）
}

bool Epoller::ModFd(int fd, uint32_t events) {
    if(fd < 0) return false;
    epoll_event ev = {0};
    ev.data.fd = fd;
    ev.events = events;
    return 0 == epoll_ctl(epollFd_, EPOLL_CTL_MOD, fd, &ev);
}

bool Epoller::DelFd(int fd) {
    if(fd < 0) return false;
    epoll_event ev = {0};
    return 0 == epoll_ctl(epollFd_, EPOLL_CTL_DEL, fd, &ev);
}

int Epoller::Wait(int timeoutMs) {
    return epoll_wait(epollFd_, &events_[0], static_cast<int>(events_.size()), timeoutMs);
}

int Epoller::GetEventFd(size_t i) const {
    assert(i < events_.size() && i >= 0);
    return events_[i].data.fd;
}

uint32_t Epoller::GetEvents(size_t i) const {
    assert(i < events_.size() && i >= 0);
    return events_[i].events;
}