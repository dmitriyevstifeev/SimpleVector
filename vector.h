#pragma once
#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <new>
#include <memory>
#include <type_traits>
#include <utility>

template<typename T>
class RawMemory {
 public:
  RawMemory() = default;

  explicit RawMemory(size_t capacity)
      : buffer_(Allocate(capacity))
      , capacity_(capacity) {
  }

  RawMemory(const RawMemory &) = delete;
  RawMemory &operator=(const RawMemory &rhs) = delete;
  RawMemory(RawMemory &&other) noexcept {
    Swap(other);
  }
  RawMemory &operator=(RawMemory &&rhs) noexcept {
    Swap(rhs);
    return *this;
  }

  ~RawMemory() {
    Deallocate(buffer_);
  }

  T *operator+(size_t offset) noexcept {
    // Разрешается получать адрес ячейки памяти, следующей за последним элементом массива
    assert(offset <= capacity_);
    return buffer_ + offset;
  }

  const T *operator+(size_t offset) const noexcept {
    return const_cast<RawMemory &>(*this) + offset;
  }

  const T &operator[](size_t index) const noexcept {
    return const_cast<RawMemory &>(*this)[index];
  }

  T &operator[](size_t index) noexcept {
    assert(index < capacity_);
    return buffer_[index];
  }

  void Swap(RawMemory &other) noexcept {
    std::swap(buffer_, other.buffer_);
    std::swap(capacity_, other.capacity_);
  }

  const T *GetAddress() const noexcept {
    return buffer_;
  }

  T *GetAddress() noexcept {
    return buffer_;
  }

  size_t Capacity() const {
    return capacity_;
  }

 private:
  // Выделяет сырую память под n элементов и возвращает указатель на неё
  static T *Allocate(size_t n) {
    return n != 0 ? static_cast<T *>(operator new(n * sizeof(T))) : nullptr;
  }

  // Освобождает сырую память, выделенную ранее по адресу buf при помощи Allocate
  static void Deallocate(T *buf) noexcept {
    operator delete(buf);
  }

  T *buffer_ = nullptr;
  size_t capacity_ = 0;
};

template<typename T>
class Vector {
 public:
  using iterator = T *;
  using const_iterator = const T *;

  Vector() = default;

  explicit Vector(size_t size)
      : data_(size)
      , size_(size)  //
  {
    std::uninitialized_value_construct_n(data_.GetAddress(), size_);
  }

  ~Vector() {
    std::destroy_n(data_.GetAddress(), size_);
  }

  Vector(const Vector &other)
      : data_(other.size_)
      , size_(other.size_)  //
  {
    std::uninitialized_copy_n(other.data_.GetAddress(), size_, data_.GetAddress());
  }

  Vector(Vector &&other) noexcept {
    Swap(other);
  }

  Vector &operator=(const Vector &rhs) {
    if (this != &rhs) {
      if (rhs.size_ > data_.Capacity()) {
        Vector rhs_copy(rhs);
        Swap(rhs_copy);
      } else {
        /* Скопировать элементы из rhs, создав при необходимости новые
           или удалив существующие */
        if (rhs.size_ < size_) {
          std::copy(rhs.begin(), rhs.end(), begin());
          std::destroy_n(begin()+rhs.size_, size_-rhs.size_);
        } else {
          std::copy_n(rhs.begin(), size_, begin());
          std::uninitialized_copy_n(begin() + size_, rhs.size_ - size_, begin() + size_);
        }
        size_ = rhs.size_;
      }
    }
    return *this;
  }

  Vector &operator=(Vector &&rhs) noexcept {
    Swap(rhs);
    return *this;
  }

  void Swap(Vector &other) noexcept {
    data_.Swap(other.data_);
    std::swap(size_, other.size_);
  }

  void CopyMoveNElements(iterator first, size_t count, iterator first_d){
    if constexpr  (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
      std::uninitialized_move_n(first, count, first_d);
    } else {
      std::uninitialized_copy_n(first, count, first_d);
    }
  }

  void Reserve(size_t new_capacity) {
    if (new_capacity <= data_.Capacity()) {
      return;
    }
    RawMemory<T> new_data(new_capacity);
    CopyMoveNElements(data_.GetAddress(), size_, new_data.GetAddress());
    std::destroy_n(data_.GetAddress(), size_);
    data_.Swap(new_data);
  }

  void Resize(size_t new_size) {
    if (size_ == new_size) {
      return;
    }
    if (new_size < size_) {
      std::destroy_n(data_.GetAddress() + new_size, size_ - new_size);
    } else {
      Reserve(new_size);
      std::uninitialized_value_construct_n(data_.GetAddress() + size_, new_size - size_);
    }
    size_ = new_size;
  }

  void PushBack(const T &value) {
    EmplaceBack(value);
  }

  void PushBack(T &&value) {
    EmplaceBack(std::move(value));
  }

  iterator begin() noexcept {
    return data_.GetAddress();
  }

  iterator end() noexcept {
    return data_.GetAddress() + size_;
  }

  const_iterator begin() const noexcept {
    return data_.GetAddress();
  }

  const_iterator end() const noexcept {
    return data_.GetAddress() + size_;
  }

  const_iterator cbegin() const noexcept {
    return data_.GetAddress();
  }

  const_iterator cend() const noexcept {
    return data_.GetAddress() + size_;
  }

  template<typename... Args>
  iterator Emplace(const_iterator pos, Args &&...args) {

    const auto offset = pos - begin();

    if (size_ == Capacity()) {
      const size_t new_capacity = size_ == 0 ? 1 : size_ * 2;
      RawMemory<T> new_data(new_capacity);
      new(new_data + offset) T{std::forward<Args>(args)...};

      // копируем/перемещаем предшествующие элементы
      try {
        CopyMoveNElements(begin(), offset, new_data.GetAddress());
      } catch (...) {
        std::destroy_at(new_data + offset);
        throw;
      }

      // копируем/перемещаем последующие элементы
      if (pos != cend()) {
        try {
          CopyMoveNElements(begin() + offset, size_ - offset, new_data.GetAddress() + offset + 1);
        } catch (...) {
          std::destroy_n(new_data.GetAddress(), offset + 1);
          throw;
        }
      }

      std::destroy_n(begin(), size_);
      data_.Swap(new_data);

    } else {
      if (pos == cend()) {
        new(end()) T(std::forward<Args>(args)...);
      } else {
        T t(std::forward<Args>(args)...);
        new(end()) T(std::move(*(end() - 1)));
        std::move_backward(begin() + offset, end() - 1, end());
        data_[offset] = std::move(t);
      }
    }

    ++size_;

    return begin() + offset;
  }

  template<typename... Args>
  T &EmplaceBack(Args &&...args) {
    return *Emplace(end(), std::forward<Args>(args)...);
  }

  iterator Erase(const_iterator pos) {
    assert(pos != end());
    const auto pos_it = const_cast<iterator>(pos);
    const auto element_to_del = std::move(pos_it + 1, end(), pos_it);
    Destroy(element_to_del);
    --size_;
    return pos_it;
  }

  iterator Insert(const_iterator pos, const T &value) {
    return Emplace(pos, value);
  }

  iterator Insert(const_iterator pos, T &&value) {
    return Emplace(pos, std::move(value));
  }

  void PopBack() noexcept {
    assert(size_ > 0);
    data_[size_ - 1].~T();
    --size_;
  };

  size_t Size() const noexcept {
    return size_;
  }

  size_t Capacity() const noexcept {
    return data_.Capacity();
  }

  const T &operator[](size_t index) const noexcept {
    return const_cast<Vector &>(*this)[index];
  }

  T &operator[](size_t index) noexcept {
    assert(index < size_);
    return data_[index];
  }

 private:
  // Вызывает деструктор объекта по адресу buf
  static void Destroy(T *buf) noexcept {
    buf->~T();
  }

  RawMemory<T> data_;
  size_t size_ = 0;
};