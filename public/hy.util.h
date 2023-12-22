/**************************************************
*
* @文件				hy.util
* @作者				钱浩宇
* @创建时间			2021-11-05
* @更新时间			2022-11-29
* @摘要
* 工具辅助模块
*
**************************************************/

#pragma once

#include "hy.freestanding.h"

// util::Array
// util::FixedArrayView
// util::ArrayView
// util::DArrayView
namespace hy::util {
	template<copyable_memory T>
	struct Array {
		using value_type = T;
		using pointer = T*;
		using const_pointer = T const*;
		using reference = T&;
		using const_reference = T const&;
		constexpr static auto value_size = sizeof(value_type);
		using size_type = remove_const<decltype(value_size)>;

		pointer mData;
		size_type mSize;

		constexpr explicit Array(size_type len = { }) noexcept : mSize{ len } {
			mData = mSize ? static_cast<pointer>(::operator new(mSize * value_size, std::nothrow)) : nullptr;
			if (mData == nullptr) mSize = 0ULL;
		}

		constexpr Array(const Array& arr) noexcept : Array(arr.mSize) {
			if (mSize) freestanding::copy_n(mData, arr.mData, mSize);
		}

		constexpr Array(Array&& arr) noexcept {
			freestanding::moveset(mData, arr.mData);
			freestanding::moveset(mSize, arr.mSize);
		}

		constexpr void release() noexcept {
			if (mData) {
				::operator delete(mData);
				mData = nullptr;
			}
			mSize = 0ULL;
		}

		constexpr ~Array() noexcept {
			release();
		}

		constexpr Array& operator = (const Array& arr) noexcept {
			if (this != &arr) {
				if (mData) ::operator delete(mData);
				mSize = arr.mSize;
				if (mSize) {
					mData = static_cast<pointer>(::operator new(mSize * value_size, std::nothrow));
					if (mData == nullptr) mSize = 0ULL;
					else freestanding::copy_n(mData, arr.mData, mSize);
				}
				else mData = nullptr;
			}
			return *this;
		}

		constexpr Array& operator = (Array&& arr) noexcept {
			freestanding::swap(mData, arr.mData);
			freestanding::swap(mSize, arr.mSize);
			return *this;
		}

		constexpr void resize(size_type len) noexcept {
			if (!mSize) {
				mSize = len;
				mData = mSize ? static_cast<pointer>(::operator new(mSize * value_size, std::nothrow)) : nullptr;
				if (mData == nullptr) mSize = 0ULL;
			}
		}

		[[nodiscard]] constexpr size_type size() const noexcept {
			return mSize;
		}

		[[nodiscard]] constexpr pointer data() noexcept {
			return mData;
		}

		[[nodiscard]] constexpr const_pointer data() const noexcept {
			return mData;
		}

		[[nodiscard]] constexpr bool empty() const noexcept {
			return mSize == 0ULL;
		}

		[[nodiscard]] constexpr reference operator [] (size_type index) noexcept {
			return mData[index];
		}

		[[nodiscard]] constexpr const_reference operator [] (size_type index) const noexcept {
			return mData[index];
		}

		[[nodiscard]] constexpr pointer begin() const noexcept {
			return mData;
		}

		[[nodiscard]] constexpr const_pointer cbegin() const noexcept {
			return mData;
		}

		[[nodiscard]] constexpr pointer end() const noexcept {
			return mData + mSize;
		}

		[[nodiscard]] constexpr const_pointer cend() const noexcept {
			return mData + mSize;
		}
	};

	template<typename T, unsigned_integral S, S fixed_len>
	struct FixedArrayView {
		using value_type = T;
		using pointer = T*;
		using const_pointer = T const*;
		using reference = T&;
		using const_reference = T const&;
		using size_type = S;
		inline static constexpr const size_type mSize = fixed_len;

		pointer mView;

		constexpr explicit FixedArrayView(pointer p = { }) noexcept : mView{ p } {}

		[[nodiscard]] constexpr size_type size() const noexcept {
			return mSize;
		}

		[[nodiscard]] constexpr pointer data() noexcept {
			return mView;
		}

		[[nodiscard]] constexpr const_pointer data() const noexcept {
			return mView;
		}

		[[nodiscard]] constexpr reference operator [] (size_type index) noexcept {
			return mView[index];
		}

		[[nodiscard]] constexpr const_reference operator [] (size_type index) const noexcept {
			return mView[index];
		}

		[[nodiscard]] constexpr pointer begin() const noexcept {
			return mView;
		}

		[[nodiscard]] constexpr const_pointer cbegin() const noexcept {
			return mView;
		}

		[[nodiscard]] constexpr pointer end() const noexcept {
			return mView + mSize;
		}

		[[nodiscard]] constexpr const_pointer cend() const noexcept {
			return mView + mSize;
		}

		constexpr void reset(pointer p = { }) noexcept {
			mView = p;
		}
	};

	template<typename T, unsigned_integral S>
	struct ArrayView {
		using value_type = T;
		using pointer = T*;
		using const_pointer = T const*;
		using reference = T&;
		using const_reference = T const&;
		using size_type = S;

		pointer mView;
		size_type mSize;

		constexpr explicit ArrayView(pointer p = { }, size_type len = { }) noexcept : mView{ p }, mSize{ len } {}

		[[nodiscard]] constexpr size_type size() const noexcept {
			return mSize;
		}

		[[nodiscard]] constexpr pointer data() noexcept {
			return mView;
		}

		[[nodiscard]] constexpr const_pointer data() const noexcept {
			return mView;
		}

		[[nodiscard]] constexpr bool empty() const noexcept {
			return mSize == 0ULL;
		}

		[[nodiscard]] constexpr reference operator [] (size_type index) noexcept {
			return mView[index];
		}

		[[nodiscard]] constexpr const_reference operator [] (size_type index) const noexcept {
			return mView[index];
		}

		[[nodiscard]] constexpr pointer begin() const noexcept {
			return mView;
		}

		[[nodiscard]] constexpr const_pointer cbegin() const noexcept {
			return mView;
		}

		[[nodiscard]] constexpr pointer end() const noexcept {
			return mView + mSize;
		}

		[[nodiscard]] constexpr const_pointer cend() const noexcept {
			return mView + mSize;
		}

		constexpr void reset(pointer p = { }, size_type len = { }) noexcept {
			mView = p;
			mSize = len;
		}
	};

	template<typename T, unsigned_integral S>
	struct DArrayView {
		using value_type = T;
		using pointer = T*;
		using const_pointer = T const*;
		using iterator = ArrayView<T, S>;
		using const_iterator = const ArrayView<T, S>;
		using size_type = S;

		pointer mView;
		size_type mRow, mCol;

		constexpr explicit DArrayView(pointer p = { }, size_type row = { }, size_type col = { })
			noexcept : mView{ p }, mRow{ row }, mCol{ col } {}

		[[nodiscard]] constexpr size_type row() const noexcept {
			return mRow;
		}

		[[nodiscard]] constexpr size_type col() const noexcept {
			return mCol;
		}

		[[nodiscard]] constexpr size_type size() const noexcept {
			return mRow * mCol; 
		}

		[[nodiscard]] constexpr pointer data() noexcept {
			return mView;
		}

		[[nodiscard]] constexpr const_pointer data() const noexcept {
			return mView;
		}

		[[nodiscard]] constexpr bool empty() const noexcept {
			return mRow * mCol == 0ULL;
		}

		[[nodiscard]] constexpr iterator operator [] (size_type index) noexcept {
			return { mView + index * mCol, mCol };
		}

		[[nodiscard]] constexpr const_iterator operator [] (size_type index) const noexcept {
			return { mView + index * mCol, mCol };
		}

		[[nodiscard]] constexpr iterator begin() const noexcept {
			return { mView, mCol };
		}

		[[nodiscard]] constexpr const_iterator cbegin() const noexcept {
			return { mView, mCol };
		}

		[[nodiscard]] constexpr iterator end() const noexcept {
			return { mView + mRow * mCol, mCol };
		}

		[[nodiscard]] constexpr const_iterator cend() const noexcept {
			return { mView + mRow * mCol, mCol };
		}

		constexpr void reset(pointer p = { }, size_type row = { }, size_type col = { }) noexcept {
			mView = p; 
			mRow = row; 
			mCol = col;
		}
	};
}

// util::ByteArray
namespace hy::util {
	class ByteArray : protected BasicString<Byte> {
	public:
		constexpr ByteArray() noexcept = default;

		explicit constexpr ByteArray(Size len) noexcept : BasicString<Byte>(len, 0U) {}

		constexpr ByteArray(CMemory data, Size len) noexcept :
			BasicString<Byte>(static_cast<const Byte*>(data), len) {}

		[[nodiscard]] constexpr bool empty() const noexcept {
			return BasicString<Byte>::empty();
		}

		[[nodiscard]] constexpr Size size() const noexcept {
			return BasicString<Byte>::size();
		}

		[[nodiscard]] constexpr Byte* data() noexcept {
			return BasicString<Byte>::data();
		}

		[[nodiscard]] constexpr const Byte* data() const noexcept {
			return BasicString<Byte>::data();
		}

		constexpr void resize(Size size) noexcept {
			BasicString<Byte>::resize(size);
		}

		constexpr void reserve(Size size) noexcept {
			BasicString<Byte>::reserve(size);
		}

		constexpr void clear() noexcept {
			BasicString<Byte>::clear(); 
		}

		constexpr Byte* begin() noexcept {
			return BasicString<Byte>::data();
		}

		constexpr Byte* end() noexcept { 
			return BasicString<Byte>::data() + BasicString<Byte>::size();
		}

		constexpr void assign(CMemory data, Size len) noexcept {
			BasicString<Byte>::assign(static_cast<const Byte*>(data), len);
		}

		constexpr ByteArray& operator += (const ByteArray& other) noexcept {
			BasicString<Byte>::append(other);
			return *this;
		}

		[[nodiscard]] constexpr Byte& operator [] (Size index) noexcept {
			return BasicString<Byte>::operator[](index);
		}

		[[nodiscard]] constexpr Byte operator [] (Size index) const noexcept {
			return BasicString<Byte>::operator[](index);
		}

	private:

		template<copyable_memory T, typename... Args>
		constexpr void append_set(Byte* base, const T arg, const Args... args) noexcept {
			freestanding::copy(base, freestanding::addressof(arg), sizeof(T));
			if constexpr (sizeof...(Args) > 0ULL) append_set(base + sizeof(T), args...);
		}

	public:
		template<typename... Args>
		constexpr void append(const Args... args) noexcept {
			BasicString<Byte>::append(arg_size<Args...>, 0U);
			append_set(data() + size() - arg_size<Args...>, args...);
		}

		template<typename T>
		[[nodiscard]] constexpr T* append_wait() noexcept {
			BasicString<Byte>::append(sizeof(T), 0U);
			return static_cast<T*>(static_cast<Memory>(data() + size() - sizeof(T)));
		}

		constexpr void append_bytes(Size len, CMemory m) noexcept {
			BasicString<Byte>::append(static_cast<const Byte*>(m), len);
		}
	};
}

// util::TinyStack
// util::BoolStack
// util::Stack
namespace hy::util {
	template<copyable_memory T, Size size>
	struct FixedStack {
		T mBase[size]{ };
		T* mTop{ mBase };

		constexpr FixedStack() noexcept = default;

		FixedStack(const FixedStack&) = delete;
		FixedStack(FixedStack&&) = delete;
		FixedStack& operator = (const FixedStack&) = delete;
		FixedStack& operator = (FixedStack&&) = delete;

		constexpr void push(T value) noexcept {
			*mTop++ = value;
		}

		constexpr T pop() noexcept {
			return *--mTop;
		}

		[[nodiscard]] constexpr T top() const noexcept {
			return *(mTop - 1);
		}

		[[nodiscard]] constexpr bool notnull() const noexcept {
			return mTop != mBase;
		}

		constexpr void clear() noexcept {
			mTop = mBase;
		}
	};

	struct BoolStack {
		Uint64 data = 0ULL;
		Byte length = 0ULL;

		constexpr BoolStack() noexcept = default;

		constexpr void push(bool value) noexcept {
			data <<= 1ULL;
			data |= static_cast<Uint64>(value);
			++length;
		}

		constexpr bool pop() noexcept {
			bool ret{ static_cast<bool>(data & 1ULL) };
			data >>= 1ULL;
			--length;
			return ret;
		}

		constexpr void top(bool value) noexcept {
			if (length) {
				if (value) data |= 1ULL;
				else data &= 0xFFFFFFFFFFFFFFFEULL;
			}
		}

		[[nodiscard]] constexpr bool top() const noexcept {
			return static_cast<bool>(data & 1ULL);
		}

		[[nodiscard]] constexpr bool notnull() const noexcept {
			return length != 0ULL;
		}

		constexpr void clear() noexcept {
			length = 0ULL;
			data = 0ULL;
		}
	};

	template<copyable_memory T>
	struct TrivialStack {
		constexpr static auto DEFAULT_CAPACITY { 8ULL };

		T* mBase;
		T* mTop;
		Size mCapacity;

		TrivialStack() noexcept : mCapacity{ DEFAULT_CAPACITY } {
			mTop = mBase = static_cast<T*>(::operator new(mCapacity * sizeof(T), std::nothrow));
		}

		~TrivialStack() noexcept {
			::operator delete(mBase);
		}

		template<typename... Args>
		constexpr void push(Args&&... args) noexcept {
			if (static_cast<Size>(mTop - mBase) == mCapacity) {
				auto newCapacity{ static_cast<Size>(mCapacity * 1.5) };
				auto newBase{ static_cast<T*>(::operator new(newCapacity * sizeof(T), std::nothrow)) };
				freestanding::copy_n(newBase, mBase, mCapacity);
				::operator delete(mBase);
				mBase = newBase;
				mTop = mBase + mCapacity;
				mCapacity = newCapacity;
			}
			*mTop++ = T{ freestanding::forward<Args>(args)... };
		}

		constexpr void pop() noexcept {
			--mTop;
		}

		[[nodiscard]] constexpr T pop_move() noexcept {
			return *--mTop;
		}

		[[nodiscard]] constexpr T& top() noexcept {
			return *(mTop - 1);
		}

		[[nodiscard]] constexpr const T& top() const noexcept {
			return *(mTop - 1);
		}

		[[nodiscard]] constexpr bool empty() const noexcept {
			return mBase == mTop;
		}

		constexpr void clear() noexcept {
			mTop = mBase;
		}
	};

	template<typename T>
	struct Stack : protected Vector<T> {
		using Container = Vector<T>;

		constexpr Stack() noexcept = default;

		template<typename... Args>
		constexpr void push(Args&&... args) noexcept {
			Container::emplace_back(freestanding::forward<Args>(args)...);
		}

		constexpr void pop() noexcept {
			Container::pop_back();
		}

		[[nodiscard]] constexpr T pop_move() noexcept {
			T t{ freestanding::move(Container::back()) };
			Container::pop_back();
			return t;
		}

		[[nodiscard]] constexpr T& top() noexcept {
			return Container::back();
		}

		[[nodiscard]] constexpr const T& top() const noexcept {
			return Container::back();
		}

		[[nodiscard]] constexpr bool empty() const noexcept {
			return Container::empty();
		}

		constexpr void clear() noexcept {
			Container::clear();
		}
	};
}

// util::StringViewMap
// util::StringMap
// util::Args
namespace hy::util {
	template<typename Value>
	struct StringViewMap : HashMap<StringView, Value> {
		using Container = HashMap<StringView, Value>;

		[[nodiscard]] bool has(const StringView key) const noexcept {
			return Container::contains(key);
		}

		template<typename... Args>
		Value& set(const StringView key, Args&&... args) noexcept {
			if (auto iter{ Container::find(key) }; iter != Container::end()) {
				iter->second = Value{ freestanding::forward<Args>(args)... };
				return iter->second;
			}
			else return Container::try_emplace(key, freestanding::forward<Args>(args)...).first->second;
		}

		[[nodiscard]] Value* get(const StringView key) noexcept {
			if (auto iter{ Container::find(key) }; iter != Container::end()) return &iter->second;
			return nullptr;
		}

		[[nodiscard]] const Value* get(const StringView key) const noexcept {
			if (auto iter{ Container::find(key) }; iter != Container::cend()) return &iter->second;
			return nullptr;
		}
	};

	template<typename Value>
	class StringMap {
	private:
		HashMap<StringView, Value> data;
		LinkList<String> cache;
	public:
		StringMap() noexcept {}

		void assign(const StringMap& sm) noexcept {
			cache.clear();
			data.clear();
			for (auto& [key, value] : sm)
				data.try_emplace(cache.emplace_front(key), value);
		}

		StringMap(const StringMap& sm) noexcept {
			assign(sm);
		}

		StringMap& operator = (const StringMap& sm) noexcept {
			if (this != &sm) assign(sm);
			return *this;
		}

		[[nodiscard]] bool empty() const noexcept {
			return data.empty();
		}

		[[nodiscard]] Size size() const noexcept { 
			return data.size(); 
		}

		void clear() noexcept {
			cache.clear();
			data.clear();
		}

		[[nodiscard]] auto begin() noexcept { 
			return data.begin();
		}

		[[nodiscard]] auto begin() const noexcept { 
			return data.begin();
		}

		[[nodiscard]] auto end() noexcept { 
			return data.end();
		}

		[[nodiscard]] auto end() const noexcept { 
			return data.end();
		}

		[[nodiscard]] bool has(const StringView key) const noexcept {
			return data.contains(key);
		}

		template<typename... Args>
		Value& set(const StringView key, Args&&... args) noexcept {
			if (auto iter{ data.find(key) }; iter != data.end()) {
				iter->second = Value{ freestanding::forward<Args>(args)... };
				return iter->second;
			}
			else {
				auto& str{ cache.emplace_front(key) };
				return data.try_emplace(str, freestanding::forward<Args>(args)...).first->second;
			}
		}

		[[nodiscard]] Value* get(const StringView key) noexcept {
			if (auto iter{ data.find(key) }; iter != data.end()) return &iter->second;
			return nullptr;
		}

		[[nodiscard]] const Value* get(const StringView key) const noexcept {
			if (auto iter{ data.find(key) }; iter != data.cend()) return &iter->second;
			return nullptr;
		}
	};

	struct Args : private StringMap<StringView> {
		void setView(const StringView arg) noexcept {
			auto p{ arg.data() };
			while (*p && *p != u':') ++p;
			set(arg.substr(0, p - arg.data()), *p == u':' ? p + 1 : p);
		}

		[[nodiscard]] StringView getView(const StringView sv) const noexcept {
			if (auto p{ get(sv) }) return *p;
			return { };
		}

		[[nodiscard]] bool hasProp(const StringView sv) const noexcept {
			if (auto p{ get(sv) }) return p->empty();
			return false;
		}

		[[nodiscard]] bool hasValue(const StringView sv) const noexcept {
			if (auto p{ get(sv) }) return !p->empty();
			return false;
		}

		[[nodiscard]] Size count() const noexcept {
			return size();
		}
	};
}

// util::Path
namespace hy::util {
	namespace details {
		inline constexpr auto __Slash{
#ifdef _WIN64
			u'\\'
#else
			u'/'
#endif
		};

		inline constexpr bool __isRootPrefix(CStr first, CStr end) noexcept {
#ifdef _WIN64
			if (end - first >= 3) {
				Uint32 value{ static_cast<Uint32>(first[0]) & 0xFFFFFFDFU };
				return value >= u'A' && value <= u'Z' && first[1] == u':' && first[2] == __Slash;
			}
			return false;
#else
			return end - first >= 1 && *first == __Slash;
#endif
		}

		inline constexpr CStr __getRoot(CStr first) noexcept {
			return first +
#ifdef _WIN64
				3
#else
				1
#endif
				;
		}

		inline constexpr CStr __getDot(CStr first, CStr end) noexcept {
			for (auto p{ end - 1 }; p != first - 1; --p) {
				if (*p == u'.') return p;
			}
			return end;
		}

		inline constexpr CStr __getParent(CStr first, CStr end) noexcept {
			for (--end; end != first - 1; --end) {
				if (*end == __Slash) return ++end;
			}
			return first;
		}

	}

	class Path {
		String data;

	public:
		constexpr static auto SLASH{ details::__Slash };

	private:
		[[nodiscard]] CStr begin() const noexcept {
			return data.data();
		}

		[[nodiscard]] CStr end() const noexcept {
			return data.data() + data.size();
		}

		[[nodiscard]] CStr actend() const noexcept {
			return isFile() ? end() : end() - 1;
		}

	public:
		Path() noexcept = default;

		Path(CStr str) noexcept {
			data.assign(str);
		}

		Path(const StringView sv) noexcept {
			data.assign(sv);
		}

		[[nodiscard]] bool empty() const noexcept {
			return data.empty();
		}

		[[nodiscard]] bool isAbsolute() const noexcept { 
			return details::__isRootPrefix(begin(), end());
		}

		[[nodiscard]] bool isRelative() const noexcept {
			return !isAbsolute(); 
		}

		[[nodiscard]] bool isFile() const noexcept {
			return !empty() && data.back() != details::__Slash;
		}

		[[nodiscard]] bool isDirectory() const noexcept {
			return !empty() && data.back() == details::__Slash;
		}

		Path& addSlash() noexcept {
			data.push_back(details::__Slash);
			return *this;
		}

		[[nodiscard]] Path getParent() const noexcept {
			Path p;
			if (!empty()) p.data.assign(begin(), details::__getParent(begin(), actend()));
			return p;
		}

		[[nodiscard]] Path getRoot() const noexcept {
			Path p;
			if (isAbsolute()) p.data.assign(begin(), details::__getRoot(begin()));
			return p;
		}

		[[nodiscard]] Path getStem() const noexcept {
			Path p;
			auto s{ isAbsolute() ? details::__getRoot(begin()) : begin() };
			if (!empty()) p.data.assign(s, details::__getParent(begin(), actend()));
			return p;
		}

		[[nodiscard]] Path getFileName() const noexcept {
			Path p;
			if (!empty()) p.data.assign(details::__getParent(begin(), actend()), actend());
			return p;
		}

		[[nodiscard]] Path getFileNameNoExt() const noexcept {
			Path p;
			if (isFile()) {
				auto s{ details::__getParent(begin(), end()) };
				p.data.assign(s, details::__getDot(s, end()));
			}
			return p;
		}

		[[nodiscard]] Path getExtension() const noexcept {
			Path p;
			if (isFile()) {
				auto s{ details::__getParent(begin(), end()) };
				p.data.assign(details::__getDot(s, end()), end());
			}
			return p;
		}

		void clear() noexcept {
			data.clear();
		}

		[[nodiscard]] StringView toView() const noexcept {
			return data;
		}

		[[nodiscard]] String toString() const noexcept {
			return data;
		}

		Path& operator += (const Path& p) noexcept {
			if (this != &p) {
				if (empty()) data.assign(p.data);
				else if (isDirectory() && p.isRelative()) data.append(p.data);
			}
			return *this;
		}

		[[nodiscard]] Path operator + (const Path& p) const noexcept {
			Path tmp{ *this };
			tmp += p;
			return tmp;
		}

		[[nodiscard]] bool operator == (const Path& p) const noexcept {
			return data == p.data;
		}
	};
}