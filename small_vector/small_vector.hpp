#pragma once

#include <algorithm>
#include <cstddef>
#include <initializer_list>
#include <memory>
#include <stdexcept>
#include <utility>

template<typename T, size_t N, typename Allocator = std::allocator<T>>
class SmallVector {
private:
    using AllocTraits = std::allocator_traits<Allocator>;

    alignas(T) unsigned char stack_[N * sizeof(T)];
    T* data_;
    size_t sz_;
    size_t cap_;
    bool using_stack_;
    Allocator alloc_;

    void switch_to_dynamic(size_t new_cap) {
        T* new_data = AllocTraits::allocate(alloc_, new_cap);
        size_t i = 0;
        try {
            for (; i < sz_; ++i)
                AllocTraits::construct(alloc_, new_data + i, std::move_if_noexcept(data_[i]));
        } catch (...) {
            for (size_t j = 0; j < i; ++j)
                AllocTraits::destroy(alloc_, new_data + j);
            AllocTraits::deallocate(alloc_, new_data, new_cap);
            throw;
        }
        for (size_t j = 0; j < sz_; ++j)
            AllocTraits::destroy(alloc_, data_ + j);
        if (!using_stack_)
            AllocTraits::deallocate(alloc_, data_, cap_);
        data_ = new_data;
        cap_ = new_cap;
        using_stack_ = false;
    }

public:
    using Iterator = T*;
    using ConstIterator = const T*;

    SmallVector() noexcept
        : data_(reinterpret_cast<T*>(stack_))
        , sz_(0)
        , cap_(N)
        , using_stack_(true)
        , alloc_() {}

    explicit SmallVector(size_t size)
        : SmallVector() {
        resize(size);
    }

    SmallVector(size_t size, const T& obj)
        : SmallVector() {
        resize(size, obj);
    }

    SmallVector(const SmallVector& other)
        : data_(reinterpret_cast<T*>(stack_))
        , sz_(0)
        , cap_(N)
        , using_stack_(true)
        , alloc_(other.alloc_) {
        reserve(other.sz_);
        for (ConstIterator it = other.cbegin(); it != other.cend(); ++it)
            push_back(*it);
    }

    SmallVector(SmallVector&& other) noexcept
        : SmallVector() {
        swap(other);
    }

    ~SmallVector() noexcept {
        for (size_t i = 0; i < sz_; ++i)
            AllocTraits::destroy(alloc_, data_ + i);
        if (!using_stack_)
            AllocTraits::deallocate(alloc_, data_, cap_);
    }

    SmallVector& operator=(const SmallVector& other) {
        if (this != &other) {
            SmallVector tmp(other);
            swap(tmp);
        }
        return *this;
    }

    SmallVector& operator=(SmallVector&& other) noexcept {
        if (this != &other) {
            swap(other);
        }
        return *this;
    }

    T& operator[](size_t index) {
        return data_[index];
    }

    const T& operator[](size_t index) const {
        return data_[index];
    }

    size_t size() const noexcept {
        return sz_;
    }

    size_t capacity() const noexcept {
        return cap_;
    }

    bool empty() const noexcept {
        return sz_ == 0;
    }

    void reserve(size_t new_cap) {
        if (new_cap <= cap_) return;
        if (using_stack_ && new_cap <= N) {
            cap_ = new_cap;
            return;
        }
        switch_to_dynamic(new_cap);
    }

    void resize(size_t n) {
        if (n < sz_) {
            for (size_t i = n; i < sz_; ++i)
                AllocTraits::destroy(alloc_, data_ + i);
            sz_ = n;
        } else if (n > sz_) {
            if (n > cap_) reserve(n);
            for (; sz_ != n; ++sz_)
                AllocTraits::construct(alloc_, data_ + sz_);
        }
    }

    void resize(size_t n, const T& value) {
        if (n < sz_) {
            for (size_t i = n; i < sz_; ++i)
                AllocTraits::destroy(alloc_, data_ + i);
            sz_ = n;
        } else if (n > sz_) {
            if (n > cap_) reserve(n);
            for (; sz_ != n; ++sz_)
                AllocTraits::construct(alloc_, data_ + sz_, value);
        }
    }


    template<typename... Args>
    void emplace_back(Args&&... args) {
        if (sz_ == cap_) {
            size_t new_cap = cap_ == 0 ? : cap_ * 2;
            reserve(new_cap);
        }
        AllocTraits::construct(alloc_, data_ + sz_, std::forward<Args>(args)...);
        ++sz_;
    }

    void push_back(const T& value) {
        emplace_back(value);
    }

    void push_back(T&& value) {
        emplace_back(std::move(value));
    }

    void pop_back() {
        if (!empty()) {
            --sz_;
            AllocTraits::destroy(alloc_, data_ + sz_);
        }
    }

    void clear() noexcept {
        for (size_t i = 0; i < sz_; ++i)
            AllocTraits::destroy(alloc_, data_ + i);
        sz_ = 0;
    }

    Iterator begin() noexcept { return data_; }
    ConstIterator begin() const noexcept { return data_; }
    Iterator end() noexcept { return data_ + sz_; }
    ConstIterator end() const noexcept { return data_ + sz_; }
    ConstIterator cbegin() const noexcept { return data_; }
    ConstIterator cend() const noexcept { return data_ + sz_; }

    void swap(SmallVector& other) noexcept {
        using std::swap;
        swap(alloc_, other.alloc_);
        if (using_stack_ && other.using_stack_) {
            size_t min_sz = std::min(sz_, other.sz_);
            for (size_t i = 0; i < min_sz; ++i)
                swap(data_[i], other.data_[i]);
            if (sz_ > other.sz_) {
                size_t extra = sz_ - min_sz;
                for (size_t i = 0; i < extra; ++i) {
                    AllocTraits::construct(other.alloc_, other.data_ + other.sz_, std::move_if_noexcept(data_[min_sz + i]));
                    ++other.sz_;
                    AllocTraits::destroy(alloc_, data_ + min_sz + i);
                }
                sz_ = min_sz;
            } else if (other.sz_ > sz_) {
                size_t extra = other.sz_ - min_sz;
                for (size_t i = 0; i < extra; ++i) {
                    AllocTraits::construct(alloc_, data_ + sz_, std::move_if_noexcept(other.data_[min_sz + i]));
                    ++sz_;
                    AllocTraits::destroy(other.alloc_, other.data_ + min_sz + i);
                }
                other.sz_ = min_sz;
            }
        } else if (!using_stack_ && !other.using_stack_) {
            swap(data_, other.data_);
            swap(sz_, other.sz_);
            swap(cap_, other.cap_);
            swap(using_stack_, other.using_stack_);
        } else if (using_stack_) {
            size_t stack_sz = sz_;
            T* heap_data = other.data_;
            size_t heap_sz = other.sz_;
            size_t heap_cap = other.cap_;

            data_ = heap_data;
            sz_ = heap_sz;
            cap_ = heap_cap;
            using_stack_ = false;

            other.data_ = reinterpret_cast<T*>(other.stack_);
            other.sz_ = 0;
            other.cap_ = N;
            other.using_stack_ = true;

            T* old_stack = reinterpret_cast<T*>(stack_);
            for (size_t i = 0; i < stack_sz; ++i) {
                AllocTraits::construct(other.alloc_, other.data_ + other.sz_, std::move_if_noexcept(old_stack[i]));
                ++other.sz_;
                AllocTraits::destroy(alloc_, old_stack + i);
            }
        } else {
            size_t stack_sz = other.sz_;
            T* heap_data = data_;
            size_t heap_sz = sz_;
            size_t heap_cap = cap_;

            other.data_ = heap_data;
            other.sz_ = heap_sz;
            other.cap_ = heap_cap;
            other.using_stack_ = false;

            data_ = reinterpret_cast<T*>(stack_);
            sz_ = 0;
            cap_ = N;
            using_stack_ = true;

            T* old_stack = reinterpret_cast<T*>(other.stack_);
            for (size_t i = 0; i < stack_sz; ++i) {
                AllocTraits::construct(alloc_, data_ + sz_, std::move_if_noexcept(old_stack[i]));
                ++sz_;
                AllocTraits::destroy(other.alloc_, old_stack + i);
            }
        }
    }
};
