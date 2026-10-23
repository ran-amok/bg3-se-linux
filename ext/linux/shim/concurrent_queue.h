// Stand-in for MSVC ConcRT <concurrent_queue.h>: a mutex around std::deque, which is
// all upstream asks of it (push, try_pop, empty, unsafe_size, clear).
#pragma once
#include <deque>
#include <memory>
#include <mutex>

namespace Concurrency
{
    template <class T, class A = std::allocator<T>>
    class concurrent_queue
    {
    public:
        void push(T const& v) { std::lock_guard<std::mutex> _(m_); q_.push_back(v); }
        void push(T&& v) { std::lock_guard<std::mutex> _(m_); q_.push_back(std::move(v)); }
        bool try_pop(T& out)
        {
            std::lock_guard<std::mutex> _(m_);
            if (q_.empty()) return false;
            out = std::move(q_.front());
            q_.pop_front();
            return true;
        }
        bool empty() const { std::lock_guard<std::mutex> _(m_); return q_.empty(); }
        size_t unsafe_size() const { return q_.size(); }
        void clear() { std::lock_guard<std::mutex> _(m_); q_.clear(); }

    private:
        mutable std::mutex m_;
        std::deque<T, typename std::allocator_traits<A>::template rebind_alloc<T>> q_;
    };
}
namespace concurrency = Concurrency;
