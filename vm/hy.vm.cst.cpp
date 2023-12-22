#include "hy.vm.impl.h"

#include <fast_io/fast_io.h>

// fast_io自定义部分
namespace fast_io {
	// fast_io对RefView的支持
	inline hy::Size print_reserve_size(io_reserve_type_t<hy::Char, hy::RefView>, hy::RefView refView) noexcept {
		return static_cast<hy::Size>(refView.mSize - 1U);
	}

	inline hy::Str print_reserve_define(io_reserve_type_t<hy::Char, hy::RefView>, hy::Str it, hy::RefView refView) noexcept {
		auto count{ static_cast<hy::Size>(refView.size() - 1U) };
		hy::freestanding::copy_n(it, refView.data(), count);
		for (auto start{ it }, end{ start + count }; start != end; ++start) {
			if (!*start) *start = u'.';
		}
		return it + count;
	}

	// fast_io对ObjArgsView的支持
	template<bool call>
	struct ObjArgsWrapper {
		hy::ObjArgsView args;
	};

	template<bool call>
	inline hy::Size print_reserve_size(io_reserve_type_t<hy::Char, ObjArgsWrapper<call>>, ObjArgsWrapper<call> w) noexcept {
		auto args{ w.args };
		auto size{ 1ULL + args.size() };
		if (args.empty()) ++size;
		for (auto arg : args) size += arg->type->v_name.size();
		return size;
	}

	template<bool call>
	inline hy::Str print_reserve_define(io_reserve_type_t<hy::Char, ObjArgsWrapper<call>>, hy::Str it, ObjArgsWrapper<call> w) noexcept {
		auto args{ w.args };
		*it++ = call ? u'(' : u'[';
		for (auto arg : args) {
			auto& name{ arg->type->v_name };
			auto size{ name.size() };
			hy::freestanding::copy_n(it, name.data(), size);
			it += size;
			*it++ = u',';
		}
		if (!args.empty()) --it;
		*it++ = call ? u')' : u']';
		return it;
	}
}

namespace hy {
	StringView GenerateLexError(HYError error) noexcept {
		switch (error) {
		case HYError::MISSING_QUOTE: return u"字符串尾部缺失引号";
		case HYError::MISSING_ANNOTATE: return u"块注释尾缺失注释符号";
		case HYError::INT_OUT_OF_RANGE: return u"整数溢出";
		case HYError::UNKNOWN_SYMBOL: return u"未知符号";
		default: return u"内部错误";
		}
	}

	StringView GenerateSyntaxError(HYError error) noexcept {
		switch (error) {
		case HYError::EXPECT_ACUTE_RIGHT: return u"缺失右尖括号";
		case HYError::EXPECT_SMALL_RIGHT: return u"缺失右小括号";
		case HYError::EXPECT_MID_RIGHT: return u"缺失右中括号";
		case HYError::EXPECT_LARGE_RIGHT: return u"缺失右大括号";
		case HYError::EXPECT_ACUTE_LEFT: return u"缺失左尖括号";
		case HYError::EXPECT_SMALL_LEFT: return u"缺失左小括号";
		case HYError::EXPECT_MID_LEFT: return u"缺失左中括号";
		case HYError::EXPECT_LARGE_LEFT: return u"缺失左大括号";
		case HYError::EXPECT_COMMA: return u"缺失逗号";
		case HYError::EXPECT_SEMICOLON: return u"缺失分号";
		case HYError::EXPECT_COLON: return u"缺失冒号";
		case HYError::EXPECT_CONNECTOR: return u"缺失连接符";
		case HYError::EXPECT_ASSIGN: return u"缺失赋值号";
		case HYError::EXPECT_ID: return u"缺少标识符";
		case HYError::EXPECT_POINTER: return u"缺失指向符";
		case HYError::EXPECT_LITERAL: return u"缺少字符串";
		case HYError::INVALID_SYMBOL: return u"非法符号";
		case HYError::INVALID_CONCEPT_TYPE: return u"非法概念类型";
		case HYError::INVALID_VECTOR_ATOM: return u"非法向量元素";
		case HYError::INVALID_RANGE_ATOM: return u"非法范围元素";
		case HYError::NOT_C: return u"不是一个常值";
		case HYError::NOT_NOC: return u"不是一个常值对象";
		case HYError::NOT_V: return u"不是一个值";
		case HYError::NOT_LV: return u"不是一个左值";
		case HYError::EXISTS_DEFAULT: return u"分支已存在默认情况";
		case HYError::EMPTY_ARGUMENTS: return u"空参数列表";
		case HYError::ETC_POSITION_ERROR: return u"可变参数包位置错误";
		case HYError::EMPTY_AUTOBIND: return u"空结构化绑定";
		case HYError::INVALID_NATIVE_TYPE: return u"非法原生类型";
		case HYError::STATEMENT_SS_ERROR: return u"不合法的语句起始";
		case HYError::STATEMENT_FDS_ERROR: return u"函数定义语句位置错误";
		case HYError::STATEMENT_TDS_ERROR: return u"类定义语句位置错误";
		case HYError::STATEMENT_CDS_ERROR: return u"概念定义语句位置错误";
		case HYError::STATEMENT_IS_ERROR: return u"导入语句位置错误";
		case HYError::STATEMENT_US_ERROR: return u"命名空间语句位置错误";
		case HYError::STATEMENT_NS_ERROR: return u"原生语句位置错误";
		case HYError::STATEMENT_CVDS_ERROR: return u"常量定义语句位置错误";
		case HYError::STATEMENT_GVDS_ERROR: return u"全局变量定义语句位置错误";
		case HYError::STATEMENT_LCS_ERROR: return u"不在while内使用continue";
		case HYError::STATEMENT_LBS_ERROR: return u"不在while内使用break";
		case HYError::STATEMENT_FRS_ERROR: return u"不在函数块内使用return";
		case HYError::STATEMENT_THIS_ERROR: return u"不在类成员函数内使用this";
		default: return u"内部错误";
		}
	}
}

namespace hy {
	void SetError_CompileError(VM* vm, CodeResult cr, const StringView name) noexcept {
		vm->result.error = HYError::COMPILE_ERROR;
		fast_io::u16ostring_ref strRef{ &vm->result.msg };
		print(strRef, u"编译错误 < ", name, u" : ", cr.line, u" > ",
			IsSyntaxError(cr.error) ? GenerateSyntaxError(cr.error) : GenerateLexError(cr.error), u" ");
		if (cr.start) print(strRef, StringView(cr.start, cr.end - cr.start + 1));
	}

	void SetError_ByteCodeBroken(VM* vm) noexcept {
		vm->result.error = HYError::BYTECODE_BROKEN;
		vm->result.msg = u"字节码损坏";
	}

	void SetError_StackOverflow(VM* vm) noexcept {
		vm->result.error = HYError::STACK_OVERFLOW;
		vm->result.msg = u"递归栈溢出";
	}

	void SetError_FileNotExists(VM* vm, const StringView name) noexcept {
		vm->result.error = HYError::FILE_NOT_EXISTS;
		fast_io::u16ostring_ref strRef{ &vm->result.msg };
		print(strRef, name, u" 文件或目录不存在");
	}

	void SetError_UnmatchedCall(VM* vm, const StringView name, ObjArgsView args) noexcept {
		vm->result.error = HYError::UNMATCHED_CALL;
		fast_io::u16ostring_ref strRef{ &vm->result.msg };
		print(strRef, u"无匹配的函数调用 ", name, fast_io::ObjArgsWrapper<true>(args));
	}

	void SetError_UnmatchedIndex(VM* vm, const StringView name, ObjArgsView args) noexcept {
		vm->result.error = HYError::UNMATCHED_INDEX;
		fast_io::u16ostring_ref strRef{ &vm->result.msg };
		print(strRef, u"无匹配的索引调用 ", name, fast_io::ObjArgsWrapper<false>(args));
	}

	void SetError_UndefinedID(VM* vm, const StringView name) noexcept {
		vm->result.error = HYError::UNDEFINED_ID;
		fast_io::u16ostring_ref strRef{ &vm->result.msg };
		print(strRef, u"未定义标识符 ", name);
	}

	void SetError_UndefinedRef(VM* vm, RefView refView) noexcept {
		vm->result.error = HYError::UNDEFINED_ID;
		fast_io::u16ostring_ref strRef{ &vm->result.msg };
		print(strRef, u"未定义标识符 ", refView);
	}

	void SetError_RedefinedID(VM* vm, const StringView name) noexcept {
		vm->result.error = HYError::REDEFINED_ID;
		fast_io::u16ostring_ref strRef{ &vm->result.msg };
		print(strRef, u"重定义标识符 ", name);
	}

	void SetError_UnmatchedVersion(VM* vm, Byte* v1, const StringView v2) noexcept {
		vm->result.error = HYError::UNMATCHED_VERSION;
		fast_io::u16ostring_ref strRef{ &vm->result.msg };
		print(strRef, u"字节码版本 ", (Int64)v1[0], u".",
			(Int64)v1[1], u".", (Int64)v1[2], u".", (Int64)v1[3],
			u" 与虚拟机版本 ", v2, u"不匹配");
	}

	void SetError_UnmatchedPlatform(VM* vm, PlatformType v1, PlatformType v2) noexcept {
		vm->result.error = HYError::UNMATCHED_PLATFORM;
		fast_io::u16ostring_ref strRef{ &vm->result.msg };
		print(strRef, u"操作系统 ", GetPlatformTypeString(v1), u" 与虚拟机运行系统 ", GetPlatformTypeString(v2), u"不匹配");
	}

	void SetError_IncompatibleAssign(VM* vm, TypeObject* t1, TypeObject* t2) noexcept {
		vm->result.error = HYError::INCOMPATIBLE_ASSIGN;
		fast_io::u16ostring_ref strRef{ &vm->result.msg };
		print(strRef, t2->v_name, u" 到 ", t1->v_name, u" 赋值不兼容");
	}

	void SetError_IncompatibleCalcAssign(VM* vm, TypeObject* t1, TypeObject* t2, AssignType opt) noexcept {
		vm->result.error = HYError::INCOMPATIBLE_ASSIGN;
		fast_io::u16ostring_ref strRef{ &vm->result.msg };
		StringView optName{ u"?" };
		switch (opt) {
		case AssignType::ADD_ASSIGN: optName = u"+="; break;
		case AssignType::SUBTRACT_ASSIGN: optName = u"-="; break;
		case AssignType::MULTIPLE_ASSIGN: optName = u"*="; break;
		case AssignType::DIVIDE_ASSIGN: optName = u"/="; break;
		case AssignType::MOD_ASSIGN: optName = u"%="; break;
		}
		print(strRef, t1->v_name, u" 与 ", t2->v_name, u" 不支持 ", optName, u" 运算赋值");
	}

	void SetError_UnsupportedSOPT(VM* vm, TypeObject* t, SOPTType opt) noexcept {
		vm->result.error = HYError::UNSUPPORTED_SOPT;
		fast_io::u16ostring_ref strRef{ &vm->result.msg };
		StringView optName{ u"?" };
		switch (opt) {
		case SOPTType::NEGATIVE: optName = u"-"; break;
		case SOPTType::NOT: optName = u"!"; break;
		}
		print(strRef, t->v_name, u" 不支持 ", optName, u" 运算");
	}

	void SetError_UnsupportedBOPT(VM* vm, TypeObject* t1, TypeObject* t2, BOPTType opt) noexcept {
		vm->result.error = HYError::UNSUPPORTED_BOPT;
		fast_io::u16ostring_ref strRef{ &vm->result.msg };
		StringView optName{ u"?" };
		switch (opt) {
		case BOPTType::ADD: optName = u"+"; break;
		case BOPTType::SUBTRACT: optName = u"-"; break;
		case BOPTType::MULTIPLE: optName = u"*"; break;
		case BOPTType::DIVIDE: optName = u"/"; break;
		case BOPTType::MOD: optName = u"%"; break;
		case BOPTType::POWER: optName = u"^"; break;
		case BOPTType::GT: optName = u">"; break;
		case BOPTType::GE: optName = u">="; break;
		case BOPTType::LT: optName = u"<"; break;
		case BOPTType::LE: optName = u"<="; break;
		case BOPTType::EQ: optName = u"=="; break;
		case BOPTType::NE: optName = u"!="; break;
		}
		print(strRef, t1->v_name, u" 与 ", t2->v_name, u" 不支持 ", optName, u" 运算");
	}

	void SetError_UnmatchedMember(VM* vm, TypeObject* t1, const StringView name) noexcept {
		vm->result.error = HYError::UNMATCHED_MEMBER;
		fast_io::u16ostring_ref strRef{ &vm->result.msg };
		print(strRef, t1->v_name, u" 无成员或属性 ", name);
	}

	void SetError_UnsupportedUnpack(VM* vm, TypeObject* t) noexcept {
		vm->result.error = HYError::UNSUPPORTED_UNPACK;
		fast_io::u16ostring_ref strRef{ &vm->result.msg };
		print(strRef, t->v_name, u" 不支持解包");
	}

	void SetError_UnmatchedUnpack(VM* vm, Size s1, Size s2) noexcept {
		vm->result.error = HYError::UNMATCHED_UNPACK;
		fast_io::u16ostring_ref strRef{ &vm->result.msg };
		print(strRef, u"不匹配的解包数量, 需要 ", s1, u" ,但提供了 ", s2);
	}

	void SetError_NotLeftValue(VM* vm, const StringView name) noexcept {
		vm->result.error = HYError::NOT_LEFT_VALUE;
		fast_io::u16ostring_ref strRef{ &vm->result.msg };
		print(strRef, name, u" 不是一个左值");
	}

	void SetError_IndexOutOfRange(VM* vm, Int64 index, Int64 boundary) noexcept {
		vm->result.error = HYError::INDEX_OUT_OF_RANGE;
		fast_io::u16ostring_ref strRef{ &vm->result.msg };
		print(strRef, u"索引 ", index, u" 越界 ", boundary);
	}

	void SetError_NotBoolean(VM* vm, TypeObject* t) noexcept {
		vm->result.error = HYError::NOT_BOOLEAN;
		fast_io::u16ostring_ref strRef{ &vm->result.msg };
		print(strRef, t->v_name, u" 不可转换为逻辑值");
	}

	void SetError_UndefinedConst(VM* vm, RefView refView) noexcept {
		vm->result.error = HYError::UNDEFINED_CONST;
		fast_io::u16ostring_ref strRef{ &vm->result.msg };
		print(strRef, u"未定义常量 ", refView);
	}

	void SetError_RedefinedConst(VM* vm, const StringView name) noexcept {
		vm->result.error = HYError::REDEFINED_CONST;
		fast_io::u16ostring_ref strRef{ &vm->result.msg };
		print(strRef, u"重定义常量 ", name);
	}

	void SetError_NotType(VM* vm, RefView refView) noexcept {
		vm->result.error = HYError::NOT_TYPE;
		fast_io::u16ostring_ref strRef{ &vm->result.msg };
		print(strRef, refView, u" 不是一个类型");
	}

	void SetError_UnmatchedType(VM* vm, TypeObject* t1, TypeObject* t2) noexcept {
		vm->result.error = HYError::NOT_TYPE;
		fast_io::u16ostring_ref strRef{ &vm->result.msg };
		print(strRef, t1->v_name, u" 不匹配 ", t2->v_name);
	}

	void SetError_UnmatchedLen(VM* vm, Size s1, Size s2) noexcept {
		vm->result.error = HYError::UNMATCHED_LEN;
		fast_io::u16ostring_ref strRef{ &vm->result.msg };
		print(strRef, u"不匹配的长度 ", s1, u" 与 ", s2);
	}

	void SetError_IllegalRange(VM* vm, Int64 start, Int64 step, Int64 end) noexcept {
		vm->result.error = HYError::ILLEGAL_RANGE;
		fast_io::u16ostring_ref strRef{ &vm->result.msg };
		print(strRef, u"非法范围 ", start, u" ~ ", step, u" ~ ", end);
	}

	void SetError_UnsupportedIterator(VM* vm, TypeObject* t) noexcept {
		vm->result.error = HYError::UNSUPPORTED_ITERATOR;
		fast_io::u16ostring_ref strRef{ &vm->result.msg };
		print(strRef, t->v_name, u" 不支持迭代器操作");
	}

	void SetError_UnsupportedKeyType(VM* vm, TypeObject* t) noexcept {
		vm->result.error = HYError::UNSUPPORTED_KEYTYPE;
		fast_io::u16ostring_ref strRef{ &vm->result.msg };
		print(strRef, t->v_name, u" 不能作为键");
	}

	void SetError_NoKey(VM* vm, const StringView keyName) noexcept {
		vm->result.error = HYError::INDEX_OUT_OF_RANGE;
		fast_io::u16ostring_ref strRef{ &vm->result.msg };
		print(strRef, u"不存在键 ", keyName);
	}

	void SetError_IllegalMemberType(VM* vm, TypeObject* t) noexcept {
		vm->result.error = HYError::ILLEGAL_MEMBERTYPE;
		fast_io::u16ostring_ref strRef{ &vm->result.msg };
		print(strRef, u"非法成员类型 ", t->v_name);
	}

	void SetError_DllError(VM* vm, const StringView path) noexcept {
		vm->result.error = HYError::DLL_ERROR;
		fast_io::u16ostring_ref strRef{ &vm->result.msg };
		print(strRef, u"动态链接库 ", path, u" 发生错误");
	}

	void SetError_NegativeNumber(VM* vm, Int64 value) noexcept {
		vm->result.error = HYError::NEGATIVE_NUMBER;
		fast_io::u16ostring_ref strRef{ &vm->result.msg };
		print(strRef, u"数值为负数 ", value);
	}

	void SetError_EmptyContainer(VM* vm, const StringView name, TypeObject* t) noexcept {
		vm->result.error = HYError::EMPTY_CONTAINER;
		fast_io::u16ostring_ref strRef{ &vm->result.msg };
		print(strRef, name, u" 处理 ", t->v_name, u" 空容器");
	}

	void SetError_AssertFalse(VM* vm, Index pos) noexcept {
		vm->result.error = HYError::ASSERT_FALSE;
		fast_io::u16ostring_ref strRef{ &vm->result.msg };
		print(strRef, u"断言条件 ", pos, u" 失败");
	}

	void SetError_UnsupportedScan(VM* vm, TypeObject* t) noexcept {
		vm->result.error = HYError::UNSUPPORTED_SCAN;
		fast_io::u16ostring_ref strRef{ &vm->result.msg };
		print(strRef, t->v_name, u" 不支持扫描");
	}

	void SetError_IODeviceError(VM* vm) noexcept {
		vm->result.error = HYError::IO_DEVICE_ERROR;
		fast_io::u16ostring_ref strRef{ &vm->result.msg };
		print(strRef, u"IO设备发生错误");
	}

	void SetError_ScanError(VM* vm, TypeObject* t) noexcept {
		vm->result.error = HYError::SCAN_ERROR;
		fast_io::u16ostring_ref strRef{ &vm->result.msg };
		print(strRef, u"扫描类型 ", t->v_name, u" 时发生错误");
	}
}

namespace hy {
	extern "C" __declspec(dllexport)
		void GetCompileErrorString(const StringView name, CodeResult cr, String * str) noexcept {
		fast_io::u16ostring_ref strRef{ str };
		print(strRef, u"编译错误 < ", name, u" : ", cr.line, u" > ",
			IsSyntaxError(cr.error) ? GenerateSyntaxError(cr.error) : GenerateLexError(cr.error), u" ");
		if (cr.start) print(strRef, StringView(cr.start, cr.end - cr.start + 1));
	}

	extern "C" __declspec(dllexport)
		void VMStackTrace(CallStackTrace * cst, VM * vm) noexcept {
		cst->error = vm->result.error;
		cst->msg = vm->result.msg;
		if (!vm->callStack.empty()) {
			cst->errStack.reserve(vm->callStack.cnt);
			for (auto& call : vm->callStack)
				cst->errStack.emplace_back(call.insView->calcLine(call.pIns), call.mod->name, call.name);
		}
	}
}