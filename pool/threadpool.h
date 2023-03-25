#ifndef THREADPOOL_H
#define THREADPOOL_H
#include <mutex>                    // 互斥锁
#include <condition_variable>       // 条件变量
#include <queue>                    // 队列
#include <thread>                   // 线程
#include <functional>               // 函数

// 线程池类
class ThreadPool {
public:
    // explict 防止构造函数的隐式转换，只能通过构造函数创建。
    explicit ThreadPool(size_t threadCount = 8): pool_(std::make_shared<Pool>()) {  // 构造函数，一共8个线程
            assert(threadCount > 0);
            for(size_t i = 0; i < threadCount; i++) {                               // 创建8个子线程
                std::thread([pool = pool_] {
                    std::unique_lock<std::mutex> locker(pool->mtx);                 // 定义锁
                    while(true) {
                        if(!pool->tasks.empty()) {                                  // 如果任务池不为空，执行任务
                            auto task = std::move(pool->tasks.front());             // 获取一个任务
                            pool->tasks.pop();
                            locker.unlock();                                        // 释放锁
                            task();                                                 // 执行任务
                            locker.lock();                                          // 加锁
                        } 
                        else if(pool->isClosed) break;                              // 如果优雅关闭，则关闭
                        // 只要为空就唤醒
                        else pool->cond.wait(locker);                               // 否则，线程休眠。条件变量休眠
                    }
                }).detach();                                                        // 线程分离
            }
    }

    ThreadPool() = default;                     // 构造

    ThreadPool(ThreadPool&&) = default;
    
    ~ThreadPool() {                             // 析构函数
        if(static_cast<bool>(pool_)) {
            {
                std::lock_guard<std::mutex> locker(pool_->mtx);
                pool_->isClosed = true;         // 关闭
            }
            pool_->cond.notify_all();           // 把所带线程偶读唤醒              
        }
    }

    // 只要添加一个就唤醒
    template<typename F>
    void AddTask(F&& task) {                                        // 添加任务时  引用任务
        {
            std::lock_guard<std::mutex> locker(pool_->mtx);         
            pool_->tasks.emplace(std::forward<F>(task));            // 将任务 task 添加到队列 tasks
        }
        pool_->cond.notify_one();     // 当有任务到达时，通知线程。条件变量唤醒。 
    }

private:
    struct Pool {                                       // struct 池子
        std::mutex mtx;                                 // 互斥锁
        std::condition_variable cond;                   // 条件变量
        bool isClosed;                                  // 优雅关闭
        std::queue<std::function<void()>> tasks;        // 队列（存放的是任务）
    };
    std::shared_ptr<Pool> pool_;                        // 定义一个线程 pool_
};

#endif //THREADPOOL_H

// 用到 queue 来保存任务。queue 定义在线程池 Pool 中