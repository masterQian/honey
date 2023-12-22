#pragma once

#include "../public/hy.error.h"
#include "../serializer/hy.serializer.bytecode.h"

namespace hy {
	enum class TypeId : Token {
		Type, Null, Int, Float, Complex,
		Bool, LV, Iterator, Function, MemberFunction,
		String, List, Map, Vector, Matrix,
		Range, Array, Bin, HashSet,
	};

	constexpr auto TypeIdBuiltinCount{ 19ULL };
	constexpr auto UserTypeIdStart{ 100ULL };

	constexpr auto UserClassFunctionString{ u"__string__" };
	constexpr auto UserClassFunctionHash{ u"__hash__" };
	constexpr auto UserClassFunctionLen{ u"__len__" };
	constexpr auto UserClassFunctionIndex{ u"__index__" };
	constexpr auto UserClassFunctionUnpack{ u"__unpack__" };
	constexpr auto UserClassFunctionConstruct{ u"__construct__" };
	constexpr auto UserClassFunctionWrite{ u"__write__" };

	constexpr auto UserClassFunctionAddAssign{ u"__addassign__" };
	constexpr auto UserClassFunctionSubAssign{ u"__subassign__" };
	constexpr auto UserClassFunctionMulAssign{ u"__mulassign__" };
	constexpr auto UserClassFunctionDivAssign{ u"__divassign__" };
	constexpr auto UserClassFunctionModAssign{ u"__modassign__" };

	constexpr auto UserClassFunctionNeg{ u"__neg__" };
	constexpr auto UserClassFunctionAdd{ u"__add__" };
	constexpr auto UserClassFunctionSub{ u"__sub__" };
	constexpr auto UserClassFunctionMul{ u"__mul__" };
	constexpr auto UserClassFunctionDiv{ u"__div__" };
	constexpr auto UserClassFunctionMod{ u"__mod__" };
	constexpr auto UserClassFunctionPow{ u"__pow__" };

	constexpr auto UserClassFunctionEqual{ u"__equal__" };
	constexpr auto UserClassFunctionGT{ u"__gt__" };
	constexpr auto UserClassFunctionGE{ u"__ge__" };
	constexpr auto UserClassFunctionLT{ u"__lt__" };
	constexpr auto UserClassFunctionLE{ u"__le__" };

	constexpr auto UserClassFunctionIterGet{ u"__iter_get__" };
	constexpr auto UserClassFunctionIterSave{ u"__iter_save__" };
	constexpr auto UserClassFunctionIterAdd{ u"__iter_add__" };
	constexpr auto UserClassFunctionIterCheck{ u"__iter_check__" };
}

namespace hy {
	// 虚拟机句柄指针
	// 提前引用的模块中使用VM时间接传递HVM
	using HVM = void*;

	// 模块指针
	// 提前引用的模块中使用Module时间接传递HMOD
	using HMOD = void*;

	struct TypeObject;
	struct LVObject;
	struct FunctionObject;
	struct IteratorObject;

	// 对象
	struct Object {
		TypeObject* type; // 类型
		Size lc; // 引用次数
		explicit Object(TypeObject* t) noexcept : type{ t }, lc{ } {}

		void link() noexcept { ++lc; }
		inline void unlink() noexcept;
	};

	// 形参
	// type是形参类型, 没有显式指出类型的形参其type为nullptr
	// name是形参的名称
	struct ObjTArg {
		TypeObject* type; // 可空
		StringView name;
	};

	// 形参列表
	// args是形参数组
	// va_args为真表示此形参列表支持变长参数
	struct ObjTArgs {
		util::Array<ObjTArg> args;
		bool va_args;
	};

	// 参数列表
	// 参数列表是一系列对象的数组
	using ObjArgs = util::Array<Object*>;

	using ObjArgsView = util::ArrayView<Object*, Size>;

	// 左值类型
	// ADDRESS: 左值是含有地址的标识符变量, 通常位于符号表或容器中
	// DATA: 左值是携带了指定数据的对象, 需要间接设置此变量的指定数据
	enum class LVType { ADDRESS, DATA };

	// 左值存储
	// LVType为ADDRESS时左值存储的address生效, DATA时data生效
	// address是左值对象的地址, 修改address将会直接影响到符号表或容器
	// data是携带数据, 左值对象根据data的操作类型id来决定操作数据ds
	// 若ds不够容纳数据, 动态分配后的左值对象通过f_free_lv_data来释放
	union LVStorage {
		Object** address;
		struct Data {
			Index id;
			Memory ds[4];
		}data;
	};

	// 迭代器存储
	struct IteratorStorage {
		Memory data[4];
	};

	// 函数类型
	// NORMAL是自定义的普通函数
	// NATIVE是由C++提供的原生函数
	enum class FunctionType { NORMAL, NATIVE };

	using NativeFunc = void (*)(HVM, ObjArgsView, Object*) noexcept;

	// 原生函数
	struct NativeFunction {
		// 原生函数FunctionObject*直接在VM中对参数列表操作
		// Object*是成员函数指针, 为空时表示普通函数
		NativeFunc func;
		// 是否检查形参
		bool checkArgs;
	};

	// 普通函数
	// hmod是函数所在模块句柄
	// ret是函数返回值类型
	// targs是函数的形参列表
	// insView是函数的指令块
	struct NormalFunction {
		HMOD hmod;
		TypeObject* ret;
		InsView* insView;
	};

	// 函数表
	// 函数表是字符串到函数对象的映射
	using FunctionTable = util::StringViewMap<FunctionObject*>;

	// 成员
	// 成员类型type非空, index指示了该类生成的对象中对应成员所在的内存位置索引
	struct Member {
		TypeObject* type;
		Index index;
	};

	// 类成员
	// 类成员是字符串到成员的映射
	using Members = util::StringViewMap<Member>;

	// 成员数据
	// 成员数据是依据索引来安排位置的对象数组
	using MembersData = util::Array<Object*>;

	// 类结构
	// funcs是成员函数, members是成员变量
	struct TypeClassStruct {
		FunctionTable funcs;
		Members members;
	};

	// 概念指令
	struct ConceptIns {
		SubConceptElementType sct;
		union {
			TypeObject* type;
			Index offset;
		}v;
	};

	// 概念结构
	using TypeConceptStruct = util::Array<ConceptIns>;

	struct TypeStaticData {
		TypeObject* builtinTypes[TypeIdBuiltinCount];
	};

	// 原型
	struct TypeObject : Object {
		// **** 类型名称 ****
		// [非空]
		String v_name;

		// **** 类型序列号 ****
		// [非空]
		// 内置类型的序列号是序号, 其他类型的序列号是名称对应的哈希值偏移一定值
		TypeId v_id;

		// **** 可变类型 ****
		// [非空]
		// 可变类型通常使用引用传递
		bool a_mutable;

		// **** 定义类型 ****
		// [非空]
		// 非定义类型不允许是类成员
		bool a_def;

		// **** 未用 ****
		bool a_unused1;
		bool a_unused2;

		// **** 类静态数据 ****
		Memory v_static;

		// **** 类结构 ****
		// [可空 : 非类类型]
		TypeClassStruct* v_cls;

		// **** 概念结构 ****
		// [可空 : 非概念类型]
		TypeConceptStruct* v_cps;

		// **** 创建类静态数据 ****
		// 创建整个类的静态共享数据
		// [可空 : 无静态数据] [无异常]
		// TypeObject* 对象原型
		void (*f_class_create)(TypeObject*) noexcept;

		// **** 释放类静态数据 ****
		// 释放整个类的静态共享数据
		// [可空 : 无静态数据] [无异常]
		// TypeObject* 对象原型
		void (*f_class_delete)(TypeObject*) noexcept;

		// **** 清理静态数据 ****
		// 清理整个类的静态共享数据
		// [可空 : 无静态数据 | 无需清理] [无异常]
		// TypeObject* 对象原型
		void (*f_class_clean)(TypeObject*) noexcept;

		// **** 分配对象 ****
		// 以指定内部参数从对象分配器中得到一个未引用的对象
		// [非空] [无异常]
		// TypeObject* 对象原型
		// Memory 参数1 视对象实现而定
		// Memory 参数2 视对象实现而定
		// return -> 分配的对象
		Object* (*f_allocate)(TypeObject*, Memory, Memory) noexcept;

		// **** 回收对象 ****
		// 将零引用的对象回收到对象分配器中
		// [非空] [无异常]
		// Object* 对象
		void (*f_deallocate)(Object*) noexcept;

		// **** 满足概念 ****
		// [可空 : 类型非概念] [无异常]
		// TypeObject* 概念
		// TypeObject* 接口实例类型
		// return -> 是否满足概念
		bool (*f_implement)(TypeObject*, TypeObject*) noexcept;

		// **** 浮点数表示 ****
		// 得到对象的float64表示
		// [可空 : 不支持转换] [无异常]
		// Object* 对象
		// return -> 浮点数值
		Float64(*f_float)(Object*) noexcept;

		// **** 逻辑值表示 ****
		// 得到对象的bool表示
		// [可空 : 不支持转换] [无异常]
		// Object* 对象
		// return -> 逻辑值
		bool (*f_bool)(Object*) noexcept;

		// **** 字符串表示 ****
		// 得到对象的字符串表示并添加到源串后
		// [非空] [异常]
		// HVM 虚拟机句柄
		// Object* 对象
		// String* 源串
		IResult<void>(*f_string)(HVM, Object*, String*) noexcept;

		// **** 哈希值 ****
		// [可空 : 不支持哈希] [异常]
		// HVM 虚拟机
		// Object* 对象
		// return -> 哈希值
		IResult<Size>(*f_hash)(HVM, Object*) noexcept;

		// **** 取长度 ****
		// [可空 : 不可取长度] [异常]
		// HVM 虚拟机
		// Object* 对象
		// return -> 长度
		IResult<Size>(*f_len)(HVM, Object*) noexcept;

		// **** 取成员 ****
		// 取对象的成员并压栈
		// [可空 : 不可取成员] [异常]
		// HVM 虚拟机句柄
		// bool 是否取左值成员
		// Object* 对象
		// const StringView 成员名
		IResult<void>(*f_member)(HVM, bool, Object*, const StringView) noexcept;

		// **** 取索引 ****
		// 取对象的索引元素并压栈
		// [可空 : 不可取索引] [异常]
		// HVM 虚拟机句柄
		// bool 是否取左值索引
		// Object* 对象
		// ObjArgsView 索引参数表
		IResult<void>(*f_index)(HVM, bool, Object*, ObjArgsView) noexcept;

		// **** 解包 ****
		// 解包对象并依次压栈
		// [可空 : 不支持解包] [异常]
		// HVM 虚拟机句柄
		// Object* 对象
		// Size 包长度
		IResult<void>(*f_unpack)(HVM, Object*, Size) noexcept;

		// **** 构造 ****
		// 构造新对象并压栈
		// [可空 : 不支持构造] [异常]
		// HVM 虚拟机句柄
		// TypeObject* 待构造的对象类型
		// ObjArgsView 构造参数列表
		IResult<void>(*f_construct)(HVM, TypeObject*, ObjArgsView) noexcept;

		// **** 全复制 ****
		// [可空 : 引用自身] [无异常]
		// Object* 对象
		// return -> 对象全拷贝
		Object* (*f_full_copy)(Object*) noexcept;

		// **** 复制 ****
		// [可空 : 不可变类 | 不支持拷贝] [无异常]
		// Object* 对象
		// return -> 对象拷贝
		Object* (*f_copy)(Object*) noexcept;

		// **** 写 ****
		// [可空 : 不支持写] [异常]
		// HVM 虚拟机句柄
		// Object* IO对象
		// ObjArgsView 对象数组
		IResult<void>(*f_write)(HVM, Object*, ObjArgsView) noexcept;

		// **** 扫描 ****
		// 从字符流扫描得到对象并压栈
		// [可空 : 不支持扫描] [异常]
		// HVM 虚拟机句柄
		// TypeObject* 待输入的对象类型
		// const StringView 扫描字符串
		// return -> 读入字符数
		IResult<Size>(*f_scan)(HVM, TypeObject*, const StringView) noexcept;

		// **** 运算赋值 ****
		// [可空 : 不支持运算赋值] [异常]
		// HVM 虚拟机句柄
		// LVObject* 左值对象
		// Object* 运算赋值对象
		// AssignType 运算赋值类型
		IResult<void>(*f_calcassign)(HVM, LVObject*, Object*, AssignType) noexcept;

		// **** 左值数据赋值 ****
		// [可空 : 不支持左值数据赋值] [异常]
		// HVM 虚拟机句柄
		// LVObject* 左值对象
		// Object* 赋值对象
		IResult<void>(*f_assign_lv_data)(HVM, LVObject*, Object*) noexcept;

		// **** 左值数据运算赋值 ****
		// [可空 : 不支持左值数据运算赋值] [异常]
		// HVM 虚拟机句柄
		// LVObject* 左值对象
		// Object* 运算赋值对象
		// AssignType 运算赋值类型
		IResult<void>(*f_calcassign_lv_data)(HVM, LVObject*, Object*, AssignType) noexcept;

		// **** 释放左值数据 ****
		// [可空 : 不支持左值数据运算赋值 | 无需释放左值数据] [无异常]
		// LVObject* 左值对象
		void (*f_free_lv_data)(LVObject*) noexcept;

		// **** 单操作数运算 ****
		// 单操作数运算后将结果压栈
		// [可空 : 不支持单操作数运算] [异常]
		// HVM 虚拟机句柄
		// Object* 操作数
		// SOPTType 运算符
		IResult<void>(*f_sopt_calc)(HVM, Object*, SOPTType) noexcept;

		// **** 双操作数运算 ****
		// 双操作数运算后将结果压栈
		// [可空 : 不支持双操作数运算] [异常]
		// HVM 虚拟机句柄
		// Object* 操作数1
		// Object* 操作数2
		// BOPTType 运算符
		IResult<void>(*f_bopt_calc)(HVM, Object*, Object*, BOPTType) noexcept;

		// **** 等于 ****
		// [非空] [异常]
		// HVM 虚拟机句柄
		// Object* 对象1
		// Object* 对象2
		// return -> 是否相等
		IResult<bool>(*f_equal)(HVM, Object*, Object*) noexcept;

		// **** 比较 ****
		// [可空 : 不支持比较] [异常]
		// HVM 虚拟机句柄
		// Object* 对象1
		// Object* 对象2
		// BOPTType 比较运算符
		// return -> 比较结果
		IResult<bool>(*f_compare)(HVM, Object*, Object*, BOPTType) noexcept;

		// **** 取迭代器 ****
		// 取出起始迭代器并压栈
		// [可空 : 不支持迭代器] [异常]
		// HVM 虚拟机句柄
		// Object* 对象
		IResult<void>(*f_iter_get)(HVM, Object*) noexcept;

		// **** 迭代器解包 ****
		// 解包迭代器并依次压栈
		// [可空 : 不支持迭代器] [异常]
		// HVM 虚拟机句柄
		// IteratorObject* 迭代器
		// Size 包长度
		IResult<void>(*f_iter_save)(HVM, IteratorObject*, Size) noexcept;

		// **** 迭代器自增 ****
		// 迭代器自增
		// [可空 : 不支持迭代器] [异常]
		// HVM 虚拟机句柄
		// IteratorObject* 迭代器
		IResult<void>(*f_iter_add)(HVM, IteratorObject*) noexcept;

		// **** 迭代器有效性检查 ****
		// 迭代器检查
		// [可空 : 不支持迭代器] [异常]
		// HVM 虚拟机句柄
		// IteratorObject* 迭代器
		// return -> 是否有效
		IResult<bool>(*f_iter_check)(HVM, IteratorObject*) noexcept;

		// **** 迭代器释放 ****
		// [可空 : 不支持迭代器 | 迭代器无需释放] [无异常]
		// IteratorObject* 迭代器
		void (*f_iter_free)(IteratorObject*) noexcept;

		explicit TypeObject(TypeObject* t) noexcept : Object{ t } {}

		TypeObject* __getType(TypeId id) noexcept {
			return static_cast<TypeStaticData*>(type->v_static)->builtinTypes[static_cast<Token>(id)];
		}

		bool canImplement(TypeObject* t) noexcept {
			if (f_implement) return f_implement(this, t);
			return this == t;
		}

		void makeUserClassHash() noexcept {
			auto nameHash{ std::hash<String>{}(v_name) };
			if (nameHash < UserTypeIdStart) nameHash = (nameHash + UserTypeIdStart) << 1ULL;
			v_id = static_cast<TypeId>(nameHash);
		}
	};

	// 空
	struct NullObject : Object {
		explicit NullObject(TypeObject* t) noexcept : Object{ t } {}
	};

	// 整数
	struct IntObject : Object {
		Int64 value;
		IntObject() noexcept : Object{ nullptr }, value{ } {}
		explicit IntObject(TypeObject* t) : Object{ t }, value{ } {}
	};

	// 浮点数
	struct FloatObject : Object {
		Float64 value;
		explicit FloatObject(TypeObject* t) noexcept : Object{ t }, value{ } {}
	};

	// 复数
	struct ComplexObject : Object {
		Float64 re;
		Float64 im;
		explicit ComplexObject(TypeObject* t) noexcept : Object{ t }, re{ }, im{ } {}
		void set(Float64 r, Float64 i) noexcept {
			re = r;
			im = i;
		}
	};

	// 逻辑值
	struct BoolObject : Object {
		bool value;
		explicit BoolObject(TypeObject* t) noexcept : Object{ t }, value{ } {}
	};

	// 左值
	struct LVObject : Object {
		LVType lvType;
		LVStorage storage;
		Object* parent;

		explicit LVObject(TypeObject* t) noexcept : Object{ t }, lvType{ }, parent{ }, storage{ } {}

		template<typename T>
		T* getParent() noexcept { return static_cast<T*>(parent); }

		bool isAddress() const noexcept { return lvType == LVType::ADDRESS; }
		void setAddress(Object* o) noexcept { *storage.address = o; }
		template<typename T>
		T* getAddressObject() noexcept { return static_cast<T*>(*storage.address); }

		bool isData() const noexcept { return lvType == LVType::DATA; }
		Index dataId() const noexcept { return storage.data.id; }

		template<Index N, typename T>
		T getData() noexcept {
			return freestanding::bit_cast<T>(storage.data.ds[N]);
		}

		template<Index id, typename T0, typename T1 = decltype(nullptr),
			typename T2 = decltype(nullptr), typename T3 = decltype(nullptr)>
		void setData(T0 t0, T1 t1 = nullptr, T2 t2 = nullptr, T3 t3 = nullptr) noexcept {
			lvType = LVType::DATA;
			storage.data.id = id;
			storage.data.ds[0] = freestanding::bit_cast<Memory>(t0);
			storage.data.ds[1] = freestanding::bit_cast<Memory>(t1);
			storage.data.ds[2] = freestanding::bit_cast<Memory>(t2);
			storage.data.ds[3] = freestanding::bit_cast<Memory>(t3);
		}
	};

	// 迭代器
	struct IteratorObject : Object {
		Object* ref;
		Index iterId;
		IteratorStorage storage;
		explicit IteratorObject(TypeObject* t) noexcept : Object{ t }, ref{ }, iterId{ }, storage{ } {}

		template<typename T>
		T* getReference() noexcept { return static_cast<T*>(ref); }

		template<Index N, typename T>
		T getData() noexcept { return freestanding::bit_cast<T>(storage.data[N]); }

		template<Index id, typename T0, typename T1 = decltype(nullptr),
			typename T2 = decltype(nullptr), typename T3 = decltype(nullptr)>
		void setData(T0 t0, T1 t1 = nullptr, T2 t2 = nullptr, T3 t3 = nullptr) noexcept {
			iterId = id;
			storage.data[0] = freestanding::bit_cast<Memory>(t0);
			storage.data[1] = freestanding::bit_cast<Memory>(t1);
			storage.data[2] = freestanding::bit_cast<Memory>(t2);
			storage.data[3] = freestanding::bit_cast<Memory>(t3);
		}

		template<Index N, typename T>
		void resetData(T t) noexcept { storage.data[N] = freestanding::bit_cast<Memory>(t); }
	};

	// 函数
	struct FunctionObject : Object {
		FunctionType ft;
		String name;
		ObjTArgs targs;
		union {
			NativeFunction native;
			NormalFunction normal;
		}data;

		explicit FunctionObject(TypeObject* t) noexcept : Object{ t }, ft{ FunctionType::NATIVE }, data{ } {}
	};

	// 成员函数
	struct MemberFunctionObject : Object {
		FunctionObject* funcObject;
		Object* thisObject;
		explicit MemberFunctionObject(TypeObject* t) noexcept : Object{ t }, funcObject{ }, thisObject{ } {}
	};

	// 对象
	struct ObjectObject : Object {
		MembersData membersData; // 成员变量
		explicit ObjectObject(TypeObject* t) noexcept : Object{ t } {}
	};

	// 字符串
	struct StringObject : Object {
		String value;
		explicit StringObject(TypeObject* t) noexcept : Object{ t } {}
	};

	// 列表
	struct ListObject : Object {
		Vector<Object*> objects;
		explicit ListObject(TypeObject* t) noexcept : Object{ t } {}
	};

	// 映射
	struct MapObject : Object {
		struct Item { // 元素实体
			Object* key; // 键
			Size hash; // 键哈希值
			Object* value; // 值
			Item* next; // 下一个实体
		};
		using ItemPointer = Item*;

		constexpr static auto LoadFactor{ 0.75 }; // 负载因子
		constexpr static auto DefaultCapacity{ 16ULL }; // 默认容量

		Size mCapacity; // 容量
		Size mSize; // 数量
		Size mThreshold; // 阈值
		ItemPointer* mTable; // 桶

		explicit MapObject(TypeObject* t) noexcept :
			Object{ t }, mCapacity{ }, mSize{ }, mThreshold{ }, mTable{ } {}
	};

	// 向量
	struct VectorObject : Object {
		util::Array<Float64> data;
		explicit VectorObject(TypeObject* t) noexcept : Object{ t } {}
	};

	// 矩阵
	struct MatrixObject : Object {
		util::Array<Float64> data;
		Size32 row, col;
		explicit MatrixObject(TypeObject* t) noexcept : Object{ t }, row{ }, col{ } {}
	};

	// 范围
	struct RangeObject : Object {
		Int64 start, step, end;
		explicit RangeObject(TypeObject* t) noexcept : Object{ t }, start{ }, step{ }, end{ } {}

		static bool check(Int64 start, Int64 step, Int64 end) noexcept {
			return step != 0 && (end - start) / step >= 0;
		}
	};

	// 数组
	struct ArrayObject : Object {
		Vector<Int64> data;
		explicit ArrayObject(TypeObject* t) noexcept : Object{ t } {}
	};

	// 字节集
	struct BinObject : Object {
		util::ByteArray data;
		explicit BinObject(TypeObject* t) noexcept : Object{ t } {}
	};

	// 集合
	struct HashSetObject : Object {
		struct Item { // 元素实体
			Object* key; // 键
			Size hash; // 键哈希值
			Item* next; // 下一个实体
		};
		using ItemPointer = Item*;

		constexpr static auto LoadFactor{ 0.75 }; // 负载因子
		constexpr static auto DefaultCapacity{ 16ULL }; // 默认容量

		Size mCapacity; // 容量
		Size mSize; // 数量
		Size mThreshold; // 阈值
		ItemPointer* mTable; // 桶

		explicit HashSetObject(TypeObject* t) noexcept :
			Object{ t }, mCapacity{ }, mSize{ }, mThreshold{ }, mTable{ } {}
	};
}

namespace hy {
	inline void Object::unlink() noexcept {
		if (!--lc) type->f_deallocate(this);
	}
}

namespace hy {
	// 对象转型
	template<typename T>
	inline constexpr T* obj_cast(Object* obj) noexcept {
		return static_cast<T*>(obj);
	}

	// 参数转型
	template<typename T>
	requires (copyable_memory<T> && sizeof(T) == sizeof(Memory))
	inline Memory arg_cast(T t) noexcept {
		return freestanding::bit_cast<Memory>(t);
	}

	// 参数逆转型
	template<typename T>
	requires (copyable_memory<T> && sizeof(T) == sizeof(Memory))
	inline T arg_recast(Memory m) noexcept {
		return freestanding::bit_cast<T>(m);
	}

	template<typename T>
	concept __4BitTransform = copyable_memory<T> && sizeof(T) == 4ULL;

	template<__4BitTransform T, __4BitTransform U>
	struct __8BitStruct { T t; U u; };

	// 合并8字节
	template<__4BitTransform T, __4BitTransform U>
	inline Memory merge8bit(T t, U u) noexcept {
		return freestanding::bit_cast<Memory>(__8BitStruct{ t, u });
	}

	// 分离8字节
	template<__4BitTransform T, __4BitTransform U>
	inline __8BitStruct<T, U> separate8bit(Memory m) noexcept {
		return freestanding::bit_cast<__8BitStruct<T, U>>(m);
	}

	template<typename T = Object>
	inline T* obj_allocate(TypeObject* type, Memory arg1 = nullptr, Memory arg2 = nullptr) noexcept {
		return obj_cast<T>(type->f_allocate(type, arg1, arg2));
	}

	template<bool check = false, bool isLink = false>
	FunctionObject* MakeNative(TypeObject* type, const StringView name, NativeFunc func, ObjArgsView argsType) noexcept {
		auto fobj = static_cast<FunctionObject*>(type->f_allocate(type, nullptr, nullptr));
		fobj->ft = FunctionType::NATIVE;
		fobj->name = name;
		fobj->data.native.func = func;
		fobj->data.native.checkArgs = check;
		fobj->targs.va_args = false;
		if (check && !argsType.empty()) {
			Size i{ }, argc{ argsType.size() };
			fobj->targs.args.resize(argc);
			for (; i < argc; ++i) fobj->targs.args[i] = { static_cast<TypeObject*>(argsType[i]), { } };
		}
		if constexpr (isLink) fobj->link();
		return fobj;
	}

	// 默认分配
	template<typename T>
	inline Object* f_allocate_default(TypeObject* type, Memory, Memory) noexcept {
		return new T(type);
	}

	// 默认回收
	template<typename T>
	inline void f_deallocate_default(Object* obj) noexcept {
		delete obj_cast<T>(obj);
	}

	// 空回收
	inline void f_deallocate_empty(Object*) noexcept {}

	// 空字符串
	inline IResult<void> f_string_empty(HVM, Object*, String*) noexcept {
		return IResult<void>(true);
	}

	// 定值哈希
	template<Size hash>
	inline IResult<Size> f_hash_fixed(HVM, Object*) noexcept {
		return IResult<Size>(hash);
	}

	// 引用判等
	inline IResult<bool> f_equal_ref(HVM, Object* obj1, Object* obj2) noexcept {
		return IResult<bool>(obj1 == obj2);
	}
}