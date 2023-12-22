/**************************************************
*
* @文件				hy.serializer.writer
* @作者				钱浩宇
* @创建时间			2021-11-30
* @更新时间			2022-11-30
* @摘要
* 汉念编译器生成字节码的序列化实现
*
**************************************************/

#include "hy.serializer.writer.h"
#include "../public/hy.strings.h"

namespace hy::serialize {
	inline Size Calculate(const StringView sv) noexcept {
		return sizeof(Size32) + sv.size() * sizeof(Char);
	}

	template<copyable_memory T>
	inline Size Calculate(util::Array<T>& arr) noexcept {
		return sizeof(Size32) + arr.size() * sizeof(T);
	}

	template<copyable_memory T>
	struct BytesWrapper {
		T const* data;
		Size size;
		inline constexpr BytesWrapper(T const* mem, Size len) noexcept : data{ mem }, size{ len * sizeof(T) } {}
	};

	inline Byte* WriteImpl(Byte* data, LiteralType v) noexcept {
		*data = static_cast<Byte>(v);
		return data + 1U;
	}

	template<unsigned_integral T>
	inline Byte* WriteImpl(Byte* data, T v) noexcept {
		if constexpr (!freestanding::endian::is_standard_endian)
			v = freestanding::endian::standard_endian(v);
		freestanding::copy(data, &v, sizeof(T));
		return data + sizeof(T);
	}

	inline Byte* WriteImpl(Byte* data, Float64 v) noexcept {
		freestanding::copy(data, &v, sizeof(Float64));
		return data + sizeof(Float64);
	}

	template<Size N>
	inline Byte* WriteImpl(Byte* data, Byte(&v)[N]) noexcept {
		freestanding::copy(data, v, N * sizeof(Byte));
		return data + N * sizeof(Byte);
	}

	inline Byte* WriteImpl(Byte* data, const StringView v) noexcept {
		data = WriteImpl(data, static_cast<Size32>(v.size()));
		if constexpr (freestanding::endian::is_standard_endian) {
			auto bs{ v.size() * sizeof(Char) };
			freestanding::copy(data, v.data(), bs);
			data += bs;
		}
		else {
			for (auto i : v) data = WriteImpl(data, i);
		}
		return data;
	}

	inline Byte* WriteImpl(Byte* data, util::Array<Index32>& arr) noexcept {
		data = WriteImpl(data, static_cast<Size32>(arr.size()));
		if constexpr (freestanding::endian::is_standard_endian) {
			auto bs{ arr.size() * sizeof(Index32) };
			freestanding::copy(data, arr.data(), bs);
			data += bs;
		}
		else {
			for (auto i : arr) data = WriteImpl(data, i);
		}
		return data;
	}

	inline Byte* WriteImpl(Byte* data, util::Array<Float64>& arr) noexcept {
		data = WriteImpl(data, static_cast<Size32>(arr.size()));
		auto bs{ arr.size() * sizeof(Float64) };
		freestanding::copy(data, arr.data(), bs);
		data += bs;
		return data;
	}

	template<typename T>
	inline Byte* WriteImpl(Byte* data, BytesWrapper<T> wrapper) noexcept {
		freestanding::copy(data, wrapper.data, wrapper.size);
		return data + wrapper.size;
	}

	template<typename T, typename... Args>
	inline Byte* Write(Byte* data, T&& arg, Args&&... args) noexcept {
		data = WriteImpl(data, freestanding::forward<T>(arg));
		if constexpr (sizeof...(Args) > 0ULL) data = Write(data, freestanding::forward<Args>(args)...);
		return data;
	}
}

// 区域1 - 字节码头区
namespace hy::serialize {
	// 计算字节码头区大小
	inline consteval Size CalcBCHeader() noexcept {
		return sizeof(BCHeader);
	}

	// 字节码头区序列化
	inline Byte* WriteBCHeader(Byte* data, CompilerConfig& config) noexcept {
		BCHeader header{ };
		freestanding::bit_set<0x20U, 0x01U, 0x11U, 0x05U>(header.magic);
		header.hash = 0U;
		header.hashCount = 0ULL;
		freestanding::copy(header.version, strings::VERSION_ID, freestanding::size(header.version));
		freestanding::copy(header.minVer, config.minVer, freestanding::size(header.minVer));
		freestanding::copy(header.maxVer, config.maxVer, freestanding::size(header.maxVer));
		header.platform.os = PLATFORM_TYPE;
		header.platform.extra1 = header.platform.extra2 = header.platform.extra3 = 0U;
		freestanding::initialize_n(header.unused, 0, freestanding::size(header.unused));

		/*
			| 标识(4B) | 字节码哈希值(4B) | 字节码哈希长度(8B) |
			| 版本号(4B) | 最低虚拟机版本(4B) | 最高虚拟机版本(4B) |
			| 平台(4B) | 未用(16B) |
		*/
		if constexpr (freestanding::endian::is_standard_endian) {
			return Write(data, BytesWrapper<BCHeader>(&header, 1ULL));
		}
		else {
			return Write(data, header.magic, header.hash, header.hashCount,
				header.version, header.minVer, header.maxVer,
				header.platform, header.unused);
		}
	}
}

// 区域2 - 字面值段
namespace hy::serialize {
	// 计算指令块大小
	inline Size CalcInsBlock(InsBlock& insBlock) noexcept {
		auto size{ sizeof(Size) + insBlock.cps.size() * (sizeof(CheckPoint::insStart) +
			sizeof(CheckPoint::line) + sizeof(CheckPoint::insOffset) + sizeof(Size16)) };
		for (auto& checkpoint : insBlock.cps)
			size += sizeof(Index16) * checkpoint.callOffset.size();
		size += sizeof(Size) + sizeof(Ins) * insBlock.iset.size();
		return size;
	}

	// 指令块序列化
	inline Byte* WriteInsBlock(Byte* data, InsBlock& insBlock) noexcept {
		/*
			| 检查点数量(4B) | { 指令起始(8B) | 指令所在行(4B) | 指令偏移(2B) |
				指令调用数(2B) | { 指令调用偏移(...) } } | 指令数(8B) | { 指令 } |
		*/
		data = Write(data, insBlock.cps.size());
		for (auto& checkpoint : insBlock.cps) {
			data = Write(data, checkpoint.insStart, checkpoint.line, checkpoint.insOffset,
				static_cast<Index16>(checkpoint.callOffset.size()));
			if constexpr (freestanding::endian::is_standard_endian) {
				data = Write(data, BytesWrapper(checkpoint.callOffset.data(), checkpoint.callOffset.size()));
			}
			else {
				for (Index16& call_offset : checkpoint.callOffset) data = Write(data, call_offset);
			}
		}
		data = Write(data, insBlock.iset.size());
		if constexpr (freestanding::endian::is_standard_endian) {
			data = Write(data, BytesWrapper(insBlock.iset.data(), insBlock.iset.size()));
		}
		else {
			for (auto& ins : insBlock.iset) data = Write(data, ins.as());
		}
		return data;
	}

	// 计算函数结构大小
	inline Size CalcFunctionStruct(__Function& func) noexcept {
		return sizeof(Size32) * 4ULL + CalcInsBlock(func.insFunc);
	}

	// 函数结构序列化
	inline Byte* WriteFunctionStruct(Byte* data, __Function& func) noexcept {
		/*
			| 函数名(4B) | 函数参数(4B) | 函数返回值(4B) | 函数是否变参(4B) | { 指令块 } |
		*/
		data = Write(data, func.index_name, func.index_targs, func.index_ret, func.va_targs);
		data = WriteInsBlock(data, func.insFunc);
		return data;
	}

	// 计算类结构大小
	inline Size CalcClassStruct(__Class& cls) noexcept {
		return sizeof(__Class::index_name) + sizeof(Size32) * 2ULL +
			cls.mvs.size() * sizeof(__Class::MemberVariable) +
			cls.mfs.size() * sizeof(Index32);
	}

	// 类结构序列化
	inline Byte* WriteClassStruct(Byte* data, __Class& cls) noexcept {
		/*
			| 类名(4B) |
			| 成员变量数目(4B) | { 类型(4B) | 变量名表(4B) } |
			| 成员函数数目(4B) | { 函数(4B) } |
		*/
		data = Write(data, cls.index_name);
		data = Write(data, static_cast<Size32>(cls.mvs.size()));
		if constexpr (freestanding::endian::is_standard_endian) {
			auto bs{ cls.mvs.size() * sizeof(__Class::MemberVariable) };
			freestanding::copy(data, cls.mvs.data(), bs);
			data += bs;
		}
		else {
			for (auto& [index_type, index_names] : cls.mvs)
				data = Write(data, index_type, index_names);
		}
		data = Write(data, static_cast<Size32>(cls.mfs.size()));
		if constexpr (freestanding::endian::is_standard_endian) {
			auto bs{ cls.mfs.size() * sizeof(Index32) };
			freestanding::copy(data, cls.mfs.data(), bs);
			data += bs;
		}
		else {
			for (auto& index_func : cls.mfs) data = Write(data, index_func);
		}
		return data;
	}

	// 计算概念结构大小
	inline Size CalcConceptStruct(__Concept& cpt) noexcept {
		return sizeof(cpt.index_name) + sizeof(Size32) + cpt.subs.size() * sizeof(SubConceptElement);
	}

	// 概念结构序列化
	inline Byte* WriteConceptStruct(Byte* data, __Concept& cpt) noexcept {
		/*    | 概念名(4B) | 子概念数目(4B) | { 子概念类型(4B) | 参数(4B) } |    */
		data = Write(data, cpt.index_name, static_cast<Size32>(cpt.subs.size()));
		if constexpr (freestanding::endian::is_standard_endian) {
			data = Write(data, BytesWrapper(cpt.subs.data(), cpt.subs.size()));
		}
		else {
			for (auto& sub : cpt.subs) data = Write(data, static_cast<Token>(sub.type), sub.arg);
		}
		return data;
	}

	// 计算字面值段大小
	inline Size CalcBCSection(LiteralPool& pool) noexcept {
		auto size{ sizeof(Size32) + pool.count() * sizeof(LiteralType) };
		for (auto& t : pool) {
			switch (t.type) {
			case LiteralType::INT: size += sizeof(Int64); break;
			case LiteralType::FLOAT: size += sizeof(Float64); break;
			case LiteralType::COMPLEX: size += sizeof(__Complex); break;
			case LiteralType::INDEXS: size += Calculate(*t.v.vIndexs); break;
			case LiteralType::STRING: size += Calculate(*t.v.vString); break;
			case LiteralType::REF: size += Calculate(t.v.vRef->data); break;
			case LiteralType::VECTOR: size += Calculate(*t.v.vVector); break;
			case LiteralType::MATRIX: size += sizeof(Size32) + Calculate(*t.v.vMatrix); break; // row, col | size
			case LiteralType::RANGE: size += sizeof(__Range); break;
			case LiteralType::FUNCTION: size += CalcFunctionStruct(*t.v.vFunction); break;
			case LiteralType::CLASS: size += CalcClassStruct(*t.v.vClass); break;
			case LiteralType::CONCEPT: size += CalcConceptStruct(*t.v.vConcept); break;
			}
		}
		return size;
	}

	// 字面值段序列化
	inline Byte* WriteBCSection(Byte* data, LiteralPool& pool) noexcept {
		/*
			| 字面值数量(4B) | { 字面值类型(1B) | 字面值(...) } |
		*/
		data = Write(data, pool.count());
		for (auto& t : pool) {
			data = Write(data, t.type);
			switch (t.type) {
			case LiteralType::INT: data = Write(data, static_cast<Uint64>(t.v.vInt)); break;
			case LiteralType::FLOAT: data = Write(data, t.v.vFloat); break;
			case LiteralType::COMPLEX: data = Write(data, t.v.vComplex.re, t.v.vComplex.im); break;
			case LiteralType::INDEXS: data = Write(data, *t.v.vIndexs); break;
			case LiteralType::STRING: data = Write(data, StringView(*t.v.vString)); break;
			case LiteralType::REF: data = Write(data, StringView(t.v.vRef->data)); break;
			case LiteralType::VECTOR: data = Write(data, *t.v.vVector); break;
			case LiteralType::MATRIX: data = Write(data, t.v.vMatrix->mRow, t.v.vMatrix->mCol,
				BytesWrapper(t.v.vMatrix->data(), t.v.vMatrix->size())); break;
			case LiteralType::RANGE: data = Write(data, static_cast<Uint64>(t.v.vRange->start),
				static_cast<Uint64>(t.v.vRange->step), static_cast<Uint64>(t.v.vRange->end)); break;
			case LiteralType::FUNCTION: data = WriteFunctionStruct(data, *t.v.vFunction); break;
			case LiteralType::CLASS: data = WriteClassStruct(data, *t.v.vClass); break;
			case LiteralType::CONCEPT: data = WriteConceptStruct(data, *t.v.vConcept); break;
			}
		}
		return data;
	}
}

// 区域3 - 主代码区
namespace hy::serialize {
	// 计算主代码区大小
	inline Size CalcBCCode(InsBlock& insBlock) noexcept {
		return CalcInsBlock(insBlock);
	}

	// 主代码区序列化
	inline Byte* WriteBCCode(Byte* data, InsBlock& insBlock) noexcept {
		return WriteInsBlock(data, insBlock);
	}
}

// 区域4 - 调试信息区
namespace hy::serialize {
	// 计算调试信息区大小
	inline Size CalcBCDebug(CompilerConfig& config, const StringView source) noexcept {
		if (config.debugMode) return Calculate(source);
		return sizeof(Size32);
	}

	// 调试信息区序列化
	inline Byte* WriteBCDebug(Byte* data, CompilerConfig& config, const StringView source) noexcept {
		/*    | 调试信息长度(4B) | 调试数据(...) |    */
		if (config.debugMode) {
			data = Write(data, source);
		}
		else {
			data = Write(data, 0U);
		}
		return data;
	}
}

// 区域5 - 资源区
namespace hy::serialize {
	// 计算资源区大小
	inline Size CalcBCResource(ResourceSet& resMap) noexcept {
		auto size{ sizeof(Size32) + resMap.size() * sizeof(Size32) }; // 数量, 资源数据长度数组
		for (auto& [name, res] : resMap) size += Calculate(name) + res.size();
		return size;
	}

	// 资源区序列化
	inline Byte* WriteBCResource(Byte* data, ResourceSet& resMap) noexcept {
		/*    | 资源数量(4B) | { 资源名长度(4B) | 资源名(...) | 资源数据长度(4B) | 资源数据(...) } |    */
		data = Write(data, static_cast<Size32>(resMap.size()));
		for (auto& [name, res] : resMap) {
			data = Write(data, name, static_cast<Size32>(res.size()), BytesWrapper(res.data(), res.size()));
		}
		return data;
	}
}

namespace hy::serialize {
#define TEST_BYTECODE 0

	// 字节码序列化
#if TEST_BYTECODE
#pragma optimize("", off)
#endif
	void WriteByteCode(CompileTable& table, util::ByteArray& ba, CompilerConfig& config, const StringView source) noexcept {
		auto totalSize{
			CalcBCHeader() + CalcBCSection(table.pool) +
			CalcBCCode(table.mainCode) + CalcBCDebug(config, source) +
			CalcBCResource(config.resMap) };
		ba.resize(totalSize);
		Byte* dataBCHeader{ ba.data() };
		Byte* dataBCSection{ WriteBCHeader(dataBCHeader, config) };
		Byte* dataBCCode{ WriteBCSection(dataBCSection, table.pool) };
		Byte* dataBCDebug{ WriteBCCode(dataBCCode, table.mainCode) };
		Byte* dataBCResource{ WriteBCDebug(dataBCDebug, config, source) };
		Byte* dataEnd{ WriteBCResource(dataBCResource, config.resMap) };
		// 字节码哈希值只计算字面值段及主代码区的二进制数据
		Byte* dataHash{ dataBCHeader + sizeof(BCHeader::magic) };
		Byte* dataHashCount{ dataHash + sizeof(BCHeader::hash) };
		Write(dataHash, freestanding::cvt::hash_bytes<Size32>(dataBCSection, dataBCDebug));
		Write(dataHashCount, static_cast<Size>(dataBCDebug - dataBCSection));
		// make sure to 'dataEnd' equals 'dataBCHeader' plus 'totalSize'
		(void)(dataBCHeader + totalSize == dataEnd);
	}
#if TEST_BYTECODE
#pragma optimize("", on)
#endif
}