#pragma once

#include <cassert>
#include <cstdlib>
#include <new>
#include <utility>
#include <memory>
#include <algorithm>

template <typename T>
class RawMemory {
public:
    RawMemory() = default;

    explicit RawMemory(size_t capacity)
        : buffer_(Allocate(capacity))
        , capacity_(capacity) {
    }

    RawMemory(const RawMemory&) = delete;

    RawMemory& operator=(const RawMemory& rhs) = delete;

    RawMemory(RawMemory&& other) noexcept {
        buffer_ = other.buffer_;
        capacity_ = other.capacity_;
        other.buffer_ = nullptr;
        other.capacity_ = 0;
    }

    RawMemory& operator=(RawMemory&& rhs) noexcept { 
        this->Swap(rhs);
        return *this;
    }

    ~RawMemory() {
        Deallocate(buffer_);
    }

    T* operator+(size_t offset) noexcept {
        // Разрешается получать адрес ячейки памяти, следующей за последним элементом массива
        assert(offset <= capacity_);
        return buffer_ + offset;
    }

    const T* operator+(size_t offset) const noexcept {
        return const_cast<RawMemory&>(*this) + offset;
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<RawMemory&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < capacity_);
        return buffer_[index];
    }

    void Swap(RawMemory& other) noexcept {
        std::swap(buffer_, other.buffer_);
        std::swap(capacity_, other.capacity_);
    }

    const T* GetAddress() const noexcept {
        return buffer_;
    }

    T* GetAddress() noexcept {
        return buffer_;
    }

    size_t Capacity() const {
        return capacity_;
    }

private:
    // Выделяет сырую память под n элементов и возвращает указатель на неё
    static T* Allocate(size_t n) {
        return n != 0 ? static_cast<T*>(operator new(n * sizeof(T))) : nullptr;
    }

    // Освобождает сырую память, выделенную ранее по адресу buf при помощи Allocate
    static void Deallocate(T* buf) noexcept {
        operator delete(buf);
    }

    T* buffer_ = nullptr;
    size_t capacity_ = 0;
};

template <typename T>
class Vector {
public:
    Vector() = default;

    explicit Vector(size_t size)
        : data_(size)
        , size_(size)
    {
        std::uninitialized_value_construct_n(data_.GetAddress(), size);
    }

    Vector(const Vector& other)
        : data_(other.size_)
        , size_(other.size_)
    {
        std::uninitialized_copy_n(other.data_.GetAddress(), size_, data_.GetAddress());
    }

    Vector& operator=(const Vector& rhs) {
        if (this != &rhs) {
            if (rhs.size_ > data_.Capacity()) {
                /* Применить copy-and-swap */
                Vector rhs_copy(rhs);
                Swap(rhs_copy);
            }
            else {
                /* Скопировать элементы из rhs, создав при необходимости новые
                   или удалив существующие */
                if (rhs.size_ < size_) {
                    std::copy(rhs.data_.GetAddress(), rhs.data_.GetAddress() + rhs.size_, data_.GetAddress());
                    std::destroy_n(data_.GetAddress() + rhs.size_, size_ - rhs.size_);
                }
                else {
                    std::copy(rhs.data_.GetAddress(), rhs.data_.GetAddress() + size_, data_.GetAddress());
                    std::uninitialized_copy_n(rhs.data_.GetAddress() + size_, rhs.size_ - size_, data_.GetAddress() + size_);
                }
                size_ = rhs.size_;
            }
        }
        return *this;
    }

    Vector(Vector&& other) noexcept {
        Swap(other);
    }

    Vector& operator=(Vector&& rhs) noexcept {
        if (this != &rhs) {
            Swap(rhs);
        }
        return *this;
    }

    ~Vector() {
        std::destroy_n(data_.GetAddress(), size_);
    }

    size_t Size() const noexcept {
        return size_;
    }

    size_t Capacity() const noexcept {
        return data_.Capacity();
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<Vector&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < size_);
        return data_[index];
    }

    void Reserve(size_t new_capacity) {
        if (new_capacity <= data_.Capacity()) {
            return;
        }
        RawMemory<T> new_data(new_capacity);
        // constexpr оператор if будет вычислен во время компиляции
        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
            std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
        }
        else {
            std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
        }
        std::destroy_n(data_.GetAddress(), size_);
        data_.Swap(new_data);
    }

    void Swap(Vector& other) noexcept {
        data_.Swap(other.data_);
        std::swap(size_, other.size_);
    }

    void Resize(size_t new_size) {
        if (new_size > size_) {
            Reserve(new_size);
            std::uninitialized_value_construct_n(data_.GetAddress() + size_, new_size - size_);
        }
        else {
            std::destroy_n(data_.GetAddress() + new_size, size_ - new_size);
        }
        size_ = new_size;
    }

    void PushBack(const T& value) {
        EmplaceBack(value);
    }
    
    void PushBack(T&& value) {
        EmplaceBack(std::move(value));
    }

    void PopBack() /* noexcept */ {
        assert(size_ > 0);
        std::destroy_at(data_.GetAddress() + size_ - 1);
        --size_;
    }

    template <typename... Args>
    T& EmplaceBack(Args&&... args) {
        T* result = nullptr;
        size_t shift = end() - begin();
        if (size_ == Capacity()) {
            RawMemory<T> new_data(size_ == 0 ? 1 : size_ * 2);
            result = new (new_data + size_) T(std::forward<Args>(args)...);
            try {
                if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                    std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
                } else {
                    std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
                }
            } catch (...) {
                std::destroy_n(new_data.GetAddress(), shift);
                throw;
            }
            std::destroy_n(data_.GetAddress(), size_);
            data_.Swap(new_data);
        } else {
            result = new (data_ + size_) T(std::forward<Args>(args)...);
        }
        ++size_;
        return *result;
    }

    using iterator = T*;
    using const_iterator = const T*;
    
    iterator begin() noexcept{
        return data_.GetAddress();
    }

    iterator end() noexcept{
        return data_.GetAddress() + size_;
    }

    const_iterator begin() const noexcept{
        return data_.GetAddress();
    }

    const_iterator end() const noexcept{
        return data_.GetAddress() + size_;
    }

    const_iterator cbegin() const noexcept{
        return begin();
    }

    const_iterator cend() const noexcept{
        return end();
    }

    template <typename... Args>
    iterator Emplace(const_iterator pos, Args&&... args){
        assert(pos >= begin() && pos <= end());
        size_t shift = pos - begin();

        if (size_ == Capacity()) {
            RawMemory<T> new_data(size_ == 0 ? 1 : size_ * 2);
            new (new_data + shift) T(std::forward<Args>(args)...);

            try {
                if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                    std::uninitialized_move_n(data_.GetAddress(), shift, new_data.GetAddress());
                } else {
                    std::uninitialized_copy_n(data_.GetAddress(), shift, new_data.GetAddress());
                }
            } catch (...) {
                std::destroy_at(new_data + shift);
                throw;
            }
            
            try {
                if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                    std::uninitialized_move_n(begin() + shift, size_ - shift, new_data.GetAddress() + shift + 1);
                } else {
                    std::uninitialized_copy_n(begin() + shift, size_ - shift, new_data.GetAddress() + shift + 1);
                }
            } catch (...) {
                std::destroy_n(new_data.GetAddress(), shift + 1);
                throw;
            }
            
            std::destroy_n(begin(), size_);
            data_.Swap(new_data);
            
        } else {
            if (size_ == shift)
                new (data_ + size_) T(std::move(*(end() - 1)));
            else {
                T copy(std::forward<Args>(args)...);
                new (data_ + size_) T(std::move(data_[size_ - 1]));
                
                size_t constructed_elements = 0;
                try {
                    std::move_backward(begin() + shift, end() - 1, end());
                    constructed_elements = end() - (begin() + shift);
                } catch (...) {
                    std::destroy_at(data_ + size_);
                    if (constructed_elements > 0) {
                        std::destroy(begin() + shift, end() - 1);
                    }
                    throw;
                }
                //std::move_backward(begin() + shift, end() - 1, end());
                data_[shift] = std::move(copy);
            }
        }

        ++size_;
        return begin() + shift;
    } 
    
    iterator Erase(const_iterator pos) /*noexcept(std::is_nothrow_move_assignable_v<T>)*/{
        assert(pos >= begin() && pos < end());
        size_t shift = pos - begin();
        std::move(begin() + shift + 1, end(), begin() + shift);
        PopBack();
        return begin() + shift;
    }

    iterator Insert(const_iterator pos, const T& value){
        return Emplace(pos, value);
    }

    iterator Insert(const_iterator pos, T&& value){
        return Emplace(pos, std::move(value));
    }

private:
    RawMemory<T> data_;
    size_t size_ = 0;
   
};
