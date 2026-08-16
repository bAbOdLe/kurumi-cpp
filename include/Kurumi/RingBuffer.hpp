#ifndef KURUMI_RING_BUFFER_HPP
#define KURUMI_RING_BUFFER_HPP

#include <atomic>
#include <iterator>
#include <cassert>
#include <memory>
#include <optional>
#include <utility>
#include <stdexcept>

namespace kurumi {
    template<typename T, typename AllocT = std::allocator<T>>
    class RingBuffer {
    public:
        using element_t = T;
        using allocator_t = AllocT;
        using pointer_t = std::allocator_traits<allocator_t>::pointer;
        using const_pointer_t = std::allocator_traits<allocator_t>::const_pointer;

    private:
        struct deleter {
            RingBuffer* const owner;

            void operator()(pointer_t ptr) const noexcept {
                if (ptr == nullptr) {
                    return;
                }

                std::allocator_traits<allocator_t>::destroy(owner->m_alloc, ptr);
                owner->move_rear_next();
            }
        };

    public:
        explicit RingBuffer(std::size_t capacity) :
            m_capacity(capacity + 1),
            m_head(0),
            m_rear(0),
            m_head_state(0),
            m_rear_state(0),
            m_buffer(nullptr)
        {
            if (m_capacity > get_maximum_capacity()) {
                throw std::invalid_argument("Buffer capacity must be less than UINT32_MAX - 1");
            }
            m_buffer = std::allocator_traits<AllocT>::allocate(m_alloc, m_capacity);
        }

        RingBuffer(RingBuffer&&) = delete;

        RingBuffer& operator=(RingBuffer&&) = delete;

        RingBuffer(const RingBuffer&) = delete;

        RingBuffer& operator=(const RingBuffer&) = delete;

        ~RingBuffer() noexcept {
            std::allocator_traits<allocator_t>::deallocate(m_alloc, m_buffer, m_capacity);
        }

        [[nodiscard]] static consteval std::size_t get_maximum_capacity() noexcept {
            return std::numeric_limits<uint32_t>::max() - 1;
        }

        [[nodiscard]] std::size_t get_capacity() const noexcept {
            return m_capacity - 1;
        }

        [[nodiscard]] std::size_t get_size() const noexcept {
            return m_size.load(std::memory_order_acquire);
        }

        [[nodiscard]] bool is_empty() const noexcept {
            return get_size() == 0;
        }

        template<typename... ArgTs>
        pointer_t try_emplace(ArgTs&&... args)
            noexcept(std::is_nothrow_constructible_v<T, ArgTs...>)
        {
            pointer_t _ptr = acquire_head();

            if (_ptr == nullptr) {
                return nullptr;
            }

            construct_at(_ptr, std::forward<ArgTs>(args)...);

            return _ptr;
        }

        template<typename... ArgTs>
        pointer_t emplace(ArgTs&&... args) {
            if (auto* _ptr = try_emplace(std::forward<ArgTs>(args)...); _ptr != nullptr) {
                return _ptr;
            }

            throw std::logic_error("Buffer is full");
        }

        template<typename... ArgTs>
        pointer_t force_emplace(ArgTs&&... args)
            noexcept(std::is_nothrow_constructible_v<T, ArgTs...>)
        {
            pointer_t _ptr = acquire_head();

            while (_ptr == nullptr) {
                _ptr = acquire_rear();
                std::allocator_traits<allocator_t>::destroy(m_alloc, _ptr);

                move_rear_next();

                _ptr = acquire_head();
            }

            construct_at(_ptr, std::forward<ArgTs>(args)...);

            return _ptr;
        }

        pointer_t try_push(const T& value)
            noexcept(std::is_nothrow_copy_constructible_v<T>)
        {
            return try_emplace(value);
        }

        pointer_t push(const T& value) {
            return emplace(value);
        }

        pointer_t force_push(const T& value)
            noexcept(std::is_nothrow_copy_constructible_v<T>)
        {
            return force_emplace(value);
        }

        [[nodiscard]] T pop() {
            pointer_t _ptr = acquire_rear();

            if (_ptr == nullptr) {
                throw std::logic_error("Buffer is empty");
            }

            std::unique_ptr<T, deleter> _ptr_ref(_ptr, deleter{ this });
            if constexpr (std::is_move_constructible_v<T>) {
                return std::move(*_ptr_ref);
            }
            else {
                return *_ptr_ref;
            }
        }

        [[nodiscard]] std::optional<T> try_pop()
            noexcept((std::movable<T>  && std::is_move_constructible_v<T>) ||
                     (std::copyable<T> && std::is_nothrow_copy_constructible_v<T>))
        {
            pointer_t _ptr = acquire_rear();

            if (_ptr == nullptr) {
                return std::nullopt;
            }

            std::unique_ptr<T, deleter> _ptr_ref(_ptr, deleter{ this });
            if constexpr (std::is_move_constructible_v<T>) {
                return std::optional<T>(std::in_place, std::move(*_ptr_ref));
            }
            else {
                return *_ptr_ref;
            }
        }

        [[nodiscard]] T acquire() {
            pointer_t _ptr = acquire_rear_with_blocking();

            std::unique_ptr<T, deleter> _ptr_ref(_ptr, deleter{ this });
            if constexpr (std::is_move_constructible_v<T>) {
                return std::move(*_ptr_ref);
            }
            else {
                return *_ptr_ref;
            }
        }

        template<std::ranges::output_range<T> OutputRangeT>
        void pop_all(OutputRangeT& range) {
            const auto [_rear, _end] = acquire_rear_all();

            std::size_t _index = _rear;
            try {
                auto _it = std::back_inserter(range);

                while (_index != _end) {
                    auto* _ptr = m_buffer + _index;
                    if constexpr (std::is_move_constructible_v<T>) {
                        *_it = std::move(*_ptr);
                    }
                    else {
                        *_it = *_ptr;
                    }
                    move_rear_next();

                    _index = ++_index % m_capacity;
                }
            }
            catch (...) {
                while (_index != _end) {
                    std::allocator_traits<allocator_t>::destroy(m_alloc, m_buffer + _index);
                    move_rear_next();

                    _index = ++_index % m_capacity;
                }

                throw;
            }
        }

        void clear() noexcept {
            const auto [_rear, _end] = acquire_rear_all();

            for (auto _index = _rear; _index != _end; _index = (_index + 1) % m_capacity) {
                std::allocator_traits<allocator_t>::destroy(m_alloc, m_buffer + _index);
                move_rear_next();
            }
        }

    private:
        [[nodiscard]] static constexpr uint64_t make_state(uint64_t index, uint64_t writing_count) noexcept {
            return index | (writing_count << 32);
        }

        [[nodiscard]] static constexpr uint64_t get_index(uint64_t state) noexcept {
            return state & static_cast<uint64_t>(std::numeric_limits<uint32_t>::max());
        }

        [[nodiscard]] static constexpr uint64_t get_write_count(uint64_t state) noexcept {
            return (state & (static_cast<uint64_t>(std::numeric_limits<uint32_t>::max()) << 32)) >> 32;
        }

        template<typename... ArgTs>
        void construct_at(pointer_t ptr, ArgTs&&... args) noexcept(std::is_nothrow_constructible_v<T, ArgTs...>) {
            if constexpr (std::is_nothrow_constructible_v<T, ArgTs...>) {
                std::allocator_traits<allocator_t>::construct(m_alloc, ptr, std::forward<ArgTs>(args)...);
            }
            else {
                try {
                    std::allocator_traits<allocator_t>::construct(m_alloc, ptr, std::forward<ArgTs>(args)...);
                }
                catch (...) {
                    release_head();
                    throw;
                }
            }

            move_head_next();
        }

        [[nodiscard]] pointer_t acquire_head() noexcept {
            uint64_t _expected = m_head_state.load(std::memory_order_acquire);
            uint64_t _desired = 0;
            do {
                const auto _head_index = (get_index(_expected) + 1) % m_capacity;
                if (_head_index == m_rear.load(std::memory_order_relaxed)) {
                    return nullptr;
                }

                _desired = make_state(_head_index, get_write_count(_expected) + 1);
            } while (!m_head_state.compare_exchange_weak(_expected,
                                                         _desired,
                                                         std::memory_order_release,
                                                         std::memory_order_acquire));

            return m_buffer + get_index(_expected);
        }

        void release_head() noexcept {
            uint64_t _expected = m_head_state.load(std::memory_order_acquire);
            uint64_t _desired = 0;
            do {
                const auto _head_index = (get_index(_expected) + m_capacity - 1) % m_capacity;

                _desired = make_state(_head_index, get_write_count(_expected) - 1);
            } while (!m_head_state.compare_exchange_weak(_expected,
                                                         _desired,
                                                         std::memory_order_release,
                                                         std::memory_order_acquire));
        }

        void move_head_next() noexcept {
            std::optional<uint64_t> _next_head{};

            uint64_t _expected = m_head_state.load(std::memory_order_acquire);
            uint64_t _desired = 0;
            do {
                const auto _head_index = get_index(_expected);
                const auto _writing_heads = get_write_count(_expected);

                if (_writing_heads == 1) {
                    _next_head = _head_index;
                }
                else {
                    _next_head = std::nullopt;
                }

                _desired = make_state(_head_index, _writing_heads - 1);
            } while (!m_head_state.compare_exchange_weak(_expected,
                                                         _desired,
                                                         std::memory_order_release,
                                                         std::memory_order_acquire));

            if (_next_head.has_value()) {
                const auto _old_head = m_head.exchange(_next_head.value(), std::memory_order_release);

                m_size.fetch_add((m_capacity + _next_head.value() - _old_head) % m_capacity, std::memory_order_release);
                m_size.notify_all();
            }
        }

        [[nodiscard]] pointer_t acquire_rear() noexcept {
            uint64_t _expected = m_rear_state.load(std::memory_order_acquire);
            uint64_t _desired = 0;
            do {
                const auto _rear_index = get_index(_expected);
                if (_rear_index == m_head.load(std::memory_order_relaxed)) {
                    return nullptr;
                }

                _desired = make_state((_rear_index + 1) % m_capacity, get_write_count(_expected) + 1);
            } while (!m_rear_state.compare_exchange_weak(_expected,
                                                         _desired,
                                                         std::memory_order_release,
                                                         std::memory_order_acquire));

            return get_index(_expected) + m_buffer;
        }

        [[nodiscard]] pointer_t acquire_rear_with_blocking() noexcept {
            uint64_t _expected = m_rear_state.load(std::memory_order_acquire);
            uint64_t _desired = 0;
            do {
                const auto _rear_index = get_index(_expected);
                while (_rear_index == m_head.load(std::memory_order_acquire)) {
                    m_size.wait(0, std::memory_order_relaxed);
                }

                _desired = make_state((_rear_index + 1) % m_capacity, get_write_count(_expected) + 1);
            } while (!m_rear_state.compare_exchange_weak(_expected,
                                                         _desired,
                                                         std::memory_order_release,
                                                         std::memory_order_acquire));

            return get_index(_expected) + m_buffer;
        }

        [[nodiscard]] std::tuple<uint32_t, uint32_t> acquire_rear_all() noexcept {
            uint64_t _expected = m_rear_state.load(std::memory_order_acquire);
            uint64_t _desired = 0;

            uint32_t _head = 0;

            do {
                const auto _rear_index = get_index(_expected);
                _head = m_head.load(std::memory_order_relaxed);
                if (_rear_index == _head) {
                    return { static_cast<uint32_t>(_rear_index), _head };
                }

                const std::size_t _size = (m_capacity + _head - _rear_index) % m_capacity;

                _desired = make_state(_head, get_write_count(_expected) + _size);
            } while (!m_rear_state.compare_exchange_weak(_expected,
                                                         _desired,
                                                         std::memory_order_release,
                                                         std::memory_order_acquire));

            return { static_cast<uint32_t>(get_index(_expected)), _head };
        }

        void move_rear_next() noexcept {
            std::optional<uint64_t> _next_rear{};

            uint64_t _expected = m_rear_state.load(std::memory_order_acquire);
            uint64_t _desired = 0;
            do {
                const auto _rear_index = get_index(_expected);
                const auto _writing_rears = get_write_count(_expected);

                if (_writing_rears == 1) {
                    _next_rear = _rear_index;
                }
                else {
                    _next_rear = std::nullopt;
                }

                _desired = make_state(_rear_index, _writing_rears - 1);
            } while (!m_rear_state.compare_exchange_weak(_expected,
                                                         _desired,
                                                         std::memory_order_release,
                                                         std::memory_order_acquire));

            if (_next_rear.has_value()) {
                const auto _old_rear = m_rear.exchange(_next_rear.value(), std::memory_order_release);

                m_size.fetch_sub((m_capacity + _next_rear.value() - _old_rear) % m_capacity, std::memory_order_release);
                m_size.notify_all();
            }
        }

        const std::size_t m_capacity;

        std::atomic_size_t m_size;

        std::atomic_uint32_t m_head;
        std::atomic_uint32_t m_rear;
        std::atomic_uint64_t m_head_state;
        std::atomic_uint64_t m_rear_state;

        allocator_t m_alloc;

        pointer_t m_buffer;

    };
}

#endif // !KURUMI_RING_BUFFER_HPP