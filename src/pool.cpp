#include <iostream>
#include <string>
#include <set>
#include <cstdlib>
#include "retain.hpp"

//
// setup
//

struct Pooled;

struct Pool
{
    typedef std::set<Pooled*> Contents;
    Contents contents;
    ~Pool();
};

struct Pooled {
    void *operator new(size_t size)
    {
        void *at = malloc(size);
        if (at == nullptr) throw std::bad_alloc();
        recall<Pool>()->contents.insert((Pooled*)at);
        return at;
    }
    virtual ~Pooled() {
        std::cout << "destroying Pooled@" << ((void*) this) << std::endl;
    }
};

Pool::~Pool()
{
    for (auto *p : contents) {
        delete p;
    }
}

//
// use
//

struct Thing : public Pooled {
    std::string m_name;
    Thing(const std::string &name) : m_name(name) {}
    ~Thing() {
        std::cout << "destroying Thing@" << ((void*) this)
                  << " name=" << m_name << std::endl;
    }
};


int main() {

    Pool apples;
    Pool oranges;

    { retain<Pool> as(&apples);

        new Thing("jona gold");
        new Thing("braeburn");
    }

    { retain<Pool> as(&oranges);
        new Thing("naval");
        new Thing("blood");
    }

    return 0;
}
