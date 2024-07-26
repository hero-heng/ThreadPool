//
// Created by Henry Towne on 2024/7/23.
//

#include "Task.h"

Any Result::get() {
    if(!isValid_) {
        return "";
    }

    sem_.wait(); //task任务如果没有执行完，这里会阻塞用户的线程
    return std::move(any_);
}

void Result::setVal(Any any) {
    any_ = std::move(any);
    sem_.post(); // 已经获取的任务的返回值，增加信号量资源
}

Result::Result(std::shared_ptr<Task> task, bool isValid)
    : task_(task), isValid_(isValid) {
    task_->setResult(this);
}

void Task::exec() {
    if(result_ != nullptr) {
        result_->setVal(run());
    }
}

void Task::setResult(Result *res) {
    result_ = res;
}

