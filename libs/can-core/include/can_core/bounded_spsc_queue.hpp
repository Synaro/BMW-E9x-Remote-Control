#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace can_core {

template <typename T, std::size_t Capacity>
class BoundedSpscQueue final {
    static_assert(Capacity > 0U, "A bounded queue needs storage");
    static_assert(
        std::is_trivially_copyable<T>::value,
        "The acquisition queue only accepts trivially copyable records");

public:
    [[nodiscard]] bool tryPush(const T& value) noexcept {
        const std::uint64_t head = head_.load(std::memory_order_relaxed);
        const std::uint64_t tail = tail_.load(std::memory_order_acquire);
        if (head - tail >= Capacity) {
            return false;
        }
        storage_[static_cast<std::size_t>(head % Capacity)] = value;
        head_.store(head + 1U, std::memory_order_release);
        return true;
    }

    [[nodiscard]] bool tryPop(T& value) noexcept {
        const std::uint64_t tail = tail_.load(std::memory_order_relaxed);
        const std::uint64_t head = head_.load(std::memory_order_acquire);
        if (tail == head) {
            return false;
        }
        value = storage_[static_cast<std::size_t>(tail % Capacity)];
        tail_.store(tail + 1U, std::memory_order_release);
        return true;
    }

    [[nodiscard]] std::size_t size() const noexcept {
        const std::uint64_t head = head_.load(std::memory_order_acquire);
        const std::uint64_t tail = tail_.load(std::memory_order_acquire);
        return static_cast<std::size_t>(head - tail);
    }

    [[nodiscard]] static constexpr std::size_t capacity() noexcept {
        return Capacity;
    }

    [[nodiscard]] bool empty() const noexcept { return size() == 0U; }
    [[nodiscard]] bool full() const noexcept { return size() == Capacity; }

    void clear() noexcept {
        const std::uint64_t head = head_.load(std::memory_order_acquire);
        tail_.store(head, std::memory_order_release);
    }

private:
    std::array<T, Capacity> storage_{};
    std::atomic<std::uint64_t> head_{0U};
    std::atomic<std::uint64_t> tail_{0U};
};

}  // namespace can_core
