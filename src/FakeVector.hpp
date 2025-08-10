#pragma once

/// Evil hack to create std::vector from a pointer without double allocation
template <typename T = uint8_t>
class FakeVector {
public:
    T* start;
    T* end;
    T* endOfStorage;

    constexpr FakeVector(void* start, size_t size)
        : start(static_cast<T*>(start)),
          end(static_cast<T*>(start) + size),
          endOfStorage(static_cast<T*>(start) + size) {}

    constexpr FakeVector(void const* start, size_t size)
        : start(static_cast<T*>(const_cast<void*>(start))),
          end(static_cast<T*>(const_cast<void*>(start)) + size),
          endOfStorage(static_cast<T*>(const_cast<void*>(start)) + size) {}

    std::vector<T>& asVector() {
        return *reinterpret_cast<std::vector<T>*>(this);
    }

    operator std::vector<T>&() {
        return asVector();
    }
};

static_assert(sizeof(FakeVector<>) == sizeof(std::vector<uint8_t>));
static_assert(alignof(FakeVector<>) == alignof(std::vector<uint8_t>));
