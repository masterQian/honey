/**************************************************
*
* @文件				hy.syntaxer
* @作者				钱浩宇
* @创建时间			2021-11-05
* @更新时间			2022-11-29
* @摘要
* 汉念语法分析器的声明
*
**************************************************/

#pragma once

#include "../lexer/hy.lexer.h"

namespace hy {
	// 语法结构符号
	enum class SyntaxToken : Token {
		/*
			第一段的Syntax符号值不能改动, 需与Lex符号值一致
		*/
		LT = 0x0007U,		// 小于号
		GT = 0x0008U,		// 大于号
		ADD = 0x0014U,		// 加号
		SUBTRACT = 0x0015U,		// 减号
		MULTIPLE = 0x0016U,		// 乘号
		DIVIDE = 0x0017U,		// 除号
		MOD = 0x0018U,		// 百分号
		POWER = 0x0019U,		// 幂号
		NEGATIVE = 0x001AU,		// 负号
		EQ = 0x0020U,		// 等于号
		GE = 0x0021U,		// 大于等于号
		LE = 0x0022U,		// 小于等于号
		NE = 0x0023U,		// 不等于号
		AND = 0x0024U,		// 与
		OR = 0x0025U,		// 或
		NOT = 0x0026U,		// 非

		REF = 0x4000U,      // 引用
		INDEX = 0x4001U,		// 索引元素
		CALL = 0x4002U,		// 函数调用
		MEMBER = 0x4003U,		// 成员引用
		CV = 0x4004U,      // 常量
		List = 0x4005U,      // 列表
		Dict = 0x4006U,      // 字典
		Vector = 0x4007U,      // 向量
		Matrix = 0x4008U,      // 矩阵
		Range = 0x4009U,      // 范围
		Args = 0x4010U,      // 参数列表
		TArg = 0x4020U,      // 形参
		TArgs = 0x4021U,      // 形参列表
		AutoBind = 0x4030U,		// 结构化绑定
		Lambda = 0x4050U,      // Lambda表达式

		ABS = 0x4100U,      // 结构化绑定语句
		CDS = 0x4110U,      // 概念定义语句
		FDS = 0x4120U,      // 函数定义语句
		TDS = 0x4130U,      // 类定义语句
		TDS_V = 0x4131U,      // 类成员变量定义语句
		TDS_F = 0x4132U,      // 类成员函数定义语句
		TDS_M = 0x4133U,      // 类成员变量定义表
		IS = 0x4150U,      // 模块导入语句
		Module = 0x4151U,      // 模块
		NS = 0x4160U,		// 原生语句
		AS = 0x4170U,      // 赋值语句
		ES = 0x4180U,      // 表达式语句
		FRS = 0x4190U,      // 函数返回语句
		CS = 0x41A0U,      // 条件语句
		LS = 0x41B0U,      // 循环语句
		ILS = 0x41C0U,      // 迭代语句
		LCS = 0x41D0U,      // 继续循环语句
		LBS = 0x41E0U,      // 退出循环语句
		SS = 0x41F0U,      // 分支语句
		SL = 0x41F1U,      // 分支
		US = 0x4200U,      // 命名空间语句
		CVDS = 0x4210U,      // 常量定义语句
		GVDS = 0x4220U,		// 全局变量定义语句
		Block = 0x4800U,      // 语句块
		Statements = 0x4900U,      // 语句集合
		Program = 0x4F00U,      // 程序

		START = LT,			// 语法结构开始
		END = Program,      // 语法结构结束
	};

	// 语法树结点
	struct ASTNode {
		using ASTList = Vector<ASTNode>;

		struct ASTLeaf {
			Lex* lex;
			CMemory unused;
		};

		struct ASTRoot {
			SyntaxToken token;
			Index32 line; // 起始行
			ASTList* childs; // 子节点
		};

		union ASTValue {
			ASTLeaf leaf;
			ASTRoot root;
		}v;

		ASTNode(Lex* p = { }) noexcept {
			v.leaf.lex = p;
			v.leaf.unused = nullptr;
		}

		ASTNode(SyntaxToken st, Index32 line) noexcept {
			v.root.token = st;
			v.root.line = line;
			v.root.childs = new ASTList;
		}

		~ASTNode() noexcept {
			if (v.root.childs) delete v.root.childs;
		}

		ASTNode(const ASTNode&) = delete;
		ASTNode& operator = (const ASTNode&) = delete;

		ASTNode(ASTNode&& ast) noexcept {
			freestanding::moveset(v, ast.v);
		}

		ASTNode& operator = (ASTNode&& ast) noexcept {
			freestanding::swap(v, ast.v);
			return *this;
		}

		// 根
		bool isRoot() const noexcept { 
			return v.root.childs != nullptr;
		}

		Size size() const noexcept {
			return v.root.childs->size();
		}

		ASTNode* begin() noexcept {
			return v.root.childs->data();
		}

		ASTNode* end() noexcept {
			return v.root.childs->data() + v.root.childs->size();
		}

		SyntaxToken& syntaxToken() noexcept { 
			return v.root.token;
		}

		ASTList* child() const noexcept { 
			return v.root.childs;
		}

		ASTNode& front() noexcept {
			return v.root.childs->front();
		}

		ASTNode& back() noexcept {
			return v.root.childs->back();
		}

		void reserve(Size len) noexcept {
			v.root.childs->reserve(len);
		}

		template<typename... Args>
		ASTNode& push(Args&&... args) noexcept {
			return v.root.childs->emplace_back(freestanding::forward<Args>(args)...);
		}

		ASTNode& operator[] (Index index) noexcept { 
			return (*v.root.childs)[index];
		}

		const ASTNode& operator [] (Index index) const noexcept {
			return (*v.root.childs)[index];
		}

		void line(Index32 l) noexcept {
			if (isRoot()) v.root.line = l;
		}

		// 叶子
		bool isLeaf() const noexcept {
			return v.leaf.unused == nullptr;
		}

		Lex* lex() noexcept {
			return v.leaf.lex;
		}

		LexToken lexToken() const noexcept {
			return v.leaf.lex->token;
		}

		CStr lexStart() const noexcept { 
			return v.leaf.lex->start;
		}

		CStr lexEnd() const noexcept { 
			return v.leaf.lex->end;
		}

		// 公共
		Index32 line() const noexcept {
			return isRoot() ? v.root.line : v.leaf.lex->line;
		}

		void reset(Lex* p = { }) noexcept {
			if (isRoot()) delete v.root.childs;
			v.leaf.lex = p;
			v.leaf.unused = nullptr;
		}

		void reset(SyntaxToken st, Index32 line) noexcept {
			if (isRoot()) delete v.root.childs;
			v.root.token = st;
			v.root.line = line;
			v.root.childs = new ASTList;
		}
	};
}

namespace hy {
	// 语法分析器配置
	struct SyntaxerConfig {

	};

	// 语法分析器
	struct Syntaxer {
		constexpr static auto AST_IS{ 0ULL };
		constexpr static auto AST_US{ 1ULL };
		constexpr static auto AST_NS{ 2ULL };
		constexpr static auto AST_CVDS{ 3ULL };
		constexpr static auto AST_TDS{ 4ULL };
		constexpr static auto AST_CDS{ 5ULL };
		constexpr static auto AST_FDS{ 6ULL };
		constexpr static auto AST_GVDS{ 7ULL };
		constexpr static auto AST_S{ 8ULL };
		constexpr static auto AST_COUNT{ AST_S + 1ULL };

		ASTNode root;
		String source;
		SyntaxerConfig cfg;

		Syntaxer() noexcept : root{ SyntaxToken::Program, 0U } {}

		Syntaxer(const Syntaxer&) = delete;
		Syntaxer& operator = (const Syntaxer&) = delete;
	};
}