/**************************************************
*
* @文件				hy.types
* @作者				钱浩宇
* @创建时间			2021-11-05
* @更新时间			2022-11-29
* @摘要
* 工程需要的数据类型定义
*
**************************************************/

#pragma once

namespace hy::details {
#if !defined(__cplusplus)
#error "honey requires C++ compiler"
#endif

#if defined(__GNUC__) && __GNUC__ >= 11 && __cplusplus < 202002L
#error "honey requires at least C++20 standard compiler."
#endif

#if !defined(__cpp_concepts) || !defined(__cpp_constexpr) || !defined(__cpp_consteval)
#error "honey requires at least C++20 standard compiler."
#endif

    static_assert(sizeof(void*) != sizeof(int), "honey requires x64 environment.");
}

#include <string>
#include <vector>
#include <list>
#include <forward_list>
#include <unordered_set>
#include <unordered_map>
#include <algorithm>

namespace hy {
    using Int8 = signed char;
    using Int16 = short;
    using Int32 = int;
    using Int64 = long long;
    using Uint8 = unsigned char;
    using Uint16 = unsigned short;
    using Uint32 = unsigned int;
    using Uint64 = unsigned long long;

    using Float32 = float;
    using Float64 = double;

    using Byte = unsigned char;
    using Status = int;
    using Token = unsigned int;
    using Size = unsigned long long;
    using Index = unsigned long long;
    using Size16 = unsigned short;
    using Index16 = unsigned short;
    using Size32 = unsigned int;
    using Index32 = unsigned int;

    using Char = char16_t;
    using Char8 = char8_t;
    using Char16 = char16_t;
    using Char32 = char32_t;
    using Str = char16_t*;
    using CStr = char16_t const*;
    using Str8 = char8_t*;
    using CStr8 = char8_t const*;

    using Memory = void*;
    using CMemory = void const*;
}

namespace hy {
    template<typename T, typename _Traits = std::char_traits<T>,
        typename _Alloc = std::allocator<T>>
    using BasicString = std::basic_string<T, _Traits, _Alloc>;

    using String = std::u16string;
    using StringView = std::u16string_view;

    template<typename T, typename _Alloc = std::allocator<T>>
    using Vector = std::vector<T, _Alloc>;

    template<typename T, typename _Alloc = std::allocator<T>>
    using LinkList = std::forward_list<T, _Alloc>;

    template<typename T, typename _Alloc = std::allocator<T>>
    using LinkListEx = std::list<T, _Alloc>;

    template<typename Key, typename Hasher = std::hash<Key>,
        typename _Keyeq = std::equal_to<Key>, typename _Alloc = std::allocator<Key>>
    using HashSet = std::unordered_set<Key, Hasher, _Keyeq, _Alloc>;

    template<typename Key, typename Value, typename Hasher = std::hash<Key>,
        typename _Keyeq = std::equal_to<Key>, typename _Alloc = std::allocator<std::pair<const Key, Value>>>
    using HashMap = std::unordered_map<Key, Value, Hasher, _Keyeq, _Alloc>;
}

namespace hy {
    // remove_reference
    template <typename T>
    struct __remove_reference { using type = T; };

    template <typename T>
    struct __remove_reference<T&> { using type = T; };

    template <typename T>
    struct __remove_reference<T&&> { using type = T; };

    template<typename T>
    using remove_reference = __remove_reference<T>::type;

    // remove_volatile
    template <typename T>
    struct __remove_volatile { using type = T; };

    template <typename T>
    struct __remove_volatile<T volatile> { using type = T; };

    template<typename T>
    using remove_volatile = __remove_volatile<T>::type;

    // remove_const
    template <typename T>
    struct __remove_const { using type = T; };

    template <typename T>
    struct __remove_const<const T> { using type = T; };

    template <typename T>
    using remove_const = __remove_const<T>::type;

    // remove_cv
    template <typename T>
    struct __remove_cv { using type = T; };

    template <typename T>
    struct __remove_cv<const T> { using type = T; };

    template <typename T>
    struct __remove_cv<volatile T> { using type = T; };

    template <class T>
    struct __remove_cv<const volatile T> { using type = T; };

    template <typename T>
    using remove_cv = __remove_cv<T>::type;

    // remove_cvref
    template<typename T>
    using remove_cvref = remove_cv<remove_reference<T>>;

    // copyable_memory
    template<typename T>
    concept copyable_memory = __is_trivially_copyable(T);

    // construct_memory
    template<typename T>
    concept construct_memory = __is_trivially_constructible(T);

    // trivial
    template<typename T>
    concept trivial = copyable_memory<T> && construct_memory<T>;

    // same
    template <typename, typename>
    inline constexpr bool __same_v = false;

    template <typename T>
    inline constexpr bool __same_v<T, T> = true;

    template<typename T1, typename T2>
    concept same = __same_v<T1, T2>&& __same_v<T2, T1>;

    // any_of
    template <typename T, typename... Args>
    concept any_of = (same<T, Args> || ...);

    // single_byte
    template<typename T>
    concept single_byte = sizeof(T) == 1ULL;

    // character
    template<typename T>
    concept character = any_of<remove_cv<T>,
        char, signed char, unsigned char,
        wchar_t, char8_t, char16_t, char32_t>;

    // integral
    template <typename T>
    concept integral = character<T> || any_of<remove_cv<T>,
        bool, short, unsigned short, int, unsigned int,
        long, unsigned long, long long, unsigned long long>;

    // signed_integral
    template <class T>
    concept signed_integral = integral<T> && static_cast<T>(-1) < static_cast<T>(0);

    // unsigned_integral
    template <class T>
    concept unsigned_integral = integral<T> && !signed_integral<T>;

    // floating_point
    template<typename T>
    concept floating_point = any_of<remove_cv<T>, float, double, long double>;

    // arg_size
    template<typename T, typename... Args>
    inline constexpr auto arg_size = arg_size<Args...> +sizeof(remove_cvref<T>);

    template<typename T>
    inline constexpr auto arg_size<T> = sizeof(remove_cvref<T>);
}

namespace hy {
#define LIB_EXPORT extern "C" __declspec(dllexport)
}