#pragma once

#include <cstddef>
#include <memory>
#if __cplusplus < 202302
    #include <type_traits>
#endif

namespace nn::util {

template <typename T, std::size_t SIZE = sizeof(T), std::size_t ALIGN = alignof(T)>
struct TypedStorage {
#if __cplusplus < 202302
    std::aligned_storage_t<SIZE, ALIGN> storage;
#else
    alignas(ALIGN) std::byte storage[SIZE];
#endif
};

template <typename T>
[[gnu::always_inline]] constexpr T* GetPointer(TypedStorage<T>& storage) {
    return std::launder(reinterpret_cast<T*>(std::addressof(storage)));
}

template <typename T>
[[gnu::always_inline]] constexpr const T* GetPointer(const TypedStorage<T>& storage) {
    return std::launder(reinterpret_cast<const T*>(std::addressof(storage)));
}

template <typename T>
[[gnu::always_inline]] constexpr T& GetReference(TypedStorage<T>& storage) {
    return *GetPointer(storage);
}

template <typename T>
[[gnu::always_inline]] constexpr const T& GetReference(const TypedStorage<T>& storage) {
    return *GetPointer(storage);
}

template <typename T, typename... Ts>
[[gnu::always_inline]] constexpr void ConstructAt(TypedStorage<T>& storage, Ts&&... args) {
    std::construct_at<T>(GetPointer(storage), std::forward<Ts>(args)...);
}

} // namespace nn::util