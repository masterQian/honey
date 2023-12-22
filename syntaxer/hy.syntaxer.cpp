/**************************************************
*
* @文件				hy.syntaxer
* @作者				钱浩宇
* @创建时间			2021-11-05
* @更新时间			2022-11-29
* @摘要
* 汉念语法分析器的实现
*
**************************************************/

#include "hy.syntaxer.h"
#include "../public/hy.error.h"

namespace hy {
	struct SyntaxerData {
		Size32 LoopCount{ };
		Size32 FunctionCount{ };
		Size32 ClassCount{ };
		Size32 TotalFunctionCount{ };
	};

	struct SyntaxRet {
		HYError err;
		Size32 unused;
		Lex* end;
		inline constexpr SyntaxRet(HYError error = HYError::NO_ERROR, Lex* lex = { },
			Size32 v = { }) noexcept : err{ error }, end{ lex }, unused{ v } {}
	};

#define case2(a,b) case a: case b
#define case3(a,b,c) case2(a,b): case c
#define case4(a,b,c,d) case3(a,b,c): case d
#define case5(a,b,c,d,e) case4(a,b,c,d): case e
#define case6(a,b,c,d,e,f) case5(a,b,c,d,e): case f
#define case7(a,b,c,d,e,f,g) case6(a,b,c,d,e,f): case g
#define case8(a,b,c,d,e,f,g,h) case7(a,b,c,d,e,f,g): case h
#define case9(a,b,c,d,e,f,g,h,i) case8(a,b,c,d,e,f,g,h): case i
#define case10(a,b,c,d,e,f,g,h,i,j) case9(a,b,c,d,e,f,g,h,i): case j

	template<LexToken... __Expects>
	inline constexpr bool TestExpect(LexToken token) noexcept {
		constexpr LexToken Expects[] { __Expects... };
		for (Index i{ }; i < sizeof...(__Expects); ++i) {
			if (Expects[i] == token) return true;
		}
		return false;
	}
}

namespace hy {
	template<LexToken T, LexToken... Ts>
	concept not_in_expects = ((T != Ts) && ...);

	template<LexToken... token>
	concept test_expect_expression =
		not_in_expects<LexToken::OR, token...>&&
		not_in_expects<LexToken::SMALL_LEFT, token...>;

	using StatusStack = util::FixedStack<char, 32ULL>;
	using ASTStack = util::Stack<ASTNode>;

	template<char normal>
	inline constexpr void GOTO_1(StatusStack& ss) noexcept {
		ss.pop();
		ss.push(normal);
	}

	template<char sp, char normal>
	inline constexpr void GOTO_2(StatusStack& ss) noexcept {
		ss.pop();
		auto c{ ss.top() };
		if (c == sp) ++c;
		else c = normal;
		ss.push(c);
	}

	template<char sp1, char sp2, char normal>
	inline constexpr void GOTO_3(StatusStack& ss) noexcept {
		ss.pop();
		auto c{ ss.top() };
		if (c == sp1 || c == sp2) ++c;
		else c = normal;
		ss.push(c);
	}

#define GOTO_E GOTO_2<1, 25>
#define GOTO_E1 GOTO_2<3, 27>
#define GOTO_E2 GOTO_2<5, 28>
#define GOTO_E3 GOTO_2<7, 29>
#define GOTO_E4 GOTO_2<9, 30>
#define GOTO_E5 GOTO_2<11, 31>
#define GOTO_E6 GOTO_2<13, 32>
#define GOTO_E7 GOTO_3<15, 17, 33>
#define GOTO_E8 GOTO_1<19>

	inline void OPER1(StatusStack& ss, ASTStack& as) noexcept {
		auto op{ as.pop_move() };
		auto opt{ as.pop_move() }; // 取出操作数与运算符
		opt.push(freestanding::move(op)); // 运算符添加操作数
		as.push(freestanding::move(opt)); // 压入运算符
		ss.pop();
		/*
		*	最后根据ss.top()来决定转移方向
		*	一元运算符是左结合的
		*	不能满足OPER2那种特殊转移
		*/
		GOTO_E7(ss);
	}

	inline void OPER2(StatusStack& ss, ASTStack& as) noexcept {
		/*
		*	因为左递归的文法保证了双操作数出栈后的转移一定是栈顶
		*	例如有文法 X -> X OPER Y
		*	ss: X OPER Y
		*	as: S1 S2 S3 S4
		*	则出栈X OPER Y和S2 S3 S4后S1状态接受归约的X符号一定会转移到S2
		*	所以只需维持以下栈状态即可
		*	ss: X
		*	as: S1 S2
		*/
		auto op2{ as.pop_move() };
		auto opt{ as.pop_move() };
		auto op1{ as.pop_move() };
		opt.push(freestanding::move(op1));
		opt.push(freestanding::move(op2));
		as.push(freestanding::move(opt));
		ss.pop(); ss.pop(); // 最后无需根据ss.top()来决定转移方向
	}

	inline void Action(StatusStack& ss, ASTStack& as, Lex*& p, char status) noexcept {
		ss.push(status);
		as.push(static_cast<SyntaxToken>(p->token), p->line);
		++p;
	}

	inline void ActionBrackets(StatusStack& ss, Lex*& p, char status) noexcept {
		ss.push(status);
		++p;
	}

	template<HYError newError>
	inline SyntaxRet& ErrorChange(SyntaxRet& ret) noexcept {
		if (ret.err == HYError::EXPECT_ERROR) ret.err = newError;
		return ret;
	}
}

namespace hy {
	SyntaxRet TestNSt(SyntaxerData& data, ASTNode& ast, Lex* lex) noexcept;
	SyntaxRet ExpectBlock(SyntaxerData& data, ASTNode& ast, Lex* lex) noexcept;

	template<LexToken... __Expects>
	requires test_expect_expression<__Expects...>
	SyntaxRet TestE(SyntaxerData& data, ASTNode& ast, Lex* lex) noexcept;

	// 提取常量 C
	// INTEGER | LITERAL | DECIMAL | COMPLEX | true | false
	template<LexToken... __Expects>
	inline SyntaxRet ExtractC(SyntaxerData&, ASTNode& ast, Lex* lex) noexcept {
		using enum LexToken;
		switch (lex->token) {
		case6(INTEGER, DECIMAL, COMPLEX, TRUE, FALSE, LITERAL) : {
			if (TestExpect<__Expects...>(lex[1].token)) {
				ast.push(lex);
				return { HYError::NO_ERROR, lex }; // 归约成功
			}
			return { HYError::EXPECT_ERROR, lex + 1 }; // 后缀错误
		}
		default: return { HYError::NOT_C, lex }; // 非常值
		}
	}

	// [ID] 归约引用 REF
	// ID | REF -> ID
	template<LexToken con, LexToken... __Expects>
	inline SyntaxRet ExpectRef(SyntaxerData&, ASTNode& ast, Lex* lex) noexcept {
		auto p{ lex };
		ASTNode ast_ref{ SyntaxToken::REF, lex->line };
		for (; p->token == LexToken::ID;) {
			ast_ref.push(p);
			++p;
			if (p->token == con) ++p;
			else if (TestExpect<__Expects...>(p->token)) {
				ast.push(freestanding::move(ast_ref));
				return { HYError::NO_ERROR, p - 1 }; // 归约成功
			}
			else return { HYError::EXPECT_ERROR, p }; // 后缀错误
		}
		return { HYError::EXPECT_ID, p }; // 后缀不是ID
	}

	// [AT] 归约常量 CV
	// AT REF
	template<LexToken... __Expects>
	inline SyntaxRet ExpectCV(SyntaxerData& data, ASTNode& ast, Lex* lex) noexcept {
		auto p{ lex + 1 };
		if (p->token != LexToken::ID) return { HYError::EXPECT_ID, p }; // 缺少名称
		SyntaxRet sr{ ExpectRef<LexToken::DOM, __Expects...>(data, ast, p) };
		if (~sr.err) ast.back().syntaxToken() = SyntaxToken::CV; // 归约成功
		return sr;
	}

	// 测试参数列表 Args
	// Args -> E, E, ... | epsilon
	// NotNull要求Args不能为空参数列表
	template<LexToken __Expects, bool NotNull = false>
	inline SyntaxRet TestArgs(SyntaxerData& data, ASTNode& ast, Lex* lex) noexcept {
		ASTNode ast_args{ SyntaxToken::Args, lex->line };
		Lex* p;
		for (p = lex; p->token != __Expects;) {
			SyntaxRet sr_e{ TestE<LexToken::COMMA, __Expects>(data, ast_args, p) };
			if (!sr_e.err) return sr_e;
			p = sr_e.end + 1;
			if (p->token == LexToken::COMMA) ++p;
		}
		if (!NotNull || p != lex) {
			ast.push(freestanding::move(ast_args));
			return { HYError::NO_ERROR, p - 1 };
		}
		return { HYError::EMPTY_ARGUMENTS, p - 1 };
	}

	// [{] 归约列表 List
	// List -> { LI } | { }
	// LI -> LI, E | E
	template<LexToken... __Expects>
	inline SyntaxRet ExpectList(SyntaxerData& data, ASTNode& ast, Lex* lex) noexcept {
		SyntaxRet sr_args{ TestArgs<LexToken::LARGE_RIGHT>(data, ast, lex + 1) };
		if (!sr_args.err) return sr_args;
		if (TestExpect<__Expects...>(sr_args.end[2].token)) {
			auto& ast_list{ ast.back() };
			ast_list.syntaxToken() = SyntaxToken::List;
			ast_list.line(lex->line);
			return { HYError::NO_ERROR, sr_args.end + 1 };
		}
		return { HYError::EXPECT_ERROR, sr_args.end + 2 };
	}

	// [{] 归约字典 Dict
	// Dict -> { DI }
	// DI -> DI, INTEGER : E | INTEGER : E
	// DI -> DI, LITERAL : E | LITERAL : E
	template<LexToken... __Expects>
	inline SyntaxRet ExpectDict(SyntaxerData& data, ASTNode& ast, Lex* lex) noexcept {
		auto p{ lex + 1 };
		ASTNode ast_dict{ SyntaxToken::Dict, lex->line };
		for (; p->token != LexToken::LARGE_RIGHT;) {
			SyntaxRet sr_key{ TestE<LexToken::COLON>(data, ast_dict, p) };
			if (!sr_key.err) return ErrorChange<HYError::EXPECT_COLON>(sr_key); // 提取键错误
			p = sr_key.end + 2; // 跳过冒号
			SyntaxRet sr_value{ TestE<LexToken::COMMA, LexToken::LARGE_RIGHT>(data, ast_dict, p) };
			if (!sr_value.err) return ErrorChange<HYError::EXPECT_LARGE_RIGHT>(sr_value); // 提取值错误
			p = sr_value.end + 1;
			if (p->token == LexToken::COMMA) ++p;
		}
		if (TestExpect<__Expects...>(p[1].token)) {
			ast.push(freestanding::move(ast_dict));
			return { HYError::NO_ERROR, p };
		}
		return { HYError::EXPECT_ERROR, p + 1 };
	}

	// [[] 归约向量 Vector
	// Vector -> [ VI ] | [ ]
	// VI -> VI, INTEGER | VI, DECIMAL | INTEGER | DECIMAL
	template<LexToken... __Expects>
	inline SyntaxRet ExpectVector(SyntaxerData&, ASTNode& ast, Lex* lex) noexcept {
		auto p{ lex + 1 };
		if (p->token == LexToken::MID_RIGHT) { // 空向量
			if (TestExpect<__Expects...>(p[1].token)) {
				ast.push(SyntaxToken::Vector, lex->line);
				return { HYError::NO_ERROR, p };
			}
			return { HYError::EXPECT_ERROR, p + 1 };
		}
		// 非空向量
		ASTNode ast_vector{ SyntaxToken::Vector, lex->line };
		for (; p->token != LexToken::MID_RIGHT;) {
			if (p->token != LexToken::INTEGER && p->token != LexToken::DECIMAL)
				return { HYError::INVALID_VECTOR_ATOM, p };
			ast_vector.push(p);
			++p;
			if (p->token == LexToken::COMMA) ++p;
			else if (p->token != LexToken::MID_RIGHT) return { HYError::EXPECT_MID_RIGHT, p };
		}
		if (TestExpect<__Expects...>(p[1].token)) {
			ast.push(freestanding::move(ast_vector));
			return { HYError::NO_ERROR, p };
		}
		return { HYError::EXPECT_ERROR, p + 1 };
	}

	// [[] 归约矩阵 Matrix
	// Matrix -> [ MI ]
	// MI -> MI, Vector | Vector
	template<LexToken... __Expects>
	inline SyntaxRet ExpectMatrix(SyntaxerData& data, ASTNode& ast, Lex* lex) noexcept {
		auto p{ lex + 1 };
		ASTNode ast_matrix{ SyntaxToken::Matrix, lex->line };
		for (; p->token != LexToken::MID_RIGHT;) {
			if (p->token != LexToken::MID_LEFT) return { HYError::EXPECT_MID_LEFT, p };
			SyntaxRet sr_vector{ ExpectVector<LexToken::COMMA, LexToken::MID_RIGHT>(data, ast_matrix, p) };
			if (!sr_vector.err) return ErrorChange<HYError::EXPECT_MID_RIGHT>(sr_vector);
			p = sr_vector.end + 1;
			if (p->token == LexToken::COMMA) ++p;
		}
		if (TestExpect<__Expects...>(p[1].token)) {
			ast.push(freestanding::move(ast_matrix));
			return { HYError::NO_ERROR, p };
		}
		return { HYError::EXPECT_ERROR, p + 1 };
	}

	// [INTEGER] 归约范围 Range
	// Range -> INTEGER ~ INTEGER | INTEGER ~ INTEGER ~ INTEGER
	template<LexToken... __Expects>
	inline SyntaxRet ExpectRange(SyntaxerData&, ASTNode& ast, Lex* lex) noexcept {
		auto p{ lex + 1 };
		if (p->token != LexToken::CONNECTOR) return { HYError::EXPECT_CONNECTOR, p }; // 缺少连接符
		if (p[1].token != LexToken::INTEGER) return { HYError::INVALID_RANGE_ATOM, p + 1 }; // 非范围元素
		ASTNode ast_range{ SyntaxToken::Range, lex->line };
		ast_range.push(p - 1);
		ast_range.push(p + 1);
		p += 2;
		if (p->token == LexToken::CONNECTOR) {
			if (p[1].token != LexToken::INTEGER) return { HYError::INVALID_RANGE_ATOM, p + 1 };
			ast_range.push(p + 1);
			p += 2;
			if (TestExpect<__Expects...>(p->token)) {
				ast.push(freestanding::move(ast_range));
				return { HYError::NO_ERROR, p - 1 };
			}
			return { HYError::EXPECT_ERROR, p };
		}
		else if (TestExpect<__Expects...>(p->token)) {
			ast.push(freestanding::move(ast_range));
			return { HYError::NO_ERROR, p - 1 };
		}
		return { HYError::EXPECT_ERROR, p };
	}

	// 提取原生对象常量 NOC
	template<LexToken... __Expects>
	inline SyntaxRet ExtractNOC(SyntaxerData& data, ASTNode& ast, Lex* lex) noexcept {
		switch (lex->token) {
		case LexToken::LARGE_LEFT: {
			SyntaxRet sr_list{ ExpectList<__Expects...>(data, ast, lex) };
			if (~sr_list.err) return sr_list;
			SyntaxRet sr_dict{ ExpectDict<__Expects...>(data, ast, lex) };
			if (~sr_dict.err || sr_dict.err == HYError::EXPECT_LARGE_RIGHT
				|| sr_dict.err == HYError::EXPECT_ERROR) return sr_dict;
			else if (sr_dict.err == HYError::NOT_V) return { HYError::INVALID_SYMBOL, sr_dict.end };
			return sr_list;
		}
		case LexToken::MID_LEFT: return lex[1].token == LexToken::MID_LEFT ?
			ExpectMatrix<__Expects...>(data, ast, lex) :
			ExpectVector<__Expects...>(data, ast, lex);
		case LexToken::INTEGER: return ExpectRange<__Expects...>(data, ast, lex);
		}
		return { HYError::NOT_NOC, lex };
	}

	// 测试形参定义列表 TArgs
	// TArgs -> ... | TArg, ... | epsilon
	// TArg -> TArg, ID : Ref | TArg, ID | ID : Ref | ID
	template<LexToken __Expects>
	inline SyntaxRet TestTArgs(SyntaxerData& data, ASTNode& ast, Lex* lex) noexcept {
		ASTNode ast_targs{ SyntaxToken::TArgs, lex->line };
		Lex* p;
		for (p = lex; p->token != __Expects;) {
			if (p->token == LexToken::ETC) {
				ast_targs.push(p);
				++p;
				if (p->token != __Expects) return { HYError::ETC_POSITION_ERROR, p - 1 };
			}
			else if (p->token == LexToken::ID) {
				ASTNode ast_targ{ SyntaxToken::TArg, p->line };
				ast_targ.push(p);
				++p;
				if (p->token == LexToken::COLON) {
					++p;
					if (p->token != LexToken::ID) return { HYError::EXPECT_ID, p };
					SyntaxRet sr_ref{ ExpectRef<LexToken::DOM, LexToken::COMMA, __Expects>(data, ast_targ, p) };
					if (!sr_ref.err) return ErrorChange<HYError::EXPECT_COMMA>(sr_ref);
					p = sr_ref.end + 1;
				}
				ast_targs.push(freestanding::move(ast_targ));
				if (p->token == LexToken::COMMA) ++p;
				else if (p->token != __Expects) return { HYError::EXPECT_ERROR, p };
			}
			else return { HYError::EXPECT_ID, p };
		}
		ast.push(freestanding::move(ast_targs));
		return { HYError::NO_ERROR, p - 1 };
	}

	// [function] 归约Lambda表达式 lambda
	// lambda -> function ( TArgs ) -> REF Block
	// lambda -> function ( TArgs ) Block
	template<LexToken... __Expects>
	inline SyntaxRet ExpectLambda(SyntaxerData& data, ASTNode& ast, Lex* lex) noexcept {
		auto p{ lex + 1 };
		if (p->token != LexToken::SMALL_LEFT) return { HYError::EXPECT_SMALL_LEFT, p };
		ASTNode ast_lambda{ SyntaxToken::Lambda, lex->line };
		SyntaxRet sr_targs{ TestTArgs<LexToken::SMALL_RIGHT>(data, ast_lambda, p + 1) };
		if (!sr_targs.err) return ErrorChange<HYError::EXPECT_SMALL_RIGHT>(sr_targs);
		p = sr_targs.end + 2;
		if (p->token == LexToken::POINTER) {
			++p;
			if (p->token != LexToken::ID) return { HYError::EXPECT_ID, p };
			SyntaxRet sr_ref{ ExpectRef<LexToken::DOM, LexToken::LARGE_LEFT>(data, ast_lambda, p) };
			if (!sr_ref.err) return ErrorChange<HYError::EXPECT_LARGE_LEFT>(sr_ref);
			p = sr_ref.end + 1;
		}
		if (p->token != LexToken::LARGE_LEFT) return { HYError::EXPECT_LARGE_LEFT, p };
		++data.FunctionCount;
		SyntaxRet sr_b{ ExpectBlock(data, ast_lambda, p) };
		--data.FunctionCount;
		if (!sr_b.err) return sr_b;
		ast_lambda.push(lex); ast_lambda.push(sr_b.end); // 标记lambda的头尾
		ast.push(freestanding::move(ast_lambda));
		return { HYError::NO_ERROR, sr_b.end };
	}

	// 提取值 V
	// C | CV | this | REF | NOC | lambda
	template<LexToken... __Expects>
	inline SyntaxRet ExtractV(SyntaxerData& data, ASTNode& ast, Lex* lex) noexcept {
		SyntaxRet sr_noc{ ExtractNOC<__Expects...>(data, ast, lex) };
		if (~sr_noc.err || sr_noc.err == HYError::EXPECT_ERROR) return sr_noc;
		SyntaxRet sr_c{ ExtractC<__Expects...>(data, ast, lex) };
		if (~sr_c.err || sr_c.err == HYError::EXPECT_ERROR) return sr_c;
		if (lex->token == LexToken::ID) return ExpectRef<LexToken::DOM, __Expects...>(data, ast, lex);
		else if (lex->token == LexToken::THIS) {
			if (data.ClassCount == 0U || data.FunctionCount == 0U)
				return { HYError::STATEMENT_THIS_ERROR, lex };
			ast.push(lex);
			return { HYError::NO_ERROR, lex };
		}
		else if (lex->token == LexToken::AT) return ExpectCV<__Expects...>(data, ast, lex);
		else if (lex->token == LexToken::FUNCTION) return ExpectLambda<__Expects...>(data, ast, lex);
		return { HYError::NOT_V, lex };
	}

	// 测试表达式 E
	// E -> E1, E | E1
	// E1 -> E2, E1 & E2
	// E2 -> E3, E2 == E3, E2 != E3
	// E3 -> E4, E3 > E4, E3 >= E4, E3 < E4, E3 <= E4
	// E4 -> E5, E4 + E5, E4 - E5
	// E5 -> E6, E5 * E6, E5 / E6, E5 % E6
	// E6 -> E7, E6 ^ E7
	// E7 -> E8, -E7, !E7
	// E8 -> V, ( E ), E8[Args], E8(Args), E8.ID
#define EP1 __Expects..., LexToken::OR
#define EP2 EP1, LexToken::AND
#define EP3 EP2, LexToken::EQ, LexToken::NE
#define EP4 EP3, LexToken::ACUTE_RIGHT, LexToken::GE, LexToken::ACUTE_LEFT, LexToken::LE
#define EP5 EP4, LexToken::ADD, LexToken::SUBTRACT
#define EP6 EP5, LexToken::MULTIPLE, LexToken::DIVIDE, LexToken::MOD
#define EP7 EP6, LexToken::POWER
#define EP8 EP7, LexToken::MID_LEFT, LexToken::SMALL_LEFT, LexToken::DOT
#define EPB(ep) ep, LexToken::SMALL_RIGHT
	// 所有的Test_Expect优先级低于其他符号的test, 因为要对其他符号归约贯彻到底
	template<LexToken... __Expects>
	requires test_expect_expression<__Expects...>
	SyntaxRet TestE(SyntaxerData& data, ASTNode& ast, Lex* lex) noexcept {
		StatusStack ss;
		ss.push(1);
		ASTStack as;
		for (auto p{ lex };;) {
			switch (ss.top()) {
			case10(1, 3, 5, 7, 9, 11, 13, 15, 17, 24) : {
				if (p->token == LexToken::SMALL_LEFT) ActionBrackets(ss, p, 24);
				else if (p->token == LexToken::NEGATIVE || p->token == LexToken::NOT) Action(ss, as, p, 17);
				else { // ExtractV
					ASTNode tmp{ SyntaxToken::Program, 0U };
					SyntaxRet sr_v{ ExtractV<EPB(EP8)>(data, tmp, p) };
					if (~sr_v.err) {
						ss.push(23);
						as.push(freestanding::move(tmp.back()));
						p = sr_v.end + 1;
					}
					else return sr_v;
				}
				break;
			}
			case 2: {
				if (p->token == LexToken::OR) Action(ss, as, p, 3);
				else if (TestExpect<__Expects...>(p->token)) {
					ast.push(as.pop_move());
					return { HYError::NO_ERROR, p - 1 };
				}
				else return { HYError::EXPECT_ERROR, p };
				break;
			}
			case 4: {
				if (p->token == LexToken::AND) Action(ss, as, p, 5);
				else if (TestExpect<EPB(EP1)>(p->token)) OPER2(ss, as);
				else return { HYError::EXPECT_ERROR, p };
				break;
			}
			case 6: {
				if (p->token == LexToken::EQ || p->token == LexToken::NE) Action(ss, as, p, 7);
				else if (TestExpect<EPB(EP2)>(p->token)) OPER2(ss, as);
				else return { HYError::EXPECT_ERROR, p };
				break;
			}
			case 8: {
				if (p->token == LexToken::ACUTE_RIGHT || p->token == LexToken::GE
					|| p->token == LexToken::ACUTE_LEFT || p->token == LexToken::LE)
					Action(ss, as, p, 9);
				else if (TestExpect<EPB(EP3)>(p->token)) OPER2(ss, as);
				else return { HYError::EXPECT_ERROR, p };
				break;
			}
			case 10: {
				if (p->token == LexToken::ADD || p->token == LexToken::SUBTRACT) Action(ss, as, p, 11);
				else if (TestExpect<EPB(EP4)>(p->token)) OPER2(ss, as);
				else return { HYError::EXPECT_ERROR, p };
				break;
			}
			case 12: {
				if (p->token == LexToken::MULTIPLE || p->token == LexToken::DIVIDE ||
					p->token == LexToken::MOD) Action(ss, as, p, 13);
				else if (TestExpect<EPB(EP5)>(p->token)) OPER2(ss, as);
				else return { HYError::EXPECT_ERROR, p };
				break;
			}
			case 14: {
				if (p->token == LexToken::POWER) Action(ss, as, p, 15);
				else if (TestExpect<EPB(EP6)>(p->token)) OPER2(ss, as);
				else return { HYError::EXPECT_ERROR, p };
				break;
			}
			case 16: {
				if (TestExpect<EPB(EP7)>(p->token)) OPER2(ss, as);
				else return { HYError::EXPECT_ERROR, p };
				break;
			}
			case 18: {
				if (TestExpect<EPB(EP7)>(p->token)) OPER1(ss, as);
				else return { HYError::EXPECT_ERROR, p };
				break;
			}
			case 19: {
				if (p->token == LexToken::MID_LEFT) {
					ASTNode tmp{ SyntaxToken::Program, 0U };
					SyntaxRet sr_args{ TestArgs<LexToken::MID_RIGHT, true>(data, tmp, p + 1) };
					if (~sr_args.err) {
						as.push(SyntaxToken::INDEX, p->line);
						as.push(freestanding::move(tmp.back()));
						ss.push(20); ss.push(20); // 配合OPER2需要弹出两个使得回到19状态
						p = sr_args.end + 2;
					}
					else return sr_args;
				}
				else if (p->token == LexToken::SMALL_LEFT) {
					ASTNode tmp{ SyntaxToken::Program, 0U };
					SyntaxRet sr_args{ TestArgs<LexToken::SMALL_RIGHT, false>(data, tmp, p + 1) };
					if (~sr_args.err) {
						as.push(SyntaxToken::CALL, p->line);
						as.push(freestanding::move(tmp.back()));
						ss.push(21); ss.push(21); // 配合OPER2需要弹出两个使得回到19状态
						p = sr_args.end + 2;
					}
					else return sr_args;
				}
				else if (p->token == LexToken::DOT) {
					if (p[1].token == LexToken::ID) {
						as.push(SyntaxToken::MEMBER, p->line);
						as.push(p + 1);
						ss.push(22); ss.push(22); // 配合OPER2需要弹出两个使得回到19状态
						p += 2;
					}
					else return { HYError::EXPECT_ID, p + 1 };
				}
				else if (TestExpect<EPB(EP7)>(p->token)) GOTO_E7(ss);
				else return { HYError::EXPECT_ERROR, p };
				break;
			}
			case3(20, 21, 22) : {
				if (TestExpect<EPB(EP8)>(p->token)) OPER2(ss, as);
				else return { HYError::EXPECT_ERROR, p };
				break;
			}
			case 23: {
				if (TestExpect<EPB(EP8)>(p->token)) GOTO_E8(ss);
				else return { HYError::EXPECT_ERROR, p };
				break;
			}
			case 25: {
				if (p->token == LexToken::OR) Action(ss, as, p, 3);
				else if (p->token == LexToken::SMALL_RIGHT) ActionBrackets(ss, p, 26);
				else return { HYError::EXPECT_ERROR, p };
				break;
			}
			case 26: {
				if (TestExpect<EPB(EP8)>(p->token)) { // 归约括号
					// as栈中仅有E, ss栈中是24,25,26,归约成E8
					ss.pop(); ss.pop();
					GOTO_E8(ss);
				}
				else return { HYError::EXPECT_ERROR, p };
				break;
			}
			case 27: {
				if (p->token == LexToken::AND) Action(ss, as, p, 5);
				else if (TestExpect<EPB(EP1)>(p->token)) GOTO_E(ss);
				else return { HYError::EXPECT_ERROR, p };
				break;
			}
			case 28: {
				if (p->token == LexToken::EQ || p->token == LexToken::NE) Action(ss, as, p, 7);
				else if (TestExpect<EPB(EP2)>(p->token)) GOTO_E1(ss);
				else return { HYError::EXPECT_ERROR, p };
				break;
			}
			case 29: {
				if (p->token == LexToken::ACUTE_RIGHT || p->token == LexToken::GE ||
					p->token == LexToken::ACUTE_LEFT || p->token == LexToken::LE)
					Action(ss, as, p, 9);
				else if (TestExpect<EPB(EP3)>(p->token)) GOTO_E2(ss);
				else return { HYError::EXPECT_ERROR, p };
				break;
			}
			case 30: {
				if (p->token == LexToken::ADD || p->token == LexToken::SUBTRACT) Action(ss, as, p, 11);
				else if (TestExpect<EPB(EP4)>(p->token)) GOTO_E3(ss);
				else return { HYError::EXPECT_ERROR, p };
				break;
			}
			case 31: {
				if (p->token == LexToken::MULTIPLE || p->token == LexToken::DIVIDE ||
					p->token == LexToken::MOD) Action(ss, as, p, 13);
				else if (TestExpect<EPB(EP5)>(p->token)) GOTO_E4(ss);
				else return { HYError::EXPECT_ERROR, p };
				break;
			}
			case 32: {
				if (p->token == LexToken::POWER) Action(ss, as, p, 15);
				else if (TestExpect<EPB(EP6)>(p->token)) GOTO_E5(ss);
				else return { HYError::EXPECT_ERROR, p };
				break;
			}
			case 33: {
				if (TestExpect<EPB(EP7)>(p->token)) GOTO_E6(ss);
				else return { HYError::EXPECT_ERROR, p };
				break;
			}
			}
		}
	}

	// 归约结构化绑定
	// [<] AutoBind -> < ABL >
	// ABL -> ABL, ID | ABL, ... | ID | ...
	template<LexToken... __Expects>
	inline SyntaxRet ExpectAutoBind(SyntaxerData&, ASTNode& ast, Lex* lex) noexcept {
		auto p{ lex + 1 };
		if (p->token == LexToken::ACUTE_RIGHT) return { HYError::EMPTY_AUTOBIND, p };
		ASTNode ast_autobind{ SyntaxToken::AutoBind, lex->line };
		for (; p->token != LexToken::ACUTE_RIGHT;) {
			if (p->token == LexToken::ID || p->token == LexToken::ETC) {
				ast_autobind.push(p);
				++p;
				if (p->token == LexToken::COMMA) ++p;
			}
			else return { HYError::EXPECT_ID, p };
		}
		if (TestExpect<__Expects...>(p[1].token)) {
			ast.push(freestanding::move(ast_autobind));
			return { HYError::NO_ERROR, p };
		}
		return { HYError::EXPECT_ERROR, p + 1 };
	}

	// 测试概念 Concept
	// Concept -> Concept | C1, C1
	// C1 -> C1 & C2, C2
	// C2 -> REF, ( Concept ), !C2
#define CP1 __Expects..., LexToken::OR
#define CP2 CP1, LexToken::AND
#define CPB(cp) cp, LexToken::SMALL_RIGHT
	template<LexToken... __Expects>
	SyntaxRet TestConcept(SyntaxerData& data, ASTNode& ast, Lex* lex) noexcept {
		StatusStack ss;
		ss.push(1);
		ASTStack as;
		for (auto p{ lex };;) {
			switch (ss.top()) {
			case5(1, 3, 5, 8, 10) : {
				if (p->token == LexToken::SMALL_LEFT) ActionBrackets(ss, p, 10);
				else if (p->token == LexToken::NOT) Action(ss, as, p, 8);
				else if (p->token == LexToken::ID) { // expect_REF
					ASTNode tmp{ SyntaxToken::Program, 0U };
					SyntaxRet sr_ref{ ExpectRef<LexToken::DOM, CPB(CP2)>(data, tmp, p) };
					if (~sr_ref.err) {
						ss.push(7);
						as.push(freestanding::move(tmp.back()));
						p = sr_ref.end + 1;
					}
					else return sr_ref;
				}
				else return { HYError::INVALID_SYMBOL, p };
				break;
			}
			case 2: {
				if (p->token == LexToken::OR) Action(ss, as, p, 3);
				else if (TestExpect<__Expects...>(p->token)) {
					ast.push(as.pop_move());
					return { HYError::NO_ERROR, p - 1 };
				}
				else return { HYError::EXPECT_ERROR, p };
				break;
			}
			case 4: {
				if (p->token == LexToken::AND) Action(ss, as, p, 5);
				else if (TestExpect<CPB(CP1)>(p->token)) OPER2(ss, as);
				else return { HYError::EXPECT_ERROR, p };
				break;
			}
			case 6: {
				if (TestExpect<CPB(CP2)>(p->token)) OPER2(ss, as);
				else return { HYError::EXPECT_ERROR, p };
				break;
			}
			case 7: {
				if (TestExpect<CPB(CP2)>(p->token)) GOTO_3<5, 8, 14>(ss);
				else return { HYError::EXPECT_ERROR, p };
				break;
			}
			case 9: {
				if (TestExpect<CPB(CP2)>(p->token)) {
					ASTNode op{ as.pop_move() };
					ASTNode opt{ as.pop_move() };
					opt.push(freestanding::move(op));
					as.push(freestanding::move(opt));
					ss.pop();
					GOTO_3<5, 8, 14>(ss);
				}
				else return { HYError::EXPECT_ERROR, p };
				break;
			}
			case 11: {
				if (p->token == LexToken::OR) Action(ss, as, p, 3);
				else if (p->token == LexToken::SMALL_RIGHT) ActionBrackets(ss, p, 12);
				else return { HYError::EXPECT_ERROR, p };
				break;
			}
			case 12: {
				if (TestExpect<CPB(CP2)>(p->token)) { // 归约括号
					// as栈中仅有E, ss栈中是10,11,12,归约成C2
					ss.pop();
					ss.pop();
					ss.pop();
					ss.push(14);
				}
				else return { HYError::EXPECT_ERROR, p };
				break;
			}
			case 13: {
				if (p->token == LexToken::AND) Action(ss, as, p, 5);
				else if (TestExpect<CPB(CP1)>(p->token)) {
					ss.pop();
					auto des{ static_cast<char>(ss.top() + 1) };
					ss.push(des);
				}
				else return { HYError::EXPECT_ERROR, p };
				break;
			}
			case 14: {
				if (TestExpect<CPB(CP2)>(p->token)) GOTO_1<13>(ss);
				else return { HYError::EXPECT_ERROR, p };
				break;
			}
			}
		}
	}

	// 模块 Module
	// Module -> Module.ID | ID
	// Module -> Module::ID | ID
	template<LexToken sep, LexToken... __Expects>
	inline SyntaxRet TestModule(SyntaxerData& data, ASTNode& ast, Lex* lex) noexcept {
		if (lex->token != LexToken::ID) return { HYError::EXPECT_ID, lex };
		SyntaxRet sr_ref{ ExpectRef<sep, __Expects...>(data, ast, lex) };
		if (~sr_ref.err) ast.back().syntaxToken() = SyntaxToken::Module;
		return sr_ref;
	}

	// 规约左值 LV
	// [ID, this] LV -> REF | LV[Args] | LV.ID | this[Args] | this.ID
	template<LexToken... __Expects>
	inline SyntaxRet ExpectLV(SyntaxerData& data, ASTNode& ast, Lex* lex) noexcept {
		auto p{ lex };
		ASTNode ast_tmp{ SyntaxToken::Program, 0U };
		if (p->token == LexToken::ID) {
			SyntaxRet sr_ref{ ExpectRef<LexToken::DOM, __Expects...,
				LexToken::DOT, LexToken::MID_LEFT>(data, ast_tmp, p) };
			if (!sr_ref.err) return sr_ref;
			p = sr_ref.end + 1;
		}
		else {
			if (data.ClassCount == 0U || data.FunctionCount == 0U)
				return { HYError::STATEMENT_THIS_ERROR, p };
			ast_tmp.push(p);
			++p;
			if (!TestExpect<__Expects..., LexToken::DOT, LexToken::MID_LEFT>(p->token))
				return { HYError::EXPECT_ERROR, p };
		}
		for (ASTNode ast_root = freestanding::move(ast_tmp.back()); ;) {
			if (p->token == LexToken::DOT) {
				if (p[1].token != LexToken::ID) return { HYError::EXPECT_ID, p + 1 };
				if (TestExpect<__Expects..., LexToken::DOT, LexToken::MID_LEFT>(p[2].token)) {
					ASTNode ast_member{ SyntaxToken::MEMBER, p->line };
					freestanding::swap(ast_root, ast_member);
					ast_root.push(freestanding::move(ast_member));
					ast_root.push(p + 1);
					p += 2;
				}
				else return { HYError::EXPECT_ERROR, p + 2 };
			}
			else if (p->token == LexToken::MID_LEFT) {
				ASTNode ast_args{ SyntaxToken::INDEX, p->line };
				freestanding::swap(ast_root, ast_args);
				ast_root.push(freestanding::move(ast_args));
				SyntaxRet sr_args{ TestArgs<LexToken::MID_RIGHT, true>(data, ast_root, p + 1) };
				if (!sr_args.err) return sr_args;
				p = sr_args.end + 2;
				if (!TestExpect<__Expects..., LexToken::DOT, LexToken::MID_LEFT>(p->token))
					return { HYError::EXPECT_ERROR, p };
			}
			else {
				if (ast_root.isLeaf()) return { HYError::NOT_LV, ast_root.lex() }; // 仅this
				ast.push(freestanding::move(ast_root));
				return { HYError::NO_ERROR, p - 1 };
			}
		}
	}
}

namespace hy {
	// [import] 库导入语句 Import-Statement
	// IS -> import Module; | import using Module;
	SyntaxRet ExpectIS(SyntaxerData& data, ASTNode& ast, Lex* lex) noexcept {
		Lex* p{ lex + 1 }, * isUsing{ };
		if (p->token == LexToken::USING) { // import using
			isUsing = p;
			++p;
		}
		ASTNode ast_is{ SyntaxToken::IS, lex->line };
		SyntaxRet sr_module{ TestModule<LexToken::DOT, LexToken::SEMICOLON>(data, ast_is, p) };
		if (!sr_module.err) return ErrorChange<HYError::EXPECT_SEMICOLON>(sr_module);
		if (isUsing) ast_is.push(isUsing);
		ast.push(freestanding::move(ast_is));
		return { HYError::NO_ERROR, sr_module.end + 1 };
	}

	// [using] 命名空间语句 Using-Statement
	// US -> using Module; | using ID = Module;
	SyntaxRet ExpectUS(SyntaxerData& data, ASTNode& ast, Lex* lex) noexcept {
		auto p{ lex + 1 };
		ASTNode ast_us{ SyntaxToken::US, lex->line };
		SyntaxRet sr_module;
		if (p->token == LexToken::ID && p[1].token == LexToken::ASSIGN) {
			Lex* idLex = p;
			p += 2;
			sr_module = TestModule<LexToken::DOM, LexToken::SEMICOLON>(data, ast_us, p);
			ast_us.push(idLex);
		}
		else sr_module = TestModule<LexToken::DOT, LexToken::SEMICOLON>(data, ast_us, p);
		if (!sr_module.err) return ErrorChange<HYError::EXPECT_SEMICOLON>(sr_module);
		ast.push(freestanding::move(ast_us));
		return { HYError::NO_ERROR, sr_module.end + 1 };
	}

	// [native] 原生语句 Native-Statement
	// NS -> native Module;
	SyntaxRet ExpectNS(SyntaxerData& data, ASTNode& ast, Lex* lex) noexcept {
		ASTNode ast_ns{ SyntaxToken::NS, lex->line };
		SyntaxRet sr{ TestModule<LexToken::DOT, LexToken::SEMICOLON>(data, ast_ns, lex + 1) };
		if (!sr.err) return ErrorChange<HYError::EXPECT_SEMICOLON>(sr);
		ast.push(freestanding::move(ast_ns));
		return { HYError::NO_ERROR, sr.end + 1 };
	}

	// [const] 常量定义语句 Const-Value-Definition-Statement
	// CVDS -> const ID = C;
	SyntaxRet ExpectCVDS(SyntaxerData& data, ASTNode& ast, Lex* lex) noexcept {
		if (lex[1].token != LexToken::ID) return { HYError::EXPECT_ID, lex + 1 }; // 缺少名称
		if (lex[2].token != LexToken::ASSIGN) return { HYError::EXPECT_ASSIGN, lex + 2 }; // 缺少赋值号
		ASTNode ast_cvds{ SyntaxToken::CVDS, lex->line };
		SyntaxRet sr_c{ ExtractC<LexToken::SEMICOLON>(data, ast_cvds, lex + 3) };
		if (!sr_c.err) return ErrorChange<HYError::EXPECT_SEMICOLON>(sr_c); // 缺少分号
		ast_cvds.push(lex + 1);
		ast.push(freestanding::move(ast_cvds));
		return { HYError::NO_ERROR, sr_c.end + 1 }; // 归约成功
	}

	// [<] 结构化绑定语句 Auto-Bind Statement
	// ABS -> AutoBind = E;
	SyntaxRet ExpectABS(SyntaxerData& data, ASTNode& ast, Lex* lex) noexcept {
		ASTNode ast_vds{ SyntaxToken::ABS, lex->line };
		SyntaxRet sr_autobind{ ExpectAutoBind<LexToken::ASSIGN>(data, ast_vds, lex) };
		if (!sr_autobind.err) return ErrorChange<HYError::EXPECT_ASSIGN>(sr_autobind);
		auto p{ sr_autobind.end + 2 };
		SyntaxRet sr_e{ TestE<LexToken::SEMICOLON>(data, ast_vds, p) };
		if (!sr_e.err) return ErrorChange<HYError::EXPECT_SEMICOLON>(sr_e); // 缺少分号
		ast.push(freestanding::move(ast_vds));
		return { HYError::NO_ERROR, sr_e.end + 1 };
	}

	// [ID | this] 赋值语句 Assign-Statement
	// AS -> AL = E;
	// AL -> AL = E | E;
	// AS -> E += E; | E -= E; | E *= E; | E /= E; | E %= E;
	SyntaxRet ExpectAS(SyntaxerData& data, ASTNode& ast, Lex* lex) noexcept {
		using enum LexToken;
		ASTNode ast_as{ SyntaxToken::AS, lex->line };
		SyntaxRet sr_lv{ ExpectLV<ASSIGN, ADD_ASSIGN, SUBTRACT_ASSIGN,
			MULTIPLE_ASSIGN, DIVIDE_ASSIGN, MOD_ASSIGN>(data, ast_as, lex) };
		if (!sr_lv.err) return ErrorChange<HYError::EXPECT_ASSIGN>(sr_lv); // 归约LV失败
		auto p{ sr_lv.end + 1 }, pe{ p };
		if (p->token == ASSIGN) {
			for (auto pp{ p + 1 };;) {
				if (pp->token == ID || pp->token == THIS) {
					if (SyntaxRet sr_lv2{ ExpectLV<ASSIGN>(data, ast_as, pp) }; ~sr_lv2.err) {
						pp = sr_lv2.end + 2;
						continue;
					}
				}
				pe = pp;
				break;
			}
		}
		else pe = p + 1;
		SyntaxRet sr_e{ TestE<SEMICOLON>(data, ast_as, pe) };
		if (!sr_e.err) return ErrorChange<HYError::EXPECT_SEMICOLON>(sr_e);
		ast_as.push(p);
		ast.push(freestanding::move(ast_as));
		return { HYError::NO_ERROR, sr_e.end + 1 };
	}

	// [ID] 表达式语句 Expression-Statement
	// ES -> E;
	SyntaxRet ExpectES(SyntaxerData& data, ASTNode& ast, Lex* lex) noexcept {
		ASTNode ast_es{ SyntaxToken::ES, lex->line };
		SyntaxRet sr{ TestE<LexToken::SEMICOLON>(data, ast_es, lex) };
		if (!sr.err) return ErrorChange<HYError::EXPECT_SEMICOLON>(sr);
		ast.push(freestanding::move(ast_es));
		return { HYError::NO_ERROR, sr.end + 1 };
	}

	// [if] 条件语句 Condition-Statement
	// CS -> if (E) Block | if (E) Block else Block
	SyntaxRet ExpectCS(SyntaxerData& data, ASTNode& ast, Lex* lex) noexcept {
		auto p{ lex + 1 };
		if (p->token != LexToken::SMALL_LEFT) return { HYError::EXPECT_SMALL_LEFT, p }; // 缺少左小括号
		ASTNode ast_cs{ SyntaxToken::CS, lex->line };
		SyntaxRet sr_e{ TestE<LexToken::SMALL_RIGHT>(data, ast_cs, p + 1) };
		if (!sr_e.err) return ErrorChange<HYError::EXPECT_SMALL_RIGHT>(sr_e);
		p = sr_e.end + 2;
		if (p->token == LexToken::LARGE_LEFT) {
			SyntaxRet sr_b1{ ExpectBlock(data, ast_cs, p) };
			if (!sr_b1.err) return sr_b1;
			p = sr_b1.end + 1;
		}
		else {
			ASTNode ast_b1{ SyntaxToken::Block, p->line };
			SyntaxRet sr_nst{ TestNSt(data, ast_b1, p) };
			if (!sr_nst.err) return sr_nst;
			ast_cs.push(freestanding::move(ast_b1));
			p = sr_nst.end + 1;
		}
		if (p->token == LexToken::ELSE) {
			++p;
			if (p->token == LexToken::LARGE_LEFT) {
				SyntaxRet sr_b2{ ExpectBlock(data, ast_cs, p) };
				if (!sr_b2.err) return sr_b2;
				p = sr_b2.end;
			}
			else {
				ASTNode ast_b2{ SyntaxToken::Block, p->line };
				SyntaxRet sr_nst{ TestNSt(data, ast_b2, p) };
				if (!sr_nst.err) return sr_nst;
				ast_cs.push(freestanding::move(ast_b2));
				p = sr_nst.end;
			}
		}
		else --p;
		ast.push(freestanding::move(ast_cs));
		return { HYError::NO_ERROR, p };
	}

	// [while] 循环语句 Loop-Statement
	// LS -> while(E) Block
	SyntaxRet ExpectLS(SyntaxerData& data, ASTNode& ast, Lex* lex) noexcept {
		auto p{ lex + 1 };
		if (p->token != LexToken::SMALL_LEFT) return { HYError::EXPECT_SMALL_LEFT, p }; // 缺少左小括号
		ASTNode ast_ls{ SyntaxToken::LS, lex->line };
		SyntaxRet sr_e{ TestE<LexToken::SMALL_RIGHT>(data, ast_ls, p + 1) };
		if (!sr_e.err) return ErrorChange<HYError::EXPECT_SMALL_RIGHT>(sr_e);
		p = sr_e.end + 2;
		if (p->token == LexToken::LARGE_LEFT) {
			++data.LoopCount;
			SyntaxRet sr_b{ ExpectBlock(data, ast_ls, p) };
			--data.LoopCount;
			if (~sr_b.err) ast.push(freestanding::move(ast_ls));
			return sr_b;
		}
		else {
			ASTNode ast_b{ SyntaxToken::Block, p->line };
			++data.LoopCount;
			SyntaxRet sr_nst{ TestNSt(data, ast_b, p) };
			--data.LoopCount;
			if (~sr_nst.err) return sr_nst;
			ast_ls.push(freestanding::move(ast_b));
			ast.push(freestanding::move(ast_ls));
			return { HYError::NO_ERROR, sr_nst.end };
		}
	}

	// [continue] 继续循环语句 Loop-Continue-Statement
	// LCS -> continue;
	SyntaxRet ExpectLCS(SyntaxerData& data, ASTNode& ast, Lex* lex) noexcept {
		if (data.LoopCount == 0U) return { HYError::STATEMENT_LCS_ERROR, lex };
		if (lex[1].token != LexToken::SEMICOLON) return { HYError::EXPECT_SEMICOLON, lex };
		ast.push(SyntaxToken::LCS, lex->line);
		return { HYError::NO_ERROR, lex + 1 };
	}

	// [break] 跳出循环语句 Loop-Break-Statement
	// LBS -> break; | break INTEGER;
	SyntaxRet ExpectLBS(SyntaxerData& data, ASTNode& ast, Lex* lex) noexcept {
		if (data.LoopCount == 0U) return { HYError::STATEMENT_LBS_ERROR, lex };
		auto p{ lex + 1 };
		if (p->token == LexToken::SEMICOLON) {
			ast.push(SyntaxToken::LBS, lex->line);
			return { HYError::NO_ERROR, p };
		}
		else if (p->token == LexToken::INTEGER && p[1].token == LexToken::SEMICOLON) {
			ast.push(SyntaxToken::LBS, lex->line).push(p);
			return { HYError::NO_ERROR, p + 1 };
		}
		return { HYError::EXPECT_SEMICOLON, p - 1 };
	}

	// [for] 迭代循环语句 Iterate-Loop-Statement
	// ILS -> for ( AutoBind : E ) Block
	SyntaxRet ExpectILS(SyntaxerData& data, ASTNode& ast, Lex* lex) noexcept {
		auto p{ lex + 1 };
		if (p->token != LexToken::SMALL_LEFT) return { HYError::EXPECT_SMALL_LEFT, p };
		++p;
		ASTNode ast_ils{ SyntaxToken::ILS, lex->line };
		if (p->token == LexToken::ID || p->token == LexToken::ETC) {
			if (p[1].token == LexToken::COLON) {
				ASTNode ast_autobind{ SyntaxToken::AutoBind, p->line };
				ast_autobind.push(p);
				ast_ils.push(freestanding::move(ast_autobind));
				p += 2;
			}
			else return { HYError::EXPECT_COLON, p + 1 };
		}
		else if (p->token == LexToken::ACUTE_LEFT) {
			SyntaxRet sr_autobind{ ExpectAutoBind<LexToken::COLON>(data, ast_ils, p) };
			if (!sr_autobind.err) return ErrorChange<HYError::EXPECT_COLON>(sr_autobind); // 缺少冒号
			p = sr_autobind.end + 2;
		}
		else return { HYError::EXPECT_ACUTE_LEFT, p };
		SyntaxRet sr_e{ TestE<LexToken::SMALL_RIGHT>(data, ast_ils, p) };
		if (!sr_e.err) return ErrorChange<HYError::EXPECT_SMALL_RIGHT>(sr_e);
		p = sr_e.end + 2;
		if (p->token == LexToken::LARGE_LEFT) {
			SyntaxRet sr_block{ ExpectBlock(data, ast_ils, p) };
			if (!sr_block.err) return sr_block;
			ast_ils.back().syntaxToken() = SyntaxToken::Block;
			ast.push(freestanding::move(ast_ils));
			return sr_block;
		}
		else {
			ASTNode ast_b{ SyntaxToken::Block, p->line };
			SyntaxRet sr_nst{ TestNSt(data, ast_b, p) };
			if (!sr_nst.err) return sr_nst;
			ast_ils.push(freestanding::move(ast_b));
			ast.push(freestanding::move(ast_ils));
			return { HYError::NO_ERROR, sr_nst.end };
		}
	}

	// [switch] 分支语句 Switch-Statement
	// SS -> switch ( E ) { SL } | switch ( E ) { SL default -> Block }
	// SL -> SL INTEGER -> Block | INTEGER -> Block
	// SL -> SL LITERAL -> Block | LITERAL -> Block
	SyntaxRet ExpectSS(SyntaxerData& data, ASTNode& ast, Lex* lex) noexcept {
		auto p{ lex + 1 };
		if (p->token != LexToken::SMALL_LEFT) return { HYError::EXPECT_SMALL_LEFT, p };
		++p;
		ASTNode ast_ss{ SyntaxToken::SS, lex->line };
		SyntaxRet sr_e{ TestE<LexToken::SMALL_RIGHT>(data, ast_ss, p) };
		if (!sr_e.err) return ErrorChange<HYError::EXPECT_SMALL_RIGHT>(sr_e);
		p = sr_e.end + 2;
		if (p->token != LexToken::LARGE_LEFT) return { HYError::EXPECT_LARGE_LEFT, p };
		++p;
		ASTNode ast_sl(SyntaxToken::SL, p->line);
		auto hasDefault{ false };
		for (;;) {
			if (p->token == LexToken::DEFAULT) {
				if (hasDefault) return { HYError::EXISTS_DEFAULT, p };
				hasDefault = true;
				++p;
				if (p->token != LexToken::POINTER) return { HYError::EXPECT_POINTER, p };
				++p;
				if (p->token == LexToken::LARGE_LEFT) {
					SyntaxRet sr_b{ ExpectBlock(data, ast_ss, p) };
					if (!sr_b.err) return sr_b;
					p = sr_b.end + 1;
				}
				else {
					ASTNode ast_b{ SyntaxToken::Block, p->line };
					SyntaxRet sr_nst{ TestNSt(data, ast_b, p) };
					if (!sr_nst.err) return sr_nst;
					ast_ss.push(freestanding::move(ast_b));
					p = sr_nst.end + 1;
				}
				if (p->token == LexToken::LARGE_RIGHT) break;
				else continue;
			}
			SyntaxRet sr_e2{ TestE<LexToken::POINTER>(data, ast_sl, p) };
			if (!sr_e2.err) return ErrorChange<HYError::EXPECT_POINTER>(sr_e2);
			p = sr_e2.end + 2;
			if (p->token == LexToken::LARGE_LEFT) {
				SyntaxRet sr_b{ ExpectBlock(data, ast_sl, p) };
				if (!sr_b.err) return sr_b;
				p = sr_b.end + 1;
			}
			else {
				ASTNode ast_b{ SyntaxToken::Block, p->line };
				SyntaxRet sr_nst{ TestNSt(data, ast_b, p) };
				if (!sr_nst.err) return sr_nst;
				ast_sl.push(freestanding::move(ast_b));
				p = sr_nst.end + 1;
			}
			if (p->token == LexToken::LARGE_RIGHT) break;
		}
		ast_ss.push(freestanding::move(ast_sl));
		ast.push(freestanding::move(ast_ss));
		return { HYError::NO_ERROR, p };
	}

	// [function] 函数定义语句 Function-Definition-Statement
	// FDS -> function ID ( TArgs ) -> REF Block
	// FDS -> functino ID ( TArgs ) Block
	SyntaxRet ExpectFDS(SyntaxerData& data, ASTNode& ast, Lex* lex) noexcept {
		auto p{ lex + 1 };
		if (p->token != LexToken::ID) return { HYError::EXPECT_ID, p };
		if (p[1].token != LexToken::SMALL_LEFT) return { HYError::EXPECT_SMALL_LEFT, p + 1 };
		ASTNode ast_fds{ SyntaxToken::FDS, lex->line };
		ast_fds.push(p);
		p += 2;
		SyntaxRet sr_targs{ TestTArgs<LexToken::SMALL_RIGHT>(data, ast_fds, p) };
		if (!sr_targs.err) return ErrorChange<HYError::EXPECT_SMALL_RIGHT>(sr_targs);
		p = sr_targs.end + 2;
		if (p->token == LexToken::POINTER) {
			++p;
			if (p->token != LexToken::ID) return { HYError::EXPECT_ID, p };
			SyntaxRet sr_ref{ ExpectRef<LexToken::DOM, LexToken::LARGE_LEFT>(data, ast_fds, p) };
			if (!sr_ref.err) return ErrorChange<HYError::EXPECT_LARGE_LEFT>(sr_ref);
			p = sr_ref.end + 1;
		}
		if (p->token != LexToken::LARGE_LEFT) return { HYError::EXPECT_LARGE_LEFT, p };
		++data.FunctionCount;
		SyntaxRet sr_b{ ExpectBlock(data, ast_fds, p) };
		--data.FunctionCount;
		if (!sr_b.err) return sr_b;
		ast.push(freestanding::move(ast_fds));
		++data.TotalFunctionCount;
		return { HYError::NO_ERROR, sr_b.end };
	}

	// [return] 函数返回语句 Function-Return-Statement
	// FRS -> return; | return E;
	SyntaxRet ExpectFRS(SyntaxerData& data, ASTNode& ast, Lex* lex) noexcept {
		if (data.FunctionCount == 0U) return { HYError::STATEMENT_FRS_ERROR, lex };
		auto p{ lex + 1 };
		if (p->token == LexToken::SEMICOLON) {
			ast.push(SyntaxToken::FRS, lex->line);
			return { HYError::NO_ERROR, p };
		}
		ASTNode ast_frs{ SyntaxToken::FRS, lex->line };
		SyntaxRet sr_e{ TestE<LexToken::SEMICOLON>(data, ast_frs, p) };
		if (!sr_e.err) return ErrorChange<HYError::EXPECT_SEMICOLON>(sr_e);
		ast.push(freestanding::move(ast_frs));
		return { HYError::NO_ERROR, sr_e.end + 1 };
	}

	// [class] 类定义语句 Type-Definition-Statement
	// TDS -> class ID { TDSL }
	// TDSL -> TDSL REF MDS; | TDSL FDS | REF MDS; | FDS
	// MDS -> MDS, ID | ID
	SyntaxRet ExpectTDS(SyntaxerData& data, ASTNode& ast, Lex* lex) noexcept {
		auto p{ lex + 1 };
		if (p->token != LexToken::ID) return { HYError::EXPECT_ID, p };
		ASTNode ast_tds{ SyntaxToken::TDS, lex->line };
		ast_tds.push(p);
		++p;
		if (p->token != LexToken::LARGE_LEFT) return { HYError::EXPECT_LARGE_LEFT, p };
		ASTNode ast_tds_v{ SyntaxToken::TDS_V, lex->line };
		ASTNode ast_tds_f{ SyntaxToken::TDS_F, lex->line };
		++p;
		for (; p->token != LexToken::LARGE_RIGHT;) {
			if (p->token == LexToken::ID) { // 成员变量
				ASTNode ast_mds{ SyntaxToken::TDS_M, p->line };
				SyntaxRet sr_ref{ ExpectRef<LexToken::DOM, LexToken::ID>(data, ast_mds, p) };
				if (!sr_ref.err) return ErrorChange<HYError::EXPECT_ID>(sr_ref);
				p = sr_ref.end + 1;
				SyntaxRet sr_var{ ExpectRef<LexToken::COMMA, LexToken::SEMICOLON>(data, ast_mds, p) };
				if (!sr_var.err) return ErrorChange<HYError::EXPECT_SEMICOLON>(sr_var);
				ast_tds_v.push(freestanding::move(ast_mds));
				p = sr_var.end + 2;
			}
			else if (p->token == LexToken::FUNCTION) { // 成员函数
				++data.ClassCount;
				SyntaxRet sr_fds{ ExpectFDS(data, ast_tds_f, p) };
				--data.ClassCount;
				if (!sr_fds.err) return sr_fds;
				p = sr_fds.end + 1;
			}
			else return { HYError::STATEMENT_SS_ERROR, p };
		}
		ast_tds.push(freestanding::move(ast_tds_v));
		ast_tds.push(freestanding::move(ast_tds_f));
		ast.push(freestanding::move(ast_tds));
		return { HYError::NO_ERROR, p };
	}

	// [concept] 概念定义语句 Concept-Definition-Statement
	// CDS -> concept ID = Concept;
	SyntaxRet ExpectCDS(SyntaxerData& data, ASTNode& ast, Lex* lex) noexcept {
		auto p{ lex + 1 };
		if (p->token != LexToken::ID) return { HYError::EXPECT_ID, p };
		if (p[1].token != LexToken::ASSIGN) return { HYError::EXPECT_ASSIGN, p + 1 };
		ASTNode ast_cds{ SyntaxToken::CDS, lex->line };
		SyntaxRet sr_concept{ TestConcept<LexToken::SEMICOLON>(data, ast_cds, p + 2) };
		if (!sr_concept.err) return ErrorChange<HYError::EXPECT_SEMICOLON>(sr_concept);
		ast_cds.push(p);
		ast.push(freestanding::move(ast_cds));
		return { HYError::NO_ERROR, sr_concept.end + 1 };
	}

	// [global] 全局变量定义语句 Global-Variable-Definition-Statement
	// GVDS -> global GVDSL;
	// GVDSL -> GVDSL, ID | ID
	SyntaxRet ExpectGVDS(SyntaxerData& data, ASTNode& ast, Lex* lex) noexcept {
		auto p{ lex + 1 };
		if (p->token != LexToken::ID) return { HYError::EXPECT_ID, p };
		ASTNode ast_gvds{ SyntaxToken::GVDS, lex->line };
		SyntaxRet sr_var{ ExpectRef<LexToken::COMMA, LexToken::SEMICOLON>(data, ast_gvds, p) };
		if (!sr_var.err) return ErrorChange<HYError::EXPECT_SEMICOLON>(sr_var);
		ast.push(freestanding::move(ast_gvds));
		return { HYError::NO_ERROR, sr_var.end + 1 };
	}
}

namespace hy {
	// ID复合/This复合测试语句
	SyntaxRet ExpectStartWithIDOrThis(SyntaxerData& data, ASTNode& ast, Lex* lex) noexcept {
		SyntaxRet sr{ ExpectAS(data, ast, lex) };
		if (~sr.err || sr.err == HYError::EXPECT_SEMICOLON ||
			sr.err == HYError::STATEMENT_THIS_ERROR) return sr;
		return ExpectES(data, ast, lex);
	}

	// 禁止通行语句
	template<HYError error>
	inline constexpr SyntaxRet ExpectBroken(SyntaxerData&, ASTNode&, Lex* lex) noexcept {
		return { error, lex };
	}

	using ExpectFunc = SyntaxRet(*)(SyntaxerData&, ASTNode&, Lex*) noexcept;

	// 普通语句 Normal-Statement
	SyntaxRet TestNSt(SyntaxerData& data, ASTNode& ast, Lex* lex) noexcept {
		ExpectFunc func{ &ExpectBroken<HYError::STATEMENT_SS_ERROR> };
		switch (lex->token) {
		case LexToken::ID: func = &ExpectStartWithIDOrThis; break;
		case LexToken::THIS: func = &ExpectStartWithIDOrThis; break;
		case LexToken::ACUTE_LEFT: func = &ExpectABS; break;
		case LexToken::IF: func = &ExpectCS; break;
		case LexToken::WHILE: func = &ExpectLS; break;
		case LexToken::CONTINUE: func = &ExpectLCS; break;
		case LexToken::BREAK: func = &ExpectLBS; break;
		case LexToken::FOR: func = &ExpectILS; break;
		case LexToken::SWITCH: func = &ExpectSS; break;
		case LexToken::RETURN: func = &ExpectFRS; break;
		case LexToken::IMPORT: func = &ExpectBroken<HYError::STATEMENT_IS_ERROR>; break;
		case LexToken::USING: func = &ExpectBroken<HYError::STATEMENT_US_ERROR>; break;
		case LexToken::NATIVE: func = &ExpectBroken<HYError::STATEMENT_NS_ERROR>; break;
		case LexToken::CONST: func = &ExpectBroken<HYError::STATEMENT_CVDS_ERROR>; break;
		case LexToken::FUNCTION: func = &ExpectBroken<HYError::STATEMENT_FDS_ERROR>; break;
		case LexToken::CLASS: func = &ExpectBroken<HYError::STATEMENT_TDS_ERROR>; break;
		case LexToken::CONCEPT: func = &ExpectBroken<HYError::STATEMENT_CDS_ERROR>; break;
		case LexToken::GLOBAL: func = &ExpectBroken<HYError::STATEMENT_GVDS_ERROR>; break;
		}
		return func(data, ast, lex);
	}

	// 语句 Statement
	SyntaxRet TestSt(SyntaxerData& data, ASTNode& ast, Lex* lex) noexcept {
		switch (lex->token) {
		case LexToken::ID: return ExpectStartWithIDOrThis(data, ast[Syntaxer::AST_S], lex);
		case LexToken::THIS: return ExpectStartWithIDOrThis(data, ast[Syntaxer::AST_S], lex);
		case LexToken::ACUTE_LEFT: return ExpectABS(data, ast[Syntaxer::AST_S], lex);
		case LexToken::IF: return ExpectCS(data, ast[Syntaxer::AST_S], lex);
		case LexToken::WHILE: return ExpectLS(data, ast[Syntaxer::AST_S], lex);
		case LexToken::FOR: return ExpectILS(data, ast[Syntaxer::AST_S], lex);
		case LexToken::SWITCH: return ExpectSS(data, ast[Syntaxer::AST_S], lex);
		case LexToken::IMPORT: return ExpectIS(data, ast[Syntaxer::AST_IS], lex);
		case LexToken::USING: return ExpectUS(data, ast[Syntaxer::AST_US], lex);
		case LexToken::NATIVE: return ExpectNS(data, ast[Syntaxer::AST_NS], lex);
		case LexToken::CONST: return ExpectCVDS(data, ast[Syntaxer::AST_CVDS], lex);
		case LexToken::FUNCTION: return ExpectFDS(data, ast[Syntaxer::AST_FDS], lex);
		case LexToken::CLASS: return ExpectTDS(data, ast[Syntaxer::AST_TDS], lex);
		case LexToken::CONCEPT: return ExpectCDS(data, ast[Syntaxer::AST_CDS], lex);
		case LexToken::GLOBAL: return ExpectGVDS(data, ast[Syntaxer::AST_GVDS], lex);
		default: return { HYError::STATEMENT_SS_ERROR, lex };
		}
	}

	// [{] 语句块 Block
	// Block -> { SList }
	// SList -> SList NSt | NSt | epsilon
	SyntaxRet ExpectBlock(SyntaxerData& data, ASTNode& ast, Lex* lex) noexcept {
		auto p{ lex + 1 };
		ASTNode ast_block{ SyntaxToken::Block, lex->line };
		for (; p->token != LexToken::LARGE_RIGHT;) {
			if (p->token == LexToken::END_LEX) return { HYError::EXPECT_LARGE_RIGHT, p - 1 };
			SyntaxRet sr_nst{ TestNSt(data, ast_block, p) };
			if (!sr_nst.err) return ErrorChange<HYError::EXPECT_LARGE_RIGHT>(sr_nst);
			p = sr_nst.end + 1;
		}
		ast.push(freestanding::move(ast_block));
		return { HYError::NO_ERROR, p };
	}
}

namespace hy {
	inline void Clean(Syntaxer* syntaxer) noexcept {
		syntaxer->root.reset(SyntaxToken::Program, 0U);
	}

	inline HYError CheckGlobalTable(SyntaxerData& data) noexcept {
		if (data.LoopCount != 0U) return HYError::GLOBAL_TABLE_LOOPCOUNT;
		if (data.FunctionCount != 0U) return HYError::GLOBAL_TABLE_FUNCTIONCOUNT;
		if (data.ClassCount != 0U) return HYError::GLOBAL_TABLE_CLASSCOUNT;
		return HYError::NO_ERROR;
	}

	LIB_EXPORT CodeResult SyntaxerAnalyse(Syntaxer * syntaxer, Lexer * lexer) noexcept {
		Clean(syntaxer);
		syntaxer->source = lexer->source;
		auto& root{ syntaxer->root };
		root.reserve(Syntaxer::AST_COUNT);
		for (Index i{ }; i < Syntaxer::AST_COUNT; ++i)
			root.push(SyntaxToken::Statements, 0U);
		SyntaxerData data;
		SyntaxRet sr;
		for (auto p{ lexer->lexes.data() }; p->token != LexToken::END_LEX; p = sr.end + 1ULL) {
			sr = TestSt(data, root, p);
			if (!sr.err) return { sr.err, sr.end->line, sr.end->start, sr.end->end };
		}
		return CodeResult{ CheckGlobalTable(data), 0U, nullptr, nullptr };
	}
}