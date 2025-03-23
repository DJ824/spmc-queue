#pragma once
#include <atomic>
#include <cstddef>
#include <memory>
#include <optional>
#include <utility>

#ifndef CACHE_LINE_SIZE
#define CACHE_LINE_SIZE 64
#endif

template <typename T, size_t SIZE> class LockFreeSPMCQueue {
private:
  struct alignas(CACHE_LINE_SIZE) Node {
    std::atomic<uint64_t> version_{0};
    alignas(T) unsigned char storage_[sizeof(T)];

    Node() noexcept = default;
    ~Node() {
      if (version_.load(std::memory_order_relaxed) % 2 == 1) {
        reinterpret_cast<T *>(storage_)->~T();
      }
    }

    Node(const Node &) = delete;
    Node &operator=(const Node &) = delete;
  };

  static constexpr size_t CAPACITY = SIZE;
  static constexpr bool IS_POWER_OF_TWO = (CAPACITY & (CAPACITY - 1)) == 0;
  static constexpr size_t INDEX_MASK = IS_POWER_OF_TWO ? (CAPACITY - 1) : 0;
  static constexpr size_t PADDING = CACHE_LINE_SIZE / sizeof(Node);
  static constexpr size_t BUFFER_SIZE = CAPACITY + 2 * PADDING;

  alignas(CACHE_LINE_SIZE) std::atomic<size_t> write_counter_{0};

  alignas(CACHE_LINE_SIZE) std::unique_ptr<Node[]> buffer_;

  Node &get_node(size_t idx) {
    if constexpr (IS_POWER_OF_TWO) {
      return buffer_[PADDING + (idx & INDEX_MASK)];
    } else {
      return buffer_[PADDING + (idx % CAPACITY)];
    }
  } 

public:
   class alignas(CACHE_LINE_SIZE) Reader {
  private:
    size_t read_position_{0};
    LockFreeSPMCQueue &queue_;

  public:
    explicit Reader(LockFreeSPMCQueue &queue) : queue_(queue) {}

    size_t position() const { return read_position_; }

    std::optional<T> try_read() { return queue_.try_read_at(read_position_); }

    std::optional<T> read() {
      auto result = try_read();
      if (result) {
        read_position_++;
      }
      return result;
    }

    void advance(size_t count = 1) { read_position_ += count; }

    void reset(size_t position) { read_position_ = position; }
  };

  LockFreeSPMCQueue() : buffer_(std::make_unique<Node[]>(BUFFER_SIZE)) {}

  ~LockFreeSPMCQueue() {}

  LockFreeSPMCQueue(const LockFreeSPMCQueue &) = delete;
  LockFreeSPMCQueue &operator=(const LockFreeSPMCQueue &) = delete;
  LockFreeSPMCQueue(LockFreeSPMCQueue &&) = delete;
  LockFreeSPMCQueue &operator=(LockFreeSPMCQueue &&) = delete;

  Reader create_reader() { return Reader(*this); }

  Reader create_reader_at(size_t position) {
    Reader reader(*this);
    reader.reset(position);
    return reader;
  }

  size_t write_position() const {
    return write_counter_.load(std::memory_order_acquire);
  }

  template <typename U> bool enqueue(U &&item) {
    const size_t write_pos = write_counter_.fetch_add(1, std::memory_order_acquire);
    Node &node = get_node(write_pos);

    uint64_t curr_version = node.version_.load(std::memory_order_acquire);
    uint64_t new_version = curr_version + 1;

    if (curr_version % 2 == 1) {
      node.version_.store(new_version, std::memory_order_release);
      new_version++;
    }

    new (node.storage_) T(std::forward<U>(item));

    node.version_.store(new_version, std::memory_order_release);

    return true;
  }

  std::optional<T> try_read_at(size_t position) {
    Node &node = get_node(position);

    uint64_t version = node.version_.load(std::memory_order_acquire);
    if (version % 2 == 1) {
      T *item_ptr = reinterpret_cast<T *>(node.storage_);
      std::optional<T> result(std::move(*item_ptr));

      node.version_.compare_exchange_strong(version, version + 2,
                                            std::memory_order_release,
                                            std::memory_order_relaxed);

      return result;
    }

    return std::nullopt;
  }

  bool empty_at(size_t position) const {
    return position >= write_counter_.load(std::memory_order_acquire);
  }

  size_t capacity() const { return CAPACITY; }
};
