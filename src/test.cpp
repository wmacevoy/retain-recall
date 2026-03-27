#include <cassert>
#include <iostream>
#include <string>
#include <sstream>
#include <cstdlib>
#include <thread>
#include <chrono>

#include "retain.hpp"

int x[100];

class Test
{
public:
    int id;
    std::ostringstream out;

    Test(int _id) : id(_id) {}

    void state()
    {
        out << id << ": ";
        for (retain<int>::iterator i = retain<int>::begin();
             i != retain<int>::end(); ++i) {
            int *p = &(*i);
            if (p != nullptr) {
                out << "[" << p - x << "]";
            } else {
                out << "[null]";
            }
        }
        if (retained<int>()) {
            if (recall<int>() != nullptr) {
                out << "@" << (recall<int>() - x);
            } else {
                out << "@null";
            }
        }
        out << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(rand() % 10));
    }

    void run()
    {
        state();
        { retain<int> as(&x[id+0]); state();
            { retain<int> as(&x[id+1]); state(); }
            { retain<int> as_if(&x[id+2], true); state();
                { retain<int> as(&x[id+3]); state(); }
                { retain<int> as(&x[id+4]); state(); }
                state();
            }
            { retain<int> as_if(&x[id+5], false); state();
                { retain<int> as(&x[id+6]); state(); }
                { retain<int> as(&x[id+7]); state(); }
                state();
            }
        }
    }
};

std::string test()
{
    Test* tests[10];
    int n = 10;

    for (int k = 0; k < n; ++k) {
        tests[k] = new Test(10 * k);
    }

    std::thread threads[9];
    for (int k = 1; k < n; ++k) {
        threads[k-1] = std::thread([&, k]() { tests[k]->run(); });
    }
    tests[0]->run();
    for (int k = 1; k < n; ++k) {
        threads[k-1].join();
    }

    std::string ans;
    for (int k = 0; k < n; ++k) {
        ans += tests[k]->out.str();
    }

    for (int k = 0; k < n; ++k) {
        delete tests[k];
    }

    return ans;
}

int main()
{
    std::cout << "std::thread (thread_local)" << std::endl;

    std::string ans = test();
    std::cout << ans << std::endl;

    for (int i = 0; i < 100; ++i) {
        assert(test() == ans);
    }
}
