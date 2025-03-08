#include <iostream>
#include <thread>
#include <vector>
#include <mutex>
#include <condition_variable>
int N = 9;
class TestObj
{
    public:
        int number;
        int number1;
        int number2;
        int number3;
        TestObj(int _number) : number(_number) {};
};
std::vector<TestObj> vector1;
std::vector<TestObj> vector2;
std::vector<TestObj> vector3;
std::mutex mtx1;
std::mutex mtx2;
std::mutex mtx3;
std::condition_variable cv1;
std::condition_variable cv2;
std::condition_variable cv3;

void func1()
{
    while (true)
    {
        std::unique_lock<std::mutex> lock1(mtx1);
        cv1.wait(lock1, [] { 
            return !vector1.empty(); 
        });
        // Handle logic step 1
        TestObj obj = vector1.front();
        obj.number1 = obj.number;
        // std::cout << "func1: " << obj.number1 << '\n';
        vector1.erase(vector1.begin());
        cv1.notify_one();
        // Add next job for step 2
        std::unique_lock<std::mutex> lock2(mtx2);
        cv2.wait(lock2, [] { 
            return vector2.empty(); 
        });
        vector2.push_back(std::move(obj));
        cv2.notify_one();
        if (obj.number1 == N - 1)
        {
            break;
        }
    }
}

void func2()
{
    while (true)
    {
        std::unique_lock<std::mutex> lock2(mtx2);
        cv2.wait(lock2, [] { 
            return !vector2.empty(); 
        });
        // Handle logic step 2
        TestObj obj = vector2.front();
        obj.number2 = obj.number1 * 10 + obj.number;
        // std::cout << "func2: " << obj.number2 << '\n';
        vector2.erase(vector2.begin());
        cv2.notify_one();
        // Add next job for step 3
        std::unique_lock<std::mutex> lock3(mtx3);
        cv3.wait(lock3, [] { 
            return vector3.empty(); 
        });
        vector3.push_back(std::move(obj));
        cv3.notify_one();
        if (obj.number1 == N - 1)
        {
            break;
        }
    }
}

void func3()
{
    while (true)
    {
        std::unique_lock<std::mutex> lock3(mtx3);
        cv3.wait(lock3, [] { 
            return !vector3.empty(); 
        });
        // Handle logic step 3
        TestObj obj = vector3.front();
        obj.number3 = obj.number2 * 10 + obj.number;
        std::cout << "func3: " << obj.number3 << '\n';
        vector3.erase(vector3.begin());
        cv3.notify_one();
        if (obj.number1 == N - 1)
        {
            break;
        }
    }
}

int main()
{
    std::thread t0([] {
        for (int i = 1; i < N; ++i)
        {
            std::unique_lock<std::mutex> lock(mtx1);
            cv1.wait(lock, [] { 
                return vector1.empty(); 
            });
            std::cout << "produce: " << i << '\n';
            vector1.push_back(*(new TestObj(i)));
            cv1.notify_one();
        }
    });

    std::thread t1(func1);
    std::thread t2(func2);
    std::thread t3(func3);
    t0.join();
    t1.join();
    t2.join();
    t3.join();

    return 0;
}