#pragma once

#include "hy.freestanding.h"

namespace hy {
	enum class HYError : Token {
		// succeed
		NO_ERROR = 0x0000U, // 无错误

		// lex error
		MIN_LEX_ERROR = 0x0001U,

		MISSING_QUOTE = 0x0001U, // 缺失引号
		MISSING_ANNOTATE = 0x0002U, // 缺失双注释号
		INT_OUT_OF_RANGE = 0x0010U, // 整数超出范围
		UNKNOWN_SYMBOL = 0x0020U, // 未知符号

		MAX_LEX_ERROR = UNKNOWN_SYMBOL,

		// syntax error
		MIN_SYNTAX_ERROR = 0x0100U,

		EXPECT_ERROR = 0x0100U,  // 展望错误 [局部]
		EXPECT_ACUTE_RIGHT = 0x0101U,  // 缺失右尖括号
		EXPECT_SMALL_RIGHT = 0x0102U,  // 缺失右小括号
		EXPECT_MID_RIGHT = 0x0103U,  // 缺失右中括号
		EXPECT_LARGE_RIGHT = 0x0104U,  // 缺失右大括号
		EXPECT_ACUTE_LEFT = 0x0105U,  // 缺失左尖括号 [未用]
		EXPECT_SMALL_LEFT = 0x0106U,  // 缺失左小括号
		EXPECT_MID_LEFT = 0x0107U,  // 缺失左中括号
		EXPECT_LARGE_LEFT = 0x0108U,  // 缺失左大括号
		EXPECT_COMMA = 0x0109U,  // 缺失逗号 [未用]
		EXPECT_SEMICOLON = 0x010AU,  // 缺失分号
		EXPECT_COLON = 0x010BU,  // 缺失冒号
		EXPECT_CONNECTOR = 0x010CU,  // 缺失连接符 [局部]
		EXPECT_ASSIGN = 0x010DU,  // 缺失赋值符
		EXPECT_ID = 0x010EU,  // 缺失ID
		EXPECT_POINTER = 0x010FU,	// 缺失箭头符
		EXPECT_LITERAL = 0x0110U,	// 缺失字符串
		INVALID_SYMBOL = 0x0200U,  // 非法符号
		INVALID_CONCEPT_TYPE = 0x0201U,  // 非法概念属性类型
		INVALID_VECTOR_ATOM = 0x0202U,  // 非法向量元素类型
		INVALID_RANGE_ATOM = 0x0203U,  // 非法范围元素类型
		NOT_C = 0x0300U,  // 不是一个常量
		NOT_NOC = 0x0301U,  // 不是一个原生对象常量 [局部]
		NOT_V = 0x0302U,  // 不是一个值
		NOT_LV = 0x0303U, // 不是一个左值
		EXISTS_DEFAULT = 0x0400U,  // 已经存在默认情况
		EMPTY_ARGUMENTS = 0x0500U,	// 空参数列表
		ETC_POSITION_ERROR = 0x0501U,	// 变参列表不在尾部
		EMPTY_AUTOBIND = 0x0550U,	// 空结构化绑定
		INVALID_NATIVE_TYPE = 0x0600U,	// 非法的原生类型
		STATEMENT_SS_ERROR = 0x0801U,	// 不合法的语句起始
		STATEMENT_FDS_ERROR = 0x0802U,  // 函数定义语句位置错误
		STATEMENT_TDS_ERROR = 0x0803U,  // 类型定义语句位置错误
		STATEMENT_CDS_ERROR = 0x0804U,  // 概念定义语句位置错误
		STATEMENT_IS_ERROR = 0x0805U,  // 导入模块语句位置错误
		STATEMENT_US_ERROR = 0x0806U,	// 命名空间语句位置错误
		STATEMENT_NS_ERROR = 0x0807U,	// 原生语句位置错误
		STATEMENT_CVDS_ERROR = 0x0808U,	// 常量定义语句位置错误
		STATEMENT_GVDS_ERROR = 0x0809U, // 全局变量定义语句未知错误
		STATEMENT_LCS_ERROR = 0x0820U,	// 继续循环语句位置错误
		STATEMENT_LBS_ERROR = 0x0821U,	// 跳出循环语句位置错误
		STATEMENT_FRS_ERROR = 0x0822U,	// 返回返回语句位置错误
		STATEMENT_THIS_ERROR = 0x0823U,	// this指针位置错误
		GLOBAL_TABLE_LOOPCOUNT = 0x0A00U,	// 全局表循环次数异常
		GLOBAL_TABLE_FUNCTIONCOUNT = 0x0A01U,	// 全局表函数嵌套次数异常
		GLOBAL_TABLE_CLASSCOUNT = 0x0A02U,	// 全局表类嵌套次数异常

		MAX_SYNTAX_ERROR = GLOBAL_TABLE_CLASSCOUNT,

		// runtime error
		MIN_RUNTIME_ERROR = 0x1000U,

		COMPILE_ERROR = 0x1000U, // 编译错误
		BYTECODE_BROKEN, // 字节码破损
		STACK_OVERFLOW, // 栈溢出
		FILE_NOT_EXISTS, // 文件不存在
		UNDEFINED_ID, // 未定义标识符
		REDEFINED_ID, // 重定义标识符
		UNMATCHED_CALL, // 无匹配的函数调用
		UNMATCHED_INDEX, // 无匹配的索引调用
		UNMATCHED_VERSION, // 不匹配的版本
		UNMATCHED_PLATFORM, // 不匹配的平台
		INCOMPATIBLE_ASSIGN, // 不兼容的赋值类型
		INCOMPATIBLE_CALCASSIGN, // 不兼容的运算赋值类型
		UNSUPPORTED_SOPT, // 不支持的一元运算
		UNSUPPORTED_BOPT, // 不支持的二元运算
		UNMATCHED_MEMBER, // 无匹配的成员
		UNSUPPORTED_UNPACK, // 不支持解包
		UNMATCHED_UNPACK, // 不匹配的解包数量
		NOT_LEFT_VALUE, // 非左值
		INDEX_OUT_OF_RANGE, // 索引越界
		NOT_BOOLEAN, // 非逻辑值
		UNDEFINED_CONST, // 未定义常量
		REDEFINED_CONST, // 重定义常量
		NOT_TYPE, // 非类型
		UNMATCHED_TYPE, // 不匹配的类型
		UNMATCHED_LEN, // 不匹配的长度
		ILLEGAL_RANGE, // 非法范围
		UNSUPPORTED_ITERATOR, // 不支持迭代器
		UNSUPPORTED_KEYTYPE, // 不支持的键类型
		ILLEGAL_MEMBERTYPE, // 非法成员类型
		NEGATIVE_NUMBER, // 数值为负数
		EMPTY_CONTAINER, // 空容器
		ASSERT_FALSE, // 断言假
		UNSUPPORTED_SCAN, // 不支持扫描
		IO_DEVICE_ERROR, // IO设备错误
		SCAN_ERROR, // 扫描错误
		DLL_ERROR, // 动态链接库错误

		MAX_RUNTIME_ERROR = DLL_ERROR,

		// internal error
	};

	inline constexpr bool operator~ (HYError error) noexcept { return error == HYError::NO_ERROR; }
	inline constexpr bool operator! (HYError error) noexcept { return error != HYError::NO_ERROR; }

	inline constexpr bool IsLexError(HYError error) noexcept {
		return error >= HYError::MIN_LEX_ERROR && error <= HYError::MAX_LEX_ERROR;
	}

	inline constexpr bool IsSyntaxError(HYError error) noexcept {
		return error >= HYError::MIN_SYNTAX_ERROR && error <= HYError::MAX_SYNTAX_ERROR;
	}

	inline constexpr bool IsRuntimeError(HYError error) noexcept {
		return error >= HYError::MIN_RUNTIME_ERROR && error <= HYError::MAX_RUNTIME_ERROR;
	}

	// 源码分析结果
	struct CodeResult {
		HYError error;
		Index32 line;
		CStr start;
		CStr end;

		constexpr operator bool() const noexcept { return error == HYError::NO_ERROR; }
	};

	// 运行结果
	struct RuntimeResult {
		HYError error;
		String msg;

		constexpr operator bool() const noexcept { return error == HYError::NO_ERROR; }
	};

	// 指令返回结果
	template<typename T>
	struct IResult {
		bool ok;
		T data;

		explicit IResult() noexcept : ok{ false }, data{ } {}
		explicit IResult(T t) noexcept : ok{ true }, data{ t } {}

		operator bool() const noexcept { return ok; }
	};

	template<>
	struct IResult<void> {
		bool ok;

		explicit IResult(bool v = false) noexcept : ok{ v } {}
		operator bool() const noexcept { return ok; }
	};

	template<typename T = void, typename Func, typename... Args>
	[[nodiscard]] inline IResult<T> SetError(Func func, Args&&... args) noexcept {
		func(freestanding::forward<Args>(args)...);
		return IResult<T>();
	}
}