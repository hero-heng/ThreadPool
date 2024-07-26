#include <iostream>
#include <chrono>
#include <thread>

#include "threadpool.h"
#include "Task.h"

class MyTask : public Task{
public:
    MyTask(int begin, int end)
        : begin_(begin)
        , end_(end) {}
    Any run() override {
        std::cout << "task begin!tid:" << std::this_thread::get_id() << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(3));
        int sum = 0;
        for (int i = begin_; i <= end_; ++i) {
            sum += i;
        }
        std::cout << "task end!tid:" << std::this_thread::get_id() << std::endl;
        return sum;
    }
private:
    int begin_;
    int end_;
};

int main() {
    ThreadPool pool;
    pool.start();

    Result res1 = pool.submitTask(std::make_shared<MyTask>(1,100));
    int sum1 = res1.get().cast_<int>();
    std::cout<<sum1<<std::endl;
#if 0
    {
        ThreadPool pool;
        pool.setPoolMode(PoolMode::MODE_CACHED);
        pool.start();

        Result res1 = pool.submitTask(std::make_shared<MyTask>(1,100));
        Result res2 = pool.submitTask(std::make_shared<MyTask>(101,200));
        Result res3 = pool.submitTask(std::make_shared<MyTask>(201,300));
        pool.submitTask(std::make_shared<MyTask>(1,100));
        pool.submitTask(std::make_shared<MyTask>(101,200));
        pool.submitTask(std::make_shared<MyTask>(201,300));

        int sum1 = res1.get().cast_<int>();
        int sum2 = res2.get().cast_<int>();
        int sum3 = res3.get().cast_<int>();
        std::cout<<(sum1+sum2+sum3)<<std::endl;
    }
#endif
    return 0;
}
