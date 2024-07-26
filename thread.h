//
// Created by Henry Towne on 2024/7/23.
//

#ifndef THREADPOOL_THREAD_H
#define THREADPOOL_THREAD_H

#include <functional>

class Thread
{
public:
    using ThreadFunc = std::function<void(int)>;

    Thread(ThreadFunc func);
    ~Thread();
    // 启动线程
    void start();

    int getId() const;
private:
    ThreadFunc func_;

    static int generateId_;
    int threadId_;
};

#endif //THREADPOOL_THREAD_H
