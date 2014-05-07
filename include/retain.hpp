#pragma once


#ifdef RETAIN_SINGLE_THREADED

template <typename T>
class retain_thread_local_storage
{
private: static T* m_value;
public: static inline T* get() { return m_value; }
public: static inline void set(T* value) { m_value=value; }
};

template <typename T>
T* retain_thread_local_storage<T>::m_value = 0;

#else

#include <new>
#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif

template <typename T>
class retain_thread_local_storage
{
#ifdef _WIN32
private: typedef DWORD key_t;
#else
private: typedef pthread_key_t key_t;
#endif

private: static key_t s_key;
private: static key_t key_init()
  {
#ifdef _WIN32
    if ((s_key = TlsAlloc()) == TLS_OUT_OF_INDEXES) 
      throw std::bad_alloc();
#else
    if (pthread_key_create(&s_key,0) != 0) 
      throw std::bad_alloc();
#endif
    return s_key;
  }

public: static inline T* get() 
  {
#ifdef _WIN32
    return (T*) TlsGetValue(s_key);
#else
    return (T*) pthread_getspecific(s_key);
#endif
  }

public: static inline void set(T* value) 
  {
#ifdef _WIN32
    TlsSetValue(s_key,value);
#else
    pthread_setspecific(s_key,value);
#endif
  }
};

template <typename T> typename retain_thread_local_storage<T>::key_t retain_thread_local_storage<T>::s_key = retain_thread_local_storage<T>::key_init();

#endif

template <typename T>
class retain
{
public: typedef retain_thread_local_storage< retain< T > > TLS;
private: retain<T> *m_previous;
private: T* m_as;
public: retain(T* as, bool use=true)
  {
    m_as = as;
    m_previous=TLS::get();
    if (use) TLS::set(this);
  }

public: ~retain()
  {
    if (TLS::get() == this) TLS::set(m_previous);
  }
  
public: class iterator 
  {
  private: retain<T> *m_at;
  public: iterator(retain<T> *at=0) : m_at(at) {};
  public: void operator++() { m_at=m_at->m_previous; }
  public: bool operator==(const iterator &to) const { return m_at==to.m_at; }
  public: bool operator!=(const iterator &to) const { return m_at!=to.m_at; }
  public: T& operator*() { return *m_at->m_as; }
  public: T* operator->() { return m_at->m_as; }
  };

  public: T& operator*() { return *m_as; }
  public: T* operator->() { return m_as; }

public: static iterator begin()
  {
    return iterator(TLS::get());
  }

public: static iterator end()
  {
    return iterator(0);
  }

  template <typename TT>
  friend TT*& recall();

  template <typename TT>
  friend bool retained();
};

template <typename T>
inline bool retained()
{
  return retain<T>::TLS::get() != 0;
}

template <typename T>
inline T*& recall()
{
  return retain<T>::TLS::get()->m_as;
}
