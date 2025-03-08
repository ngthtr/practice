// threadpool with task as void function (don't return any result)

#include <queue>
#include <functional>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <iostream>
class ThreadPool
{
private:
    std::atomic_bool stop = false;
    std::mutex task_vector_mtx;
    std::condition_variable thread_queue_cv;
    std::vector<std::thread> threads;
    std::queue<std::function<void()>> tasks;

public:
    ThreadPool(size_t number_threads = 1)
    {
        // create N threads
        for (size_t i = 0; i < number_threads; i++)
        {
            // create thread_member
            std::thread thread_member([this]() {
            while (true)
            {
                std::function<void()> task;
                // thread wait for being assigned task
                // use mutex to lock when try to get task from list
                {
                    std::unique_lock<std::mutex> lock(task_vector_mtx);
                    thread_queue_cv.wait(lock, [this] {
                        return !tasks.empty() || stop;
                    });
                    // return if threadpool is terminated actively or empty tasks
                    if (stop && tasks.empty())
                    {
                        return;
                    }
                    // get task from list tasks
                    task = std::move(tasks.front());
                    tasks.pop();
                }
                // do task()
                task();
            } });
            // store thread_member in pool
            threads.push_back(std::move(thread_member));
        }
    }

    // public API to add new task to ThreadPool
    void addTask(std::function<void()> task)
    {
        {
            std::unique_lock<std::mutex> lock(task_vector_mtx);
            tasks.push(std::move(task));
        }
        thread_queue_cv.notify_one();
    }
    ~ThreadPool()
    {
        std::cout << "ThreadPool deconstructor is called" << std::endl;
        {
            std::unique_lock<std::mutex> lock(task_vector_mtx);
            stop = true;
        }
        // notify all threads to wakeup and join to main thread
        // if thread completed, it will be terminated
        thread_queue_cv.notify_all();
        // thread completed work need to join (or detach)
        for (std::thread& thread : threads)
        {
            thread.join();
        }
    }
};

void task(int number)
{
    std::cout << "Task " << number << " is running on thread " << std::this_thread::get_id() << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(1));
}

int main(int argc, char const *argv[])
{
    ThreadPool thread_pool(10);
    for (size_t i = 0; i < 100; i++)
    {
        // add function<void()> as lambda
        // then function<void()> do task() -> more flexible
        thread_pool.addTask([i]{ 
            task(i); 
        });
    }
    return 0;
}