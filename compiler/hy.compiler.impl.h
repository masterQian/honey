/**************************************************
*
* @文件				hy.compiler.impl
* @作者				钱浩宇
* @创建时间			2021-11-05
* @更新时间			2022-11-29
* @摘要
* 编译字节码的预处理表的实现
*
**************************************************/

#pragma once

#include "../public/hy.util.h"
#include "../compiler/hy.compiler.ins.h"
#include "../serializer/hy.serializer.bctype.h"

namespace hy {
	// 检查点
	struct CheckPoint {
		Index insStart; // 指令起始
		Index32 line; // 所在行
		Index16 insOffset; // 指令尾偏移量
		Vector<Index16> callOffset; // 函数调用指令偏移表
		CheckPoint(Index _is, Index32 _line) noexcept : insStart{ _is }, line{ _line }, insOffset{ } {}
	};

	// 指令块
	struct InsBlock {
		Vector<Ins> iset;	// 指令集
		Vector<CheckPoint> cps; // 检查点集

		Size size() const noexcept {
			return iset.size();
		}

		Index check_start(Index32 line) noexcept {
			auto index{ cps.size() };
			cps.emplace_back(iset.size(), line);
			return index;
		}

		void check_end(Index index) noexcept {
			auto& cp{ cps[index] };
			cp.insOffset = static_cast<Index16>(iset.size() - cp.insStart - 1ULL);
		}

		void check_call() noexcept {
			auto& cp{ cps.back() };
			cp.callOffset.emplace_back(static_cast<Index16>(iset.size() - cp.insStart));
		}

		void push(InsType type, unsigned_integral auto v) noexcept {
			iset.emplace_back(type, v);
		}

		template<InsType type>
		void push() noexcept {
			iset.emplace_back(type);
		}

		template<InsType type>
		Index jump_start() noexcept {
			auto index{ iset.size() };
			iset.emplace_back(type);
			return index;
		}

		void jump_end(Index index) noexcept {
			iset[index].set(iset.size() - index);
		}

		Index jump_re_end() const noexcept {
			return iset.size();
		}

		template<InsType type>
		void jump_re_start(Index index) noexcept {
			iset.emplace_back(type, iset.size() - index);
		}

		void set_back_lv() noexcept {
			Ins& ins{ iset.back() };
			switch (ins.type) {
			case InsType::PUSH_SYMBOL: ins.type = InsType::PUSH_SYMBOL_LV; break;
			case InsType::PUSH_REF: ins.type = InsType::PUSH_REF_LV; break;
			case InsType::MEMBER: ins.type = InsType::MEMBER_LV; break;
			case InsType::INDEX: ins.type = InsType::INDEX_LV; break;
			}
		}
	};

	// Continue / Break 表
	struct LoopControlTable {
		Vector<Index> ct, bt;
	};

	// 循环环境
	struct LoopEnv : private Vector<LoopControlTable> {
		Size alloc;

		LoopEnv() noexcept : alloc{ } { }

		void push() noexcept {
			if (size() == alloc) emplace_back();
			LoopControlTable& lct{ at(alloc++) };
			lct.bt.clear();
			lct.ct.clear();
		}

		void pop() noexcept {
			--alloc;
		}

		Vector<Index>& topCT() noexcept {
			return at(alloc - 1ULL).ct;
		}

		Vector<Index>& topBT() noexcept {
			return at(alloc - 1ULL).bt;
		}

		Vector<Index>& indexBT(Index i) noexcept {
			if (i == 0ULL || alloc < i) return front().bt;
			else return at(alloc - i).bt;
		}

		void clear() noexcept {
			Vector<LoopControlTable>::clear();
			alloc = 0ULL;
		}
	};

	// 指令环境
	struct InsEnv {
		bool isTerminal;
		LoopEnv loopEnv;
		InsEnv() noexcept : isTerminal{ } {}
	};
}

namespace hy {
	// 复数
	struct __Complex {
		Float64 re, im;

		constexpr bool operator == (__Complex c) const noexcept {
			return re == c.re && im == c.im;
		}
	};

	struct __Complex_Hasher {
		Size operator() (__Complex c) const noexcept {
			std::hash<Float64> hasher;
			return hasher(c.re) ^ hasher(c.im);
		}
	};

	// 索引集
	using __Indexs = util::Array<Index32>;

	// 字串引用
	struct __Ref {
		String data;
		Size len{ };

		bool operator == (const __Ref& s) const noexcept {
			return data == s.data; // basic_string比较无视其中的\0, 取决于size()
		}
	};

	struct __Ref_Hasher {
		Size operator() (const __Ref& v) const noexcept {
			return std::hash<String>{}(v.data);
		}
	};

	// 向量
	struct __Vector : public util::Array<Float64> {
		constexpr explicit __Vector(Size size) noexcept : Array<Float64>(size) {}
	};

	// 矩阵
	struct __Matrix : public util::Array<Float64> {
		Size32 mRow, mCol;
		constexpr __Matrix(Size32 row, Size32 col) noexcept :
			mRow(row), mCol(col), Array<Float64>(static_cast<Size>(row)* static_cast<Size>(col)) {}
	};

	// 范围
	struct __Range {
		Int64 start, step, end;

		constexpr bool operator == (const __Range& r) const noexcept {
			return start == r.start && step == r.step && end == r.end;
		}
	};

	struct __Range_Hasher {
		Size operator() (const __Range& r) const noexcept {
			std::hash<Int64> hasher;
			return hasher(r.start) ^ hasher(r.step) ^ hasher(r.end);
		}
	};

	// 函数
	struct __Function {
		Index32 index_name; // 函数名
		Index32 index_ret; // 返回值类型
		Index32 va_targs; // 是否变长参数
		Index32 index_targs; // 参数列表
		InsBlock insFunc; // 函数体
	};

	// 类
	struct __Class {
		struct MemberVariable {
			Index32 index_type; // 类型
			Index32 index_names; // 变量名
		};

		Index32 index_name; // 类名
		Vector<MemberVariable> mvs; // 成员变量
		Vector<Index32> mfs; // 成员函数
	};

	// 概念
	struct __Concept {
		Index32 index_name; // 概念名
		Vector<SubConceptElement> subs; // 子概念
	};

	// 常值符号
	struct ConstLiteral {
		LiteralType type;

		union Data {
			Int64 vInt; Float64 vFloat; __Complex vComplex;
			__Indexs* vIndexs; String* vString; __Ref* vRef;
			__Vector* vVector; __Matrix* vMatrix; __Range* vRange;
			__Function* vFunction; __Class* vClass; __Concept* vConcept;
		}v;
	};

	// 符号去重池
	template<typename Key, typename Hasher = std::hash<Key>>
	using Pool = HashMap<Key, Index32, Hasher>;

	// 常值池
	struct LiteralPool : protected Vector<ConstLiteral> {
		Pool<Int64> IntPool;
		Pool<Float64> FloatPool;
		Pool<__Complex, __Complex_Hasher> ComplexPool;
		util::StringMap<Index32> StringPool;
		Pool<__Ref, __Ref_Hasher> RefPool;
		Pool<__Range, __Range_Hasher> RangePool;

		LiteralPool() noexcept = default;

		~LiteralPool() noexcept {
			for (auto& i : *this) {
				switch (i.type) {
				case LiteralType::INDEXS: delete i.v.vIndexs; break;
				case LiteralType::STRING: delete i.v.vString; break;
				case LiteralType::REF: delete i.v.vRef; break;
				case LiteralType::VECTOR: delete i.v.vVector; break;
				case LiteralType::MATRIX: delete i.v.vMatrix; break;
				case LiteralType::RANGE: delete i.v.vRange; break;
				case LiteralType::FUNCTION: delete i.v.vFunction; break;
				case LiteralType::CLASS: delete i.v.vClass; break;
				case LiteralType::CONCEPT: delete i.v.vConcept; break;
				}
			}
		}

		Size32 count() const noexcept {
			return static_cast<Size32>(size());
		}

		const ConstLiteral* begin() const noexcept {
			return data();
		}

		const ConstLiteral* end() const noexcept {
			return data() + size();
		}

		Index32 get(Int64 r) noexcept {
			if (auto iter{ IntPool.find(r) }; iter != IntPool.cend())
				return iter->second;
			auto index{ count() };
			IntPool.try_emplace(r, index);
			auto& cl{ emplace_back() };
			cl.type = LiteralType::INT;
			cl.v.vInt = r;
			return index;
		}

		Index32 get(Float64 r) noexcept {
			if (auto iter{ FloatPool.find(r) }; iter != FloatPool.cend())
				return iter->second;
			auto index{ count() };
			FloatPool.try_emplace(r, index);
			auto& cl{ emplace_back() };
			cl.type = LiteralType::FLOAT;
			cl.v.vFloat = r;
			return index;
		}

		Index32 get(__Complex r) noexcept {
			if (auto iter{ ComplexPool.find(r) }; iter != ComplexPool.cend())
				return iter->second;
			auto index{ count() };
			ComplexPool.try_emplace(r, index);
			auto& cl{ emplace_back() };
			cl.type = LiteralType::COMPLEX;
			cl.v.vComplex = r;
			return index;
		}

		Index32 get(__Indexs&& r) noexcept {
			auto index{ count() };
			auto& cl{ emplace_back() };
			cl.type = LiteralType::INDEXS;
			cl.v.vIndexs = new __Indexs(freestanding::move(r));
			return index;
		}

		Index32 get(const StringView r) noexcept {
			if (auto item{ StringPool.get(r) }) return *item;
			auto index{ count() };
			StringPool.set(r, index);
			auto& cl{ emplace_back() };
			cl.type = LiteralType::STRING;
			cl.v.vString = new String(r);
			return index;
		}

		Index32 get(String&& r) noexcept {
			if (auto item{ StringPool.get(r) }) return *item;
			auto index{ count() };
			StringPool.set(r, index);
			auto& cl{ emplace_back() };
			cl.type = LiteralType::STRING;
			cl.v.vString = new String(freestanding::move(r));
			return index;
		}

		Index32 get(__Ref& r) noexcept {
			if (auto iter{ RefPool.find(r) }; iter != RefPool.cend())
				return iter->second;
			auto index{ count() };
			RefPool.try_emplace(r, index);
			auto& cl{ emplace_back() };
			cl.type = LiteralType::REF;
			cl.v.vRef = new __Ref(r);
			return index;
		}

		Index32 get(__Ref&& r) noexcept {
			if (auto iter{ RefPool.find(r) }; iter != RefPool.cend())
				return iter->second;
			auto index{ count() };
			RefPool.try_emplace(r, index);
			auto& cl{ emplace_back() };
			cl.type = LiteralType::REF;
			cl.v.vRef = new __Ref(freestanding::move(r));
			return index;
		}

		Index32 get(__Vector&& r) noexcept {
			auto index{ count() };
			auto& cl{ emplace_back() };
			cl.type = LiteralType::VECTOR;
			cl.v.vVector = new __Vector(freestanding::move(r));
			return index;
		}

		Index32 get(__Matrix&& r) noexcept {
			auto index{ count() };
			auto& cl{ emplace_back() };
			cl.type = LiteralType::MATRIX;
			cl.v.vMatrix = new __Matrix(freestanding::move(r));
			return index;
		}

		Index32 get(const __Range& r) noexcept {
			if (auto iter{ RangePool.find(r) }; iter != RangePool.cend())
				return iter->second;
			auto index{ count() };
			RangePool.try_emplace(r, index);
			auto& cl{ emplace_back() };
			cl.type = LiteralType::RANGE;
			cl.v.vRange = new __Range(r);
			return index;
		}

		Index32 get(__Function&& r) noexcept {
			auto index{ count() };
			auto& cl{ emplace_back() };
			cl.type = LiteralType::FUNCTION;
			cl.v.vFunction = new __Function(freestanding::move(r));
			return index;
		}

		Index32 get(__Class&& r) noexcept {
			auto index{ count() };
			auto& cl{ emplace_back() };
			cl.type = LiteralType::CLASS;
			cl.v.vClass = new __Class(freestanding::move(r));
			return index;
		}

		Index32 get(__Concept&& r) noexcept {
			auto index{ count() };
			auto& cl{ emplace_back() };
			cl.type = LiteralType::CONCEPT;
			cl.v.vConcept = new __Concept(freestanding::move(r));
			return index;
		}

		// [unsafe] 使用fetch得到的对象使用时要保证期间ConstLiteral没有扩容
		template<LiteralType t>
		decltype(auto) fetch(Index32 index) noexcept {
			auto& cl{ this->operator[](index) };
			if constexpr (t == LiteralType::INT) return cl.v.vInt;
			else if constexpr (t == LiteralType::FLOAT) return cl.v.vFloat;
			else if constexpr (t == LiteralType::COMPLEX) return cl.v.vComplex;
			else if constexpr (t == LiteralType::INDEXS) return *cl.v.vIndexs;
			else if constexpr (t == LiteralType::STRING) return *cl.v.vString;
			else if constexpr (t == LiteralType::REF) return *cl.v.vRef;
			else if constexpr (t == LiteralType::VECTOR) return *cl.v.vVector;
			else if constexpr (t == LiteralType::MATRIX) return *cl.v.vMatrix;
			else if constexpr (t == LiteralType::RANGE) return *cl.v.vRange;
			else if constexpr (t == LiteralType::FUNCTION) return *cl.v.vFunction;
			else if constexpr (t == LiteralType::CLASS) return *cl.v.vClass;
			else if constexpr (t == LiteralType::CONCEPT) return *cl.v.vConcept;
			else static_assert(!same<decltype(t), LiteralType>, "error type");
		}
	};

	// 编译预处理表
	struct CompileTable {
		LiteralPool pool;
		InsBlock mainCode;
	};
}