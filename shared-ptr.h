#pragma once

#include <cstddef>
#include <memory>
#include <type_traits>
#include <utility>

namespace details {

struct control_block_base {
public:
  void add_strong_ref() noexcept;

  void release_strong_ref() noexcept;

  void add_weak_ref() noexcept;

  void release_weak_ref() noexcept;

  std::size_t get_strong_refs() const noexcept;

private:
  virtual void delete_data() noexcept = 0;

protected:
  virtual ~control_block_base();

private:
  std::size_t strong_refs_{1};
  std::size_t weak_refs_{1};
};

template <typename T, typename Deleter>
struct mem_control_block : control_block_base {
  explicit mem_control_block(T* ptr, Deleter deleter) : data_(ptr), deleter_(std::move(deleter)) {}

private:
  void delete_data() noexcept override {
    deleter_(data_);
  }

private:
  T* data_;
  [[no_unique_address]] Deleter deleter_;
};

template <typename T>
struct inplace_control_block : control_block_base {
public:
  T* get_ptr() {
    return &storage_.data;
  }

  template <typename... Args>
  inplace_control_block(Args&&... args) {
    std::construct_at(&storage_.data, std::forward<Args>(args)...);
  }

private:
  void delete_data() noexcept override {
    std::destroy_at(&storage_.data);
  }

private:
  union storage {
    storage() {}

    ~storage() {}

    T data;
  } storage_;
};

} // namespace details

template <typename T>
class shared_ptr {

  template <typename Any>
  friend class shared_ptr;

  template <typename Any>
  friend class weak_ptr;

  using control_block_t = details::control_block_base;

public:
  shared_ptr() noexcept : ptr_(nullptr), cb_(nullptr) {}

  shared_ptr(std::nullptr_t) noexcept : shared_ptr() {}

  template <typename Y>
  explicit shared_ptr(Y* ptr) : shared_ptr(ptr, std::default_delete<Y>()) {}

  template <typename Y, typename Deleter>
  shared_ptr(Y* ptr, Deleter deleter) : ptr_(ptr) {
    try {
      cb_ = new details::mem_control_block(ptr, std::move(deleter));
    } catch (...) {
      deleter(ptr);
      throw;
    }
  }

  template <typename Y>
  shared_ptr(const shared_ptr<Y>& other, T* ptr) noexcept : shared_ptr(ptr, other.cb_) {}

  template <typename Y>
  shared_ptr(shared_ptr<Y>&& other, T* ptr) noexcept : ptr_(ptr),
                                                       cb_(std::exchange(other.cb_, nullptr)) {
    other.ptr_ = nullptr;
  }

  shared_ptr(const shared_ptr& other) noexcept : shared_ptr(other, other.ptr_) {}

  template <typename Y>
  shared_ptr(const shared_ptr<Y>& other) noexcept : shared_ptr(other, other.ptr_) {}

  shared_ptr(shared_ptr&& other) noexcept : shared_ptr(std::move(other), other.ptr_) {}

  template <typename Y>
  shared_ptr(shared_ptr<Y>&& other) noexcept : shared_ptr(std::move(other), other.ptr_) {}

  shared_ptr& operator=(const shared_ptr& other) noexcept {
    shared_ptr(other).swap(*this);
    return *this;
  }

  template <typename Y>
  shared_ptr& operator=(const shared_ptr<Y>& other) noexcept {
    shared_ptr(other).swap(*this);
    return *this;
  }

  shared_ptr& operator=(shared_ptr&& other) noexcept {
    shared_ptr(std::move(other)).swap(*this);
    return *this;
  }

  template <typename Y>
  shared_ptr& operator=(shared_ptr<Y>&& other) noexcept {
    shared_ptr(std::move(other)).swap(*this);
    return *this;
  }

  T* get() const noexcept {
    return ptr_;
  }

  operator bool() const noexcept {
    return ptr_;
  }

  T& operator*() const noexcept {
    return *ptr_;
  }

  T* operator->() const noexcept {
    return ptr_;
  }

  std::size_t use_count() const noexcept {
    return cb_ ? cb_->get_strong_refs() : 0;
  }

  void reset() noexcept {
    shared_ptr().swap(*this);
  }

  template <typename Y>
  void reset(Y* new_ptr) {
    shared_ptr(new_ptr).swap(*this);
  }

  template <typename Y, typename Deleter>
  void reset(Y* new_ptr, Deleter deleter) {
    shared_ptr(new_ptr, std::move(deleter)).swap(*this);
  }

  void swap(shared_ptr& other) noexcept {
    std::swap(ptr_, other.ptr_);
    std::swap(cb_, other.cb_);
  }

  ~shared_ptr() {
    if (cb_) {
      cb_->release_strong_ref();
    }
  }

  template <typename Y, typename... Args>
  friend shared_ptr<Y> make_shared(Args&&... args);

private:
  explicit shared_ptr(T* ptr, control_block_t* cb) noexcept : ptr_(ptr), cb_(cb) {
    if (cb_) {
      cb_->add_strong_ref();
    }
  }

  explicit shared_ptr(details::inplace_control_block<T>* cb) noexcept : ptr_(cb->get_ptr()), cb_(cb) {}

  T* ptr_;
  control_block_t* cb_;
};

template <typename TL, typename TR>
bool operator==(const shared_ptr<TL>& lhs, const shared_ptr<TR>& rhs) {
  return lhs.get() == rhs.get();
}

template <typename T>
bool operator==(const shared_ptr<T>& lhs, std::nullptr_t) {
  return !lhs.get();
}

template <typename T>
bool operator==(std::nullptr_t, const shared_ptr<T>& rhs) {
  return !rhs.get();
}

template <typename TL, typename TR>
bool operator!=(const shared_ptr<TL>& lhs, const shared_ptr<TR>& rhs) {
  return !(lhs == rhs);
}

template <typename T>
bool operator!=(const shared_ptr<T>& lhs, std::nullptr_t) {
  return !(lhs == nullptr);
}

template <typename T>
bool operator!=(std::nullptr_t, const shared_ptr<T>& rhs) {
  return !(nullptr == rhs);
}

template <typename T>
class weak_ptr {
  template <typename Any>
  friend class weak_ptr;

  using control_block_t = details::control_block_base;

public:
  weak_ptr() noexcept : ptr_(nullptr), cb_(nullptr) {}

  template <typename Y>
  weak_ptr(const shared_ptr<Y>& other) noexcept : weak_ptr(other.ptr_, other.cb_) {}

  weak_ptr(const weak_ptr& other) noexcept : weak_ptr(other.ptr_, other.cb_) {}

  template <typename Y>
  weak_ptr(const weak_ptr<Y>& other) noexcept : weak_ptr(other.ptr_, other.cb_) {}

  weak_ptr(weak_ptr&& other) noexcept
      : ptr_(std::exchange(other.ptr_, nullptr)),
        cb_(std::exchange(other.cb_, nullptr)) {}

  template <typename Y>
  weak_ptr(weak_ptr<Y>&& other) noexcept
      : ptr_(std::exchange(other.ptr_, nullptr)),
        cb_(std::exchange(other.cb_, nullptr)) {}

  template <typename Y>
  weak_ptr& operator=(const shared_ptr<Y>& other) noexcept {
    weak_ptr(other).swap(*this);
    return *this;
  }

  weak_ptr& operator=(const weak_ptr& other) noexcept {
    weak_ptr(other).swap(*this);
    return *this;
  }

  template <typename Y>
  weak_ptr& operator=(const weak_ptr<Y>& other) noexcept {
    weak_ptr(other).swap(*this);
    return *this;
  }

  weak_ptr& operator=(weak_ptr&& other) noexcept {
    weak_ptr(std::move(other)).swap(*this);
    return *this;
  }

  template <typename Y>
  weak_ptr& operator=(weak_ptr<Y>&& other) noexcept {
    weak_ptr(std::move(other)).swap(*this);
    return *this;
  }

  shared_ptr<T> lock() const noexcept {
    if (!cb_ || cb_->get_strong_refs() == 0) {
      return shared_ptr<T>();
    }
    return shared_ptr<T>(ptr_, cb_);
  }

  void reset() noexcept {
    weak_ptr().swap(*this);
  }

  void swap(weak_ptr& other) noexcept {
    std::swap(ptr_, other.ptr_);
    std::swap(cb_, other.cb_);
  }

  ~weak_ptr() {
    if (cb_) {
      cb_->release_weak_ref();
    }
  }

private:
  explicit weak_ptr(T* ptr, control_block_t* cb) noexcept : ptr_(ptr), cb_(cb) {
    if (cb_) {
      cb_->add_weak_ref();
    }
  }

  T* ptr_;
  control_block_t* cb_;
};

template <typename T, typename... Args>
shared_ptr<T> make_shared(Args&&... args) {
  auto cb = new details::inplace_control_block<T>(std::forward<Args>(args)...);
  return shared_ptr<T>(cb);
}
