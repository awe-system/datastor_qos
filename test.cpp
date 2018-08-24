
#include <iostream>
#include <functional>

class B
{
public:
    void output(int a, int b)
    {
        std::cout << a - b << std::endl;
    }
};

class A
{
public:
    void test(std::function<void(int)> f)
    {
        f(4);
    }
};

int main(int argc, char *argv[])
{
    B b;
    A a;
    a.test(std::bind(&B::output, b, std::placeholders::_1, 3));
    a.test(std::bind(&B::output, b, 3, std::placeholders::_1));
    return 0;
}
