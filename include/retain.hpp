#pragma once

//
// retain/recall semantics — C++17
//
// A retain<T> object is an automatic (stack-scoped) variable that publishes
// a pointer to T on a thread-local stack.  recall<T>() retrieves the most
// recently retained T* from the current thread.  When the retain<T> goes
// out of scope its destructor pops the stack — classic RAII, tied to the
// lifetime of the enclosing block.
//

template <typename T>
class retain
{
private:
    static inline thread_local retain<T>* s_current = nullptr;

    retain<T>* m_previous;
    T* m_as;

public:
    retain(T* as, bool use = true)
        : m_previous(s_current), m_as(as)
    {
        if (use) s_current = this;
    }

    ~retain()
    {
        if (s_current == this) s_current = m_previous;
    }

    // Tied to stack frame — not copyable or movable
    retain(const retain&) = delete;
    retain& operator=(const retain&) = delete;

    T& operator*() { return *m_as; }
    T* operator->() { return m_as; }

    class iterator
    {
    private:
        retain<T>* m_at;
    public:
        iterator(retain<T>* at = nullptr) : m_at(at) {}
        void operator++() { m_at = m_at->m_previous; }
        bool operator==(const iterator& to) const { return m_at == to.m_at; }
        bool operator!=(const iterator& to) const { return m_at != to.m_at; }
        T& operator*() { return *m_at->m_as; }
        T* operator->() { return m_at->m_as; }
    };

    static iterator begin() { return iterator(s_current); }
    static iterator end() { return iterator(nullptr); }

    template <typename TT>
    friend TT*& recall();

    template <typename TT>
    friend bool retained();
};

template <typename T>
inline bool retained()
{
    return retain<T>::s_current != nullptr;
}

template <typename T>
inline T*& recall()
{
    return retain<T>::s_current->m_as;
}
