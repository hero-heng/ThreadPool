//
// Created by Henry Towne on 2024/7/23.
//

#include "thread.h"

#include <thread>

int Thread::generateId_ = 0;

Thread::Thread(ThreadFunc func)
    : func_(func)
    , threadId_(generateId_++)
{
}
Thread::~Thread() {

}

void Thread::start() {
    // 创建一个线程来执行一个线程函数
    std::thread t(func_,threadId_); // C++11来说，出了这个作用域，线程对象t和线程函数func_都会被析构
    t.detach();
}

int Thread::getId() const {
    return threadId_;
}
