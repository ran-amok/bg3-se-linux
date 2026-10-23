// Stand-in for MSVC ConcRT <concurrent_vector.h>. Upstream's one user (Lua/Shared/
// LuaReference.h) grows it from the Lua thread only, so a std::vector behind a mutex
// on growth is enough; element references stay valid because the vector only grows
// (reserve()d generously by the user).
#pragma once
#include <memory>
#include <mutex>
#include <vector>

namespace Concurrency
{
    template <class T, class A = std::allocator<T>>
    class concurrent_vector
    {
    public:
        using iterator = typename std::vector<T, A>::iterator;
        using const_iterator = typename std::vector<T, A>::const_iterator;
        iterator push_back(T const& v) { std::lock_guard<std::mutex> _(m_); v_.push_back(v); return v_.end() - 1; }
        iterator push_back(T&& v) { std::lock_guard<std::mutex> _(m_); v_.push_back(std::move(v)); return v_.end() - 1; }
        iterator grow_by(size_t n) { std::lock_guard<std::mutex> _(m_); auto k = v_.size(); v_.resize(k + n); return v_.begin() + k; }
        iterator grow_by(size_t n, T const& v) { std::lock_guard<std::mutex> _(m_); auto k = v_.size(); v_.resize(k + n, v); return v_.begin() + k; }
        iterator grow_to_at_least(size_t n) { std::lock_guard<std::mutex> _(m_); auto k = v_.size(); if (n > k) v_.resize(n); return v_.begin() + k; }
        T& operator [] (size_t i) { return v_[i]; }
        T const& operator [] (size_t i) const { return v_[i]; }
        T& at(size_t i) { return v_.at(i); }
        size_t size() const { return v_.size(); }
        bool empty() const { return v_.empty(); }
        void clear() { std::lock_guard<std::mutex> _(m_); v_.clear(); }
        void reserve(size_t n) { std::lock_guard<std::mutex> _(m_); v_.reserve(n); }
        iterator begin() { return v_.begin(); }
        iterator end() { return v_.end(); }
        const_iterator begin() const { return v_.begin(); }
        const_iterator end() const { return v_.end(); }

    private:
        std::mutex m_;
        std::vector<T, A> v_;
    };
}
namespace concurrency = Concurrency;
