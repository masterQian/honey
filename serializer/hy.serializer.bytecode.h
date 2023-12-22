/**************************************************
*
* @文件				hy.serializer.bytecode
* @作者				钱浩宇
* @创建时间			2021-11-05
* @更新时间			2022-11-29
* @摘要
* 包含了汉念语言字节码的组织定义形式
*
**************************************************/

#pragma once

#include "../public/hy.util.h"
#include "../compiler/hy.compiler.ins.h"
#include "../serializer/hy.serializer.bctype.h"

namespace hy {
	struct Complex64 { Float64 re, im; };

	using IndexsView = util::ArrayView<Index32, Size32>;

	using StrView = util::ArrayView<Char, Size32>;

	struct RefView : StrView {
		struct RefViewIterator {
			Str mData;

			explicit RefViewIterator(Str data) noexcept : mData{ data } {}

			bool operator == (const RefViewIterator& iterator) const noexcept {
				return mData == iterator.mData;
			}

			StrView operator *() const noexcept {
				auto p{ mData };
				while (*p) ++p;
				return StrView{ mData, static_cast<Size32>(p - mData) };
			}

			RefViewIterator& operator ++ () noexcept {
				while (*mData) ++mData;
				++mData;
				return *this;
			}
		};

		RefView() noexcept = default;

		RefView(const StringView sv) noexcept :
			StrView{ const_cast<Str>(sv.data()), static_cast<Size32>(sv.size()) + 1U } {}

		RefViewIterator begin() const noexcept {
			return RefViewIterator(mView);
		}

		RefViewIterator end() const noexcept {
			return RefViewIterator(mView + mSize);
		}

		template<Char ch>
		String toString() const noexcept {
			auto count{ static_cast<Size>(mSize - 1U) };
			String str{ mView, count };
			for (auto start{ str.data() }, end{ start + count }; start != end; ++start) {
				if (!*start) *start = ch;
			}
			return str;
		}

		Size count() const noexcept {
			auto i{ 0ULL };
			for (auto start{ mView }, end{ start + mSize }; start != end; ++start) {
				if (!*start) ++i;
			}
			return i;
		}

		StringView detach(RefView& rv) noexcept {
			rv = *this;
			auto start{ rv.mView }, end{ start + rv.mSize }, p{ end - 2 };
			while (*p) {
				if (p == start) {
					rv.mSize = 0;
					return p;
				}
				--p;
			}
			rv.mSize -= static_cast<Size32>(end - p - 1ULL);
			return p + 1;
		}
	};

	using VectorView = util::ArrayView<Float64, Size32>;

	using MatrixView = util::DArrayView<Float64, Size32>;

	using RangeView = util::FixedArrayView<Uint64, Size32, 3U>;

	struct CheckPointView {
		Index insStart;
		Index32 line;
		Index16 insOffset;
		Index16 sizeCallOffset;
		Index16* dataCallOffset;
	};

	struct InsView {
		util::Array<CheckPointView> cpViews;
		util::ArrayView<Ins, Size> insView;
		Index32 calcLine(const Ins* pos) noexcept {
			auto index{ static_cast<Index>(pos - insView.cbegin()) };
			for (auto& cpv : cpViews) {
				if (index >= cpv.insStart && index <= cpv.insStart + cpv.insOffset) return cpv.line;
			}
			return 0U;
		}
	};

	struct FunctionView {
		util::FixedArrayView<Index32, Size32, 4U> pre;
		InsView insView;

		Index32 index_name() const noexcept {
			return pre.mView[0];
		}

		Index32 index_targs() const noexcept {
			return pre.mView[1];
		}

		bool hasRet() const noexcept {
			return pre.mView[2] != INone;
		}

		Index32 index_ret() const noexcept {
			return pre.mView[2];
		}

		bool vaTargs() const noexcept {
			return pre.mView[3] != 0U;
		}
	};

	struct MemberVariable {
		Index32 index_type; // 类型
		Index32 index_names; // 变量名
	};

	struct ClassView {
		Index32 index_name; // 类名
		util::ArrayView<MemberVariable, Size32> index_mv; // 成员变量
		util::ArrayView<Index32, Size32> index_mf; // 成员函数
	};

	struct ConceptView {
		Index32 index_name; // 概念名
		util::ArrayView<SubConceptElement, Size32> subView; // 子概念
	};

	// 字面值
	struct LiteralView {
		LiteralType type;
		union LiteralValue {
			Uint64 vInt;
			Float64 vFloat;
			Complex64 vComplex;
			IndexsView vIndexs;
			StrView vString;
			RefView vRef;
			VectorView vVector;
			MatrixView vMatrix;
			RangeView vRange;
			FunctionView* vFunction;
			ClassView* vClass;
			ConceptView* vConcept;
		}v;
	};

	struct LiteralSection : util::Array<LiteralView> {
		~LiteralSection() noexcept {
			for (auto& lv : *this) {
				switch (lv.type) {
				case LiteralType::FUNCTION: delete lv.v.vFunction; break;
				case LiteralType::CLASS: delete lv.v.vClass; break;
				case LiteralType::CONCEPT: delete lv.v.vConcept; break;
				}
			}
		}

		Int64 getInt(Index32 index) const noexcept {
			return static_cast<Int64>(mData[index].v.vInt); 
		}

		Int64 getInt(Ins ins) const noexcept { 
			return static_cast<Int64>(mData[ins.get<Size32>()].v.vInt);
		}

		Float64 getFloat(Index32 index) const noexcept {
			return mData[index].v.vFloat;
		}

		Float64 getFloat(Ins ins) const noexcept { 
			return mData[ins.get<Size32>()].v.vFloat; 
		}

		Complex64 getComplex(Index32 index) const noexcept {
			return mData[index].v.vComplex;
		}

		Complex64 getComplex(Ins ins) const noexcept { 
			return mData[ins.get<Size32>()].v.vComplex; 
		}

		IndexsView getIndexs(Index32 index) const noexcept {
			return mData[index].v.vIndexs;
		}

		IndexsView getIndexs(Ins ins) const noexcept {
			return mData[ins.get<Size32>()].v.vIndexs;
		}

		StringView getString(Index32 index) const noexcept {
			StrView strView{ mData[index].v.vString };
			return StringView{ strView.data(), static_cast<Size>(strView.size()) };
		}

		StringView getString(Ins ins) const noexcept {
			StrView strView{ mData[ins.get<Size32>()].v.vString };
			return StringView{ strView.data(), static_cast<Size>(strView.size()) };
		}

		RefView getRef(Index32 index) const noexcept {
			return mData[index].v.vRef; 
		}

		RefView getRef(Ins ins) const noexcept {
			return mData[ins.get<Size32>()].v.vRef;
		}

		VectorView getVector(Ins ins) const noexcept { 
			return mData[ins.get<Size32>()].v.vVector;
		}

		MatrixView getMatrix(Ins ins) const noexcept { 
			return mData[ins.get<Size32>()].v.vMatrix;
		}

		RangeView getRange(Ins ins) const noexcept { 
			return mData[ins.get<Size32>()].v.vRange; 
		}

		FunctionView& getFunction(Index32 index) const noexcept {
			return *mData[index].v.vFunction; 
		}

		ClassView& getClass(Index32 index) const noexcept { 
			return *mData[index].v.vClass;
		}

		ConceptView& getConcept(Index32 index) const noexcept {
			return *mData[index].v.vConcept;
		}
	};

	struct DebugView {
		StrView sourceCode;

		StringView getSourceCode() const noexcept {
			return StringView{ sourceCode.data(), static_cast<Size>(sourceCode.size()) };
		}
	};

	using ResourceView = util::ArrayView<Byte, Size32>;
	using ResourceMapView = util::StringViewMap<ResourceView>;

	// 字节码
	struct ByteCode {
		constexpr static auto MIN_SIZE { sizeof(BCHeader) };

		util::ByteArray source;
		BCHeader* pHeader;
		LiteralSection values;
		InsView mainCode;
		DebugView debugView;
		ResourceMapView resMap;
	};
}