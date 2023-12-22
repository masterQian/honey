/**************************************************
*
* @文件				hy.serializer.reader
* @作者				钱浩宇
* @创建时间			2021-12-11
* @更新时间			2022-12-11
* @摘要
* 汉念编译器生成字节码的反序列化实现
*
**************************************************/

#include "hy.serializer.reader.h"

namespace hy::serialize {
	inline Byte* ReadImpl(Byte* data, LiteralType& lt) noexcept {
		lt = static_cast<LiteralType>(*data);
		return data + 1ULL;
	}

	template<unsigned_integral T>
	inline Byte* ReadImpl(Byte* data, T& t) noexcept {
		freestanding::copy(&t, data, sizeof(T));
		if constexpr (!freestanding::endian::is_standard_endian) {
			t = freestanding::endian::standard_endian(t);
		}
		return data + sizeof(T);
	}

	inline Byte* ReadImpl(Byte* data, Float64& t) noexcept {
		freestanding::copy(&t, data, sizeof(Float64));
		return data + sizeof(Float64);
	}

	template<typename T, typename S>
	inline Byte* ReadImpl(Byte* data, util::ArrayView<T, S>& view) noexcept {
		data = ReadImpl(data, view.mSize);
		view.mView = reinterpret_cast<T*>(data);
		if constexpr (!freestanding::endian::is_standard_endian) {
			if constexpr (unsigned_integral<T>) {
				for (T& v : view) v = freestanding::endian::standard_endian(v);
			}
		}
		return data + view.mSize * sizeof(T);
	}

	template<typename T, typename S>
	inline Byte* ReadImpl(Byte* data, util::DArrayView<T, S>& view) noexcept {
		data = ReadImpl(data, view.mRow);
		data = ReadImpl(data, view.mCol);
		view.mView = reinterpret_cast<T*>(data);
		return data + view.mRow * view.mCol * sizeof(T);
	}

	template<unsigned_integral T, Size32 len>
	inline Byte* ReadImpl(Byte* data, util::FixedArrayView<T, Size32, len>& view) noexcept {
		view.reset(reinterpret_cast<T*>(data));
		if constexpr (!freestanding::endian::is_standard_endian) {
			for (T& v : view) v = freestanding::endian::standard_endian(v);
		}
		return data + len * sizeof(T);
	}

	template<typename T>
	requires (sizeof(T) > sizeof(Memory))
	inline Byte* ReadImpl(Byte* data, T*& t) noexcept {
		t = reinterpret_cast<T*>(data);
		return data + sizeof(T);
	}

	template<typename T, typename... Args>
	inline Byte* Read(Byte* data, T& arg, Args&... args) noexcept {
		data = ReadImpl(data, arg);
		if constexpr (sizeof...(Args) > 0ULL) data = Read(data, args...);
		return data;
	}
}

// 区域1 - 字节码头区
namespace hy::serialize {
	// 字节码头区反序列化
	inline Byte* ReadBCHeader(Byte* data, ByteCode& bc) noexcept {
		data = Read(data, bc.pHeader);
		if constexpr (!freestanding::endian::is_standard_endian) {
			bc.pHeader->hash = freestanding::endian::standard_endian(bc.pHeader->hash);
		}
		return data;
	}
}

// 区域2 - 字面值段
namespace hy::serialize {
	// 指令视图反序列化
	inline Byte* ReadInsView(Byte* data, InsView& view) noexcept {
		Size count;
		data = Read(data, count);
		view.cpViews.resize(count);
		for (auto& cpv : view.cpViews) {
			data = Read(data, cpv.insStart, cpv.line, cpv.insOffset, cpv.sizeCallOffset);
			cpv.dataCallOffset = reinterpret_cast<Index16*>(data);
			if constexpr (!freestanding::endian::is_standard_endian) {
				for (Index16 i{ }; i < cpv.sizeCallOffset; ++i) {
					cpv.dataCallOffset[i] = freestanding::endian::standard_endian(cpv.dataCallOffset[i]);
				}
			}
			data += cpv.sizeCallOffset * sizeof(Index16);
		}
		data = Read(data, view.insView);
		if constexpr (!freestanding::endian::is_standard_endian) {
			for (auto begin = reinterpret_cast<Uint32*>(view.insView.begin()),
				end = reinterpret_cast<Uint32*>(view.insView.end()); begin != end; ++begin)
				*begin = freestanding::endian::standard_endian(*begin);
		}
		return data;
	}

	// 字面值段反序列化
	inline Byte* ReadBCSection(Byte* data, LiteralSection& values) noexcept {
		Size32 count;
		data = Read(data, count);
		values.resize(count);
		for (auto& value : values) {
			data = Read(data, value.type);
			switch (value.type) {
			case LiteralType::INT: data = Read(data, value.v.vInt); break;
			case LiteralType::FLOAT: data = Read(data, value.v.vFloat); break;
			case LiteralType::COMPLEX: data = Read(data, value.v.vComplex.re, value.v.vComplex.im); break;
			case LiteralType::INDEXS: data = Read(data, value.v.vIndexs); break;
			case LiteralType::STRING:
			case LiteralType::REF: data = Read(data, value.v.vString); break;
			case LiteralType::VECTOR: data = Read(data, value.v.vVector); break;
			case LiteralType::MATRIX: data = Read(data, value.v.vMatrix); break;
			case LiteralType::RANGE: data = Read(data, value.v.vRange); break;
			case LiteralType::FUNCTION: {
				value.v.vFunction = new FunctionView;
				data = Read(data, value.v.vFunction->pre);
				data = ReadInsView(data, value.v.vFunction->insView);
				break;
			}
			case LiteralType::CLASS: {
				value.v.vClass = new ClassView;
				data = Read(data, value.v.vClass->index_name, value.v.vClass->index_mv,
					value.v.vClass->index_mf);
				if constexpr (!freestanding::endian::is_standard_endian) {
					for (auto& mv : value.v.vClass->index_mv) {
						mv.index_type = freestanding::endian::standard_endian(mv.index_type);
						mv.index_names = freestanding::endian::standard_endian(mv.index_names);
					}
				}
				break;
			}
			case LiteralType::CONCEPT: {
				value.v.vConcept = new ConceptView;
				data = Read(data, value.v.vConcept->index_name, value.v.vConcept->subView);
				if constexpr (!freestanding::endian::is_standard_endian) {
					for (auto& name : value.v.vConcept->subView) {
						name.type = static_cast<SubConceptElementType>(freestanding::endian::standard_endian(
							static_cast<Token>(name.type)));
						name.arg = freestanding::endian::standard_endian(name.arg);
					}
				}
				break;
			}
			}
		}
		return data;
	}
}

// 区域3 - 主代码区
namespace hy::serialize {
	// 主代码区反序列化
	inline Byte* ReadBCCode(Byte* data, InsView& view) noexcept {
		return ReadInsView(data, view);
	}
}

// 区域4 - 调试信息区
namespace hy::serialize {
	// 调试信息区反序列化
	inline Byte* ReadBCDebug(Byte* data, DebugView& view) noexcept {
		return Read(data, view.sourceCode);
	}
}

// 区域5 - 资源区
namespace hy::serialize {
	// 资源区反序列化
	inline Byte* ReadBCResource(Byte* data, ResourceMapView& resMap) noexcept {
		Size32 count, dataSize;
		data = Read(data, count);
		for (Index32 i{ }; i < count; ++i) {
			StrView name;
			data = Read(data, name, dataSize);
			resMap.set(StringView{ name.data(), name.size() }, data, dataSize);
			data += dataSize;
		}
		return data;
	}
}

namespace hy::serialize {
#define TEST_BYTECODE 0

#if TEST_BYTECODE
#pragma optimize("", off)
#endif
	// 字节码反序列化
	bool ReadByteCode(util::ByteArray&& ba, ByteCode& bc) noexcept {
		if (ba.size() >= ByteCode::MIN_SIZE) {
			bc.source = freestanding::move(ba);
			Byte* dataBCHeader{ bc.source.data() };
			Byte* dataBCSection{ ReadBCHeader(dataBCHeader, bc) };
			Byte* dataBCCode{ ReadBCSection(dataBCSection, bc.values) };
			Byte* dataBCDebug{ ReadBCCode(dataBCCode, bc.mainCode) };
			Byte* dataBCResource{ ReadBCDebug(dataBCDebug, bc.debugView) };
			Byte* dataEnd{ ReadBCResource(dataBCResource, bc.resMap) };
			// make sure to 'dataEnd' equals 'dataBCHeader' plus 'bc.source.size()'
			return dataBCHeader + bc.source.size() == dataEnd;
		}
		return false;
	}
#if TEST_BYTECODE
#pragma optimize("", on)
#endif

	bool ReadByteCode(util::ByteArray& ba, ByteCode& bc) noexcept {
		util::ByteArray tmp{ ba.data(), ba.size() };
		return ReadByteCode(freestanding::move(tmp), bc);
	}
}