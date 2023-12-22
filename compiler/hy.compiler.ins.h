/**************************************************
*
* @文件				hy.compiler.ins
* @作者				钱浩宇
* @创建时间			2021-11-05
* @更新时间			2022-12-06
* @摘要
* 记录所有操作码, 指令码的值, 定义了指令的形式
*
**************************************************/

#pragma once

#include "../public/hy.types.h"

namespace hy {
	enum class SOPTType : Byte {
		NONE,					// 空
		NEGATIVE,				// 取负
		NOT,					// 取非
	};

	inline constexpr StringView SOPTString(SOPTType type) noexcept {
		switch (type) {
		case SOPTType::NEGATIVE: return u"-";
		case SOPTType::NOT: return u"!";
		default: return u"?";
		}
	}

	enum class BOPTType : Byte {
		NONE,					// 空
		ADD,					// 加法
		SUBTRACT,				// 减法
		MULTIPLE,				// 乘法
		DIVIDE,					// 除法
		MOD,					// 模
		POWER,					// 幂
		GT,						// 大于
		GE,						// 大于等于
		LT,						// 小于
		LE,						// 小于等于
		EQ,						// 等于
		NE,						// 不等于
	};

	inline constexpr StringView BOPTString(BOPTType type) noexcept {
		switch (type) {
		case BOPTType::ADD: return u"+";
		case BOPTType::SUBTRACT: return u"-";
		case BOPTType::MULTIPLE: return u"*";
		case BOPTType::DIVIDE: return u"/";
		case BOPTType::MOD: return u"%";
		case BOPTType::POWER: return u"^";
		case BOPTType::GT: return u">";
		case BOPTType::GE: return u">=";
		case BOPTType::LT: return u"<";
		case BOPTType::LE: return u"<=";
		case BOPTType::EQ: return u"==";
		case BOPTType::NE: return u"!=";
		default: return u"?";
		}
	}

	enum class AssignType : Byte {
		NONE,					// 空
		ADD_ASSIGN,				// 加赋值
		SUBTRACT_ASSIGN,		// 减赋值
		MULTIPLE_ASSIGN,		// 乘赋值
		DIVIDE_ASSIGN,			// 除赋值
		MOD_ASSIGN,				// 模赋值
	};

	inline constexpr StringView ATString(AssignType type) noexcept {
		switch (type) {
		case AssignType::ADD_ASSIGN: return u"+=";
		case AssignType::SUBTRACT_ASSIGN: return u"-=";
		case AssignType::MULTIPLE_ASSIGN: return u"*=";
		case AssignType::DIVIDE_ASSIGN: return u"/=";
		case AssignType::MOD_ASSIGN: return u"%=";
		default: return u"?";
		}
	}

	enum class InsType : Byte {
		// [空指令]
		// 无意义
		NOP,

		// [字面值压栈] (常值索引)
		// 从字面值段取出常值索引对应的值构建对应类型对象压入栈顶
		PUSH_LITERAL,

		// [逻辑值压栈] (逻辑值)
		// 将逻辑值压入栈顶
		PUSH_BOOLEAN,

		// [this指针压栈]
		// 将this指针指向对象的压入栈顶
		PUSH_THIS,

		// [空压栈]
		// 将Null对象压入栈顶
		PUSH_NULL,

		// [符号压栈] (符号索引)
		// 将当前符号表中符号索引对应的符号压入栈顶
		PUSH_SYMBOL,

		// [引用压栈] (引用索引)
		// 从内向外查找字面值段中引用索引对应的对象将其压入栈顶
		PUSH_REF,

		// [符号左值压栈] (符号索引)
		// 将当前符号表中符号索引对应的符号地址压入栈顶
		PUSH_SYMBOL_LV,

		// [引用左值压栈] (符号索引)
		// 从内向外查找字面值段中引用索引对应的对象地址将其压入栈顶
		PUSH_REF_LV,

		// [常量压栈] (引用索引)
		// 从常量表中查找字面值段中引用索引对应的对象将其压入栈顶
		PUSH_CONST,

		// [列表压栈] (参数数量)
		// 弹出栈顶等同于参数数量的对象, 构建一个列表, 再压入栈顶
		PUSH_LIST,

		// [字典压栈] (参数数量)
		// 弹出栈顶等同于参数数量的键值对对象, 构建一个字典, 再压入栈顶
		PUSH_DICT,

		// [向量压栈] (符号索引)
		// 将字面值段中符号索引对应的向量压入栈顶, 若符号索引为INone则压入空向量
		PUSH_VECTOR,

		// [矩阵压栈] (符号索引)
		// 将字面值段中符号索引对应的矩阵压入栈顶, 若符号索引为INone则压入空矩阵
		PUSH_MATRIX,

		// [范围压栈] (符号索引)
		// 将字面值段中符号索引对应的范围压入栈顶
		PUSH_RANGE,

		// [保存参数包] (符号索引)
		// 记数量为字面值段中符号索引对应的引用字符串数量, 弹出栈顶等数量的对象
		// 推导这些对象的类型并依次注册, 名称为引用字符串对应的值
		SAVE_PACK,

		// [弹栈]
		// 弹出栈顶的对象
		POP,

		// [赋值] (参数个数)
		// 弹出栈顶等同于参数数目的参数, 再弹出栈顶的表达式, 逆向依次为各参数赋值
		ASSIGN,

		// [复合赋值] (AssignType复合类型)
		// 弹出栈顶的左值, 再弹出栈顶的表达式, 完成复合赋值
		ASSIGN_EX,

		// [成员操作] (符号索引)
		// 弹出栈顶的对象, 使用字面值段中符号索引对应的名称来索引对象, 将新对象压栈
		MEMBER,

		// [成员左值操作] (符号索引)
		// 弹出栈顶的对象, 使用字面值段中符号索引对应的名称来索引对象, 将新对象地址压栈
		MEMBER_LV,

		// [索引操作] (参数个数)
		// 弹出栈顶的对象, 再弹出栈顶等同于参数数目的参数, 对其使用参数索引, 将新对象压栈
		INDEX,

		// [索引左值操作] (参数个数)
		// 弹出栈顶的对象, 再弹出栈顶等同于参数数目的参数, 对其使用参数索引, 将新对象地址压栈
		INDEX_LV,

		// [函数调用] (参数个数)
		// 弹出栈顶等同于参数数目的参数, 再弹出栈顶的对象, 对其使用参数函数调用, 将返回值压栈
		CALL,

		// [解包] (参数个数)
		// 弹出栈顶的对象, 将其等同于参数数目的元素依次分离压栈
		UNPACK,

		// [取逻辑值]
		// 将栈顶的对象转换为逻辑值
		AS_BOOL,

		// [一元操作] (SOPTType操作类型)
		// 取出栈顶的元素,使用操作类型对应的操作,将结果压栈
		OP_SINGLE,

		// [二元操作] (BOPTType操作类型)
		// 两次弹出栈顶的对象2和对象1, 使用操作类型对应的操作, 将结果压栈
		OP_BINARY,

		// [取迭代元]
		// 取出栈顶的元素, 将其迭代元压栈
		GET_ITER,

		// [记录迭代元解包] (符号索引)
		// 将栈顶的迭代元包含元素对应到符号索引记录到符号表
		SAVE_ITER,

		// [递增迭代元]
		// 将栈顶的迭代元递增
		ADD_ITER,

		// [检查迭代元跳转] (偏移)
		// 检查栈顶的迭代元, 如果到达迭代尾则弹出栈顶的迭代元并跳转到向后偏移指定值的位置执行, 否则向下执行
		JUMP_CHECK_ITER,

		// [不同跳转弹栈] (偏移)
		// 弹出栈顶的元素, 与此时栈顶的元素比较, 若不同则跳转到向后偏移指定值的位置执行, 否则向下执行
		JUMP_NE_POP,

		// [无条件跳转] (偏移)
		// 跳转到向后偏移指定值的位置执行
		JUMP,

		// [真跳转] (偏移)
		// 如果栈顶结果是真则跳转到向后偏移指定值的位置执行, 否则向下执行
		JUMP_TRUE,

		// [假跳转] (偏移)
		// 如果栈顶结果是假则跳转到向后偏移指定值的位置执行, 否则向下执行
		JUMP_FALSE,

		// [真跳转弹栈] (偏移)
		// 如果栈顶结果是真则跳转到向后偏移指定值的位置执行, 否则向下执行, 执行前把栈顶弹出
		JUMP_TRUE_POP,

		// [假跳转弹栈] (偏移)
		// 如果栈顶结果是假则跳转到向后偏移指定值的位置执行, 否则向下执行, 执行前把栈顶弹出
		JUMP_FALSE_POP,

		// [无条件逆跳转] (偏移)
		// 到向前偏移指定值的位置执行
		JUMP_RE,

		// [结束函数]
		// 弹出对象栈栈顶的返回值
		RETURN,

		// [导入库] (符号索引)
		// 导入符号索引对应的模块
		PRE_IMPORT,

		// [引用导入库] (符号索引)
		// 导入符号索引对应的模块并引用其
		PRE_IMPORT_USING,

		// [引用] (符号索引)
		// 引用符号索引对应的模块
		PRE_USING,

		// [软链接] (符号索引)
		// 为符号索引对应的名称及引用建立软链接
		PRE_SOFT_LINK,

		// [加载原生] (符号索引)
		// 加载符号索引对应的原生库
		PRE_NATIVE,

		// [加载常量] (符号索引)
		// 加载符号索引对应的常量
		PRE_CONST,

		// [加载全局变量] (符号索引)
		// 加载符号索引对应的全局变量
		PRE_GLOBAL,

		// [加载函数] (符号索引)
		// 加载符号索引对应的函数
		PRE_FUNCTION,

		// [加载Lambda] (符号索引)
		// 加载符号索引对应的Lambda函数, 若已存在则忽略
		PRE_LAMBDA,

		// [加载类] (符号索引)
		// 加载符号索引对应的类
		PRE_CLASS,

		// [加载概念] (符号索引)
		// 加载符号索引对应的概念
		PRE_CONCEPT,
	};

	// 指令
	struct Ins {
		InsType type;	// 操作码
		Uint8 arg1;	// 操作数1
		Uint8 arg2;	// 操作数2
		Uint8 arg3;	// 操作数3

		template<unsigned_integral V>
		constexpr void set(V v) noexcept {
			if constexpr (sizeof(V) == 1U) {
				arg1 = arg2 = 0U;
				arg3 = v;
			}
			else if constexpr (sizeof(V) == 2U) {
				arg1 = 0U;
				arg2 = static_cast<Uint8>((v >> 8U) & 0xFFU);
				arg3 = static_cast<Uint8>(v & 0xFFU);
			}
			else {
				arg1 = static_cast<Uint8>((v >> 16U) & 0xFFU);
				arg2 = static_cast<Uint8>((v >> 8U) & 0xFFU);
				arg3 = static_cast<Uint8>(v & 0xFFU);
			}
		}

		template<unsigned_integral V>
		constexpr V get() const noexcept {
			if constexpr (sizeof(V) == 1U) return static_cast<V>(arg3);
			else if constexpr (sizeof(V) == 2U) return static_cast<V>((arg2 << 8U) | arg3);
			else return static_cast<V>((arg1 << 16U) | (arg2 << 8U) | arg3);
		}

		constexpr Ins(InsType it, unsigned_integral auto v) noexcept : type{ it } { set(v); }

		constexpr explicit Ins(InsType it) noexcept : type{ it }, arg1{ }, arg2{ }, arg3{ } {}

		constexpr Uint32 as() const noexcept {
			return (static_cast<Byte>(type) << 24U) | (arg1 << 16U) | (arg2 << 8U) | arg3;
		}
	};
}