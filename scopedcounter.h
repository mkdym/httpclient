#pragma once
#include <boost/thread.hpp>



template<typename counter_t, typename mutex_t>
class scoped_counter
{
public:
    scoped_counter(counter_t& counter, mutex_t& mutex)
        : m_counter(counter)
        , m_mutex(mutex)
    {
        boost::lock_guard<mutex_t> lock(m_mutex);
        ++m_counter;
    }

    ~scoped_counter()
    {
        boost::lock_guard<mutex_t> lock(m_mutex);
        --m_counter;
    }

private:
    counter_t& m_counter;
    mutex_t& m_mutex;
};



template<typename counter_t, typename mutex_t, typename cond_t>
class scoped_counter_cond
{
public:
    scoped_counter_cond(counter_t& counter, mutex_t& mutex, cond_t& cond)
        : m_counter(counter)
        , m_mutex(mutex)
        , m_cond(cond)
    {
        boost::lock_guard<mutex_t> lock(m_mutex);
        ++m_counter;
    }

    ~scoped_counter_cond()
    {
        boost::lock_guard<mutex_t> lock(m_mutex);
        --m_counter;
        m_cond.notify_one();
    }

private:
    counter_t& m_counter;
    mutex_t& m_mutex;
    cond_t& m_cond;
};



