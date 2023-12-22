/**************************************************
*
* @文件				hy.freestanding
* @作者				钱浩宇
* @创建时间			2021-11-30
* @更新时间			2022-11-30
* @摘要
* 标准库工具函数的独立实现
*
**************************************************/

#pragma once

#include "hy.types.h"
#include <bit>

#if defined(__has_builtin)
#define __has_builtin_addressof __has_builtin(__builtin_addressof)
#define __has_builtin_memcpy __has_builtin(__builtin_memcpy)
#define __has_builtin_memcmp __has_builtin(__builtin_memcmp)
#define __has_builtin_memset __has_builtin(__builtin_memset)
#define __has_builtin_bit_cast __has_builtin(__builtin_bit_cast)
#else
#define __has_builtin_addressof 0
#define __has_builtin_memcpy 0
#define __has_builtin_memcmp 0
#define __has_builtin_memset 0
#define __has_builtin_bit_cast 0
#endif

namespace hy::freestanding {
	template<typename T>
	[[nodiscard]] inline constexpr T* addressof(T& r) noexcept {
#if __has_builtin_addressof
		return __builtin_addressof(r);
#else
		return &r;
#endif
	}

	template<typename T>
	T const* addressof(T const&&) = delete;

	template<typename T>
	[[nodiscard]] inline constexpr T&& forward(remove_reference<T>& t) noexcept {
		return static_cast<T&&>(t);
	}

	template<typename T>
	[[nodiscard]] inline constexpr T&& forward(remove_reference<T>&& t) noexcept {
		return static_cast<T&&>(t);
	}

	template<typename T>
	[[nodiscard]] inline constexpr remove_reference<T>&& move(T&& t) noexcept {
		return static_cast<remove_reference<T>&&>(t);
	}

	template<typename T>
	inline constexpr void swap(T& t1, T& t2) noexcept {
		T tmp{ freestanding::move(t1) };
		t1 = freestanding::move(t2);
		t2 = freestanding::move(tmp);
	}

	template<typename T1, typename T2>
	inline constexpr T1 exchange(T1& t1, T2&& t2) noexcept {
		T1 tmp{ static_cast<T1&&>(t1) };
		t1 = static_cast<T2&&>(t2);
		return tmp;
	}

	template<copyable_memory T>
	inline constexpr void moveset(T& t1, T& t2) noexcept {
		t1 = t2;
		t2 = T{ };
	}

#if !__has_builtin_memcmp
	extern "C" int __cdecl memcmp(void const* des, void const* src, unsigned long long size);
#endif

	inline
#if __has_builtin_memcmp
	constexpr
#endif
	int compare(void const* des, void const* src, unsigned long long size) noexcept {
#if __has_builtin_memcmp
		return __builtin_memcmp(des, src, size);
#else
		return memcmp(des, src, size);
#endif
	}

#if !__has_builtin_memcpy
	extern "C" void* __cdecl memcpy(void* des, void const* src, unsigned long long size);
#endif

	inline
#if __has_builtin_memcpy
	constexpr
#endif
	void* copy(void* des, void const* src, unsigned long long size) noexcept {
#if __has_builtin_memcpy
		return __builtin_memcpy(des, src, size);
#else
		return memcpy(des, src, size);
#endif
	}

	template<copyable_memory T>
	inline
#if __has_builtin_memcpy
	constexpr
#endif
	T* copy_n(T* des, void const* src, unsigned long long n) noexcept {
		return static_cast<T*>(freestanding::copy(des, src, sizeof(T) * n));
	}

#if !__has_builtin_memset
	extern "C" void* __cdecl memset(void* des, int val, unsigned long long size);
#endif

	inline
#if __has_builtin_memset
	constexpr
#endif
	void* initialize(void* des, int val, unsigned long long size) noexcept {
#if __has_builtin_memset
		return __builtin__memset(des, val, size);
#else
		return memset(des, val, size);
#endif
	}

	template<typename T>
#if __has_builtin_memset
	constexpr
#endif
	inline T* initialize_n(T* des, int val, unsigned long long n) noexcept {
		return static_cast<T*>(freestanding::initialize(des, val, sizeof(T) * n));
	}

	template<typename To, typename From>
	requires (sizeof(To) == sizeof(From) && copyable_memory<To> && trivial<From>)
	[[nodiscard]] inline
#if __has_builtin_memcpy || __has_builtin_bit_cast
	constexpr
#endif
	To bit_cast(From const& src) noexcept {
#if __has_builtin_bit_cast
		return __builtin_bit_cast(To, src);
#else
		To des;
		copy(addressof(des), addressof(src), sizeof(To));
		return des;
#endif
	}

	template<unsigned char... args, unsigned long long Len = sizeof...(args)>
	inline constexpr void bit_set(unsigned char(&arr)[Len]) noexcept {
		constexpr unsigned char value[Len]{ args... };
		freestanding::copy(arr, value, Len * sizeof(unsigned char));
	}

	template<typename T, size_t Len>
	[[nodiscard]] inline constexpr size_t size(const T(&)[Len]) noexcept {
		return Len;
	}
}

namespace hy::freestanding::cvt {
	[[nodiscard]] inline constexpr bool isblank(char16_t ch) noexcept {
		return ch == u' ' || ch == u'\t';
	}

	[[nodiscard]] inline constexpr bool isalpha(char16_t ch) noexcept {
		ch |= 32U;
		return ch >= u'a' && ch <= u'z';
	}

	[[nodiscard]] inline constexpr bool isdigit(char16_t ch) noexcept {
		return ch >= u'0' && ch <= u'9';
	}

	[[nodiscard]] inline constexpr bool isalnum(char16_t ch) noexcept {
		return isdigit(ch) || isalpha(ch);
	}

	[[nodiscard]] inline constexpr bool check_stol(char16_t const* nptr) noexcept {
		constexpr long long min{ -9223372036854775807LL - 1 }, max{ 9223372036854775807LL };
		char16_t const* endp{ nptr };
		bool is_neg{ };
		unsigned long long n{ };
		if (*nptr == u'-') {
			nptr++;
			is_neg = true;
		}
		unsigned long long cutoff{ static_cast<unsigned long long>(is_neg ? -(min / 10) : max / 10) };
		int cutlim{ is_neg ? -(min % 10) : max % 10 };
		while (*nptr >= u'0' && *nptr <= u'9') {
			int c{ *nptr - u'0' };
			endp = ++nptr;
			if (n > cutoff || (n == cutoff && c > cutlim)) return true;
			n = n * 10 + c;
		}
		return false;
	}

	[[nodiscard]] inline constexpr long long safe_stol(char16_t const* nptr, char16_t** endptr) noexcept {
		bool is_neg{ };
		unsigned long long n{ };
		char16_t const* endp{ nptr };
		if (*nptr == u'-') {
			nptr++;
			is_neg = true;
		}
		while (*nptr >= u'0' && *nptr <= u'9') {
			n = n * 10 + *nptr - u'0';
			endp = ++nptr;
		}
		if (endptr) *endptr = const_cast<char16_t*>(endp);
		return is_neg ? -static_cast<long long>(n) : static_cast<long long>(n);
	}

	[[nodiscard]] inline constexpr long stox(char16_t const* nptr, char16_t** endptr) noexcept {
		unsigned long n{ };
		for (int c{ *nptr }; c; c = *++nptr) {
			if (c >= u'0' && c <= u'9') n = n * 16 + c - u'0';
			else {
				c |= 32U;
				if (c >= u'a' && c <= u'f') n = n * 16 + c - u'a' + 10;
				else {
					if (endptr) *endptr = const_cast<char16_t*>(nptr);
					break;
				}
			}
		}
		return static_cast<long>(n);
	}

	[[nodiscard]] inline constexpr double stod(char16_t const* s, char16_t** endptr) {
		char16_t const* p{ s };
		long double value{ };
		char16_t sign{ };
		long double factor{ };
		if (*p == u'-' || *p == u'+') sign = *p++;
		while (*p - u'0' < 10U) value = value * 10 + (*p++ - u'0');
		if (*p == u'.') {
			factor = 1.0L;
			p++;
			while (*p - u'0' < 10U) {
				factor *= 0.1;
				value += (*p++ - u'0') * factor;
			}
		}
		if ((*p | 32U) == u'e') {
			bool ret{ true };
			unsigned int expo{ };
			factor = 10.0L;
			switch (*++p) {
			case u'-': factor = 0.1L;
			case u'+': ++p; break;
			case u'0': case u'1': case u'2':
			case u'3': case u'4': case u'5':
			case u'6': case u'7': case u'8':
			case u'9': break;
			default: value = 0.0L; p = s; ret = false; break;
			}
			if (ret) {
				while (*p - u'0' < 10U) expo = 10 * expo + (*p++ - u'0');
				while (true) {
					if (expo & 1) value *= factor;
					if ((expo >>= 1) == 0) break;
					factor *= factor;
				}
			}
		}
		if (endptr) *endptr = const_cast<char16_t*>(p);
		return static_cast<double>(sign == u'-' ? -value : value);
	}


	template<unsigned_integral T>
	[[nodiscard]] inline constexpr T hash_bytes(const unsigned char* start, const unsigned char* end) noexcept {
		unsigned long long hash{ 14695981039346656037ULL };
		for (; start != end; ++start) {
			hash ^= static_cast<unsigned long long>(*start);
			hash *= 1099511628211ULL;
		}
		return static_cast<T>(hash);
	}
}

namespace hy::freestanding::endian {
	inline constexpr bool is_little_endian = std::endian::native == std::endian::little;
	inline constexpr bool is_big_endian = std::endian::native == std::endian::big;
	inline constexpr bool is_standard_endian = is_little_endian;

	template<typename U>
	requires (sizeof(U) == 1ULL || sizeof(U) == 2ULL || sizeof(U) == 4ULL || sizeof(U) == 8ULL)
	[[nodiscard]] inline constexpr U byte_swap_naive(U a) noexcept {
		if constexpr (sizeof(U) == 8ULL)
			return  ((a & 0xFF00000000000000ULL) >> 56ULL) | ((a & 0x00FF000000000000ULL) >> 40ULL) |
			((a & 0x0000FF0000000000ULL) >> 24ULL) | ((a & 0x000000FF00000000ULL) >> 8ULL) |
			((a & 0x00000000FF000000ULL) << 8ULL) | ((a & 0x0000000000FF0000ULL) << 24ULL) |
			((a & 0x000000000000FF00ULL) << 40ULL) | ((a & 0x00000000000000FFULL) << 56ULL);
		else if constexpr (sizeof(U) == 4ULL)
			return  ((a & 0xFF000000U) >> 24U) | ((a & 0x00FF0000U) >> 8U) |
			((a & 0x0000FF00U) << 8U) | ((a & 0x000000FFU) << 24U);
		else if constexpr (sizeof(U) == 2ULL)
			return static_cast<U>(static_cast<unsigned>(((a & 0xFF00U) >> 8U) | ((a & 0x00FFU) << 8U)));
		else return a;
	}

	template<unsigned_integral U>
	[[nodiscard]] inline constexpr U big_endian(U u) noexcept {
		if constexpr (sizeof(U) == 1ULL || is_big_endian) return u;
		else return byte_swap_naive(u);
	}

	template<unsigned_integral U>
	[[nodiscard]] inline constexpr U little_endian(U u) noexcept {
		if constexpr (sizeof(U) == 1ULL || is_little_endian) return u;
		else return byte_swap_naive(u);
	}

	template<unsigned_integral U>
	[[nodiscard]] inline constexpr U standard_endian(U u) noexcept { return little_endian(u); }
}