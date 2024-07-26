#include <iostream>
#include <chrono>
#include <thread>

#include "threadpool.h"

int sum1(int a, int b)
{
    return a + b;
}

int main() {
    ThreadPool pool;
    pool.start();

    std::future<int> r1 = pool.submitTask(sum1,1,2);
    std::cout<<r1.get()<<std::endl;

    return 0;
}
