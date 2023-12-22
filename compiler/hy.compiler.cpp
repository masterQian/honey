/**************************************************
*
* @文件				hy.compiler
* @作者				钱浩宇
* @创建时间			2021-11-05
* @更新时间			2022-11-30
* @摘要
* 汉念编译器的实现
*
**************************************************/

#include "hy.compiler.h"
#include "../syntaxer/hy.syntaxer.h"
#include "../serializer/hy.serializer.writer.h"

namespace hy {
	// 编译ID
	inline StringView CompileID(ASTNode& ast) noexcept {
		return StringView{ ast.lexStart(), ast.lexEnd() - ast.lexStart() + 1ULL };
	}

	// 编译整数
	inline Int64 CompileInteger(ASTNode& ast) noexcept {
		Str end;
		Int64 value{ freestanding::cvt::safe_stol(ast.lexStart(), &end) };
		return end == ast.lexEnd() + 1 ? value : 0LL;
	}

	// 编译浮点数
	inline Float64 CompileDecimal(ASTNode& ast) noexcept {
		Str end;
		Float64 value{ freestanding::cvt::stod(ast.lexStart(), &end) };
		return end == ast.lexEnd() + 1 ? value : 0.0;
	}

	// 编译复数
	inline __Complex CompileComplex(ASTNode& ast) noexcept {
		Str end;
		Float64 value{ freestanding::cvt::stod(ast.lexStart(), &end) };
		return __Complex{ 0.0, end == ast.lexEnd() ? value : 0.0 };
	}

	// 编译逻辑值
	inline bool CompileBoolean(ASTNode& ast) noexcept {
		return ast.lexToken() == LexToken::TRUE;
	}

	// 编译字符串
	inline String CompileString(ASTNode& ast) noexcept {
		auto start{ ast.lexStart() }, end{ ast.lexEnd() };
		if (end == start + 1) return { };
		String str(end - start, 0U);
		auto pStart{ str.data() }, pEnd{ pStart };
		for (++start; start != end; ++start) {
			switch (*start) {
			[[unlikely]] case u'\\': {
				++start;
				switch (*start) {
				case u'n': *pEnd++ = u'\n'; break;
				case u't': *pEnd++ = u'\t'; break;
				case u'v': *pEnd++ = u'\v'; break;
				case u'\\': *pEnd++ = u'\\'; break;
				case u'\"': *pEnd++ = u'\"'; break;
				case u'u': {
					Str utfEnd;
					Char c{ static_cast<Char>(freestanding::cvt::stox(start + 1, &utfEnd)) };
					if (utfEnd == start + 5) {
						*pEnd++ = c;
						start += 4;
					}
					break;
				}
				default: *pEnd++ = u'\\'; --start; break;
				}
				break;
			}
			[[unlikely]] case u'\r':
			[[unlikely]] case u'\n': break;
			[[likely]] default: *pEnd++ = *start; break;
			}
		}
		str.resize(pEnd - pStart);
		return str;
	}

	// 编译引用
	inline __Ref CompileRef(ASTNode& ast) noexcept {
		__Ref ref;
		for (auto& id : ast) {
			ref.data += CompileID(id);
			ref.data.push_back(u'\0');
			++ref.len;
		}
		return ref;
	}

	// 编译结构化绑定
	inline __Ref CompileAutoBind(ASTNode& ast) noexcept {
		__Ref ab;
		for (auto& id : ast) {
			if (id.lexToken() == LexToken::ID) ab.data += CompileID(id);
			ab.data.push_back(u'\0');
			++ab.len;
		}
		return ab;
	}

	// 编译参数列表
	inline bool CompileTArgs(CompileTable& table, InsEnv&, ASTNode& ast, __Indexs& indexs) noexcept {
		auto size{ ast.size() };
		if (!size) return false;
		auto va_targs{ ast[size - 1].isLeaf() };
		if (va_targs) --size;
		indexs.resize(size << 1ULL);
		for (Index i{ }, j{ }; i < size; ++i) {
			ASTNode& targNode{ ast[i] };
			indexs[j++] = table.pool.get(CompileID(targNode[0ULL]));
			indexs[j++] = targNode.size() == 2ULL ? table.pool.get(CompileRef(targNode[1ULL])) : INone;
		}
		return va_targs;
	}

	// 编译概念
	inline void CompileConcept(CompileTable& table, ASTNode& ast, Vector<SubConceptElement>& sc) noexcept {
		switch (auto token{ ast.syntaxToken() }) {
		case SyntaxToken::REF: {
			sc.emplace_back(table.pool.get(CompileRef(ast)));
			break;
		}
		case SyntaxToken::NOT: {
			CompileConcept(table, ast[0ULL], sc); // 编译左子树
			sc.emplace_back(SubConceptElementType::NOT, 0U);
			break;
		}
		case SyntaxToken::AND:
		case SyntaxToken::OR: {
			CompileConcept(table, ast[0ULL], sc); // 编译左子树
			auto jumpPos{ sc.size() };
			sc.emplace_back(token == SyntaxToken::AND ? SubConceptElementType::JUMP_FALSE
				: SubConceptElementType::JUMP_TRUE, 0U);
			CompileConcept(table, ast[1ULL], sc); // 编译右子树
			sc[jumpPos].arg = static_cast<Index32>(sc.size() - jumpPos);
			break;
		}
		}
	}

	// 双操作数映射
	inline constexpr BOPTType OPBinaryMap(SyntaxToken token) noexcept {
		switch (token) {
		case SyntaxToken::ADD: return BOPTType::ADD;
		case SyntaxToken::SUBTRACT: return BOPTType::SUBTRACT;
		case SyntaxToken::MULTIPLE: return BOPTType::MULTIPLE;
		case SyntaxToken::DIVIDE: return BOPTType::DIVIDE;
		case SyntaxToken::MOD: return BOPTType::MOD;
		case SyntaxToken::POWER: return BOPTType::POWER;
		case SyntaxToken::GT: return BOPTType::GT;
		case SyntaxToken::GE: return BOPTType::GE;
		case SyntaxToken::LT: return BOPTType::LT;
		case SyntaxToken::LE: return BOPTType::LE;
		case SyntaxToken::EQ: return BOPTType::EQ;
		case SyntaxToken::NE: return BOPTType::NE;
		default: return BOPTType::NONE;
		}
	}

	// 单操作数映射
	inline constexpr SOPTType OPSingleMap(SyntaxToken token) noexcept {
		switch (token) {
		case SyntaxToken::NEGATIVE: return SOPTType::NEGATIVE;
		case SyntaxToken::NOT: return SOPTType::NOT;
		default: return SOPTType::NONE;
		}
	}

	// 赋值操作数映射
	inline constexpr AssignType OPAssignMap(LexToken token) noexcept {
		switch (token) {
		case LexToken::ADD_ASSIGN: return AssignType::ADD_ASSIGN;
		case LexToken::SUBTRACT_ASSIGN: return AssignType::SUBTRACT_ASSIGN;
		case LexToken::MULTIPLE_ASSIGN: return  AssignType::MULTIPLE_ASSIGN;
		case LexToken::DIVIDE_ASSIGN: return AssignType::DIVIDE_ASSIGN;
		case LexToken::MOD_ASSIGN: return AssignType::MOD_ASSIGN;
		default: return AssignType::NONE;
		}
	}

	// 取lambda哈希化名称
	// insBlock 函数体
	// argc 函数参数个数
	// ret 是否有返回值
	String GetLambdaName(InsBlock& insBlock, Size argc, bool ret) noexcept {
		constexpr const Size32 prime1[] {
			2, 11, 23, 41, 59, 73, 97, 109,
			137, 157, 179, 197, 227, 241, 269, 283,
			313, 347, 367, 389, 419, 439, 461, 487,
			509, 547, 571, 599, 617, 643, 661, 691,
			727, 751, 773, 811, 829, 859, 883, 919,
			947, 977, 1009, 1031, 1051, 1087, 1103, 1129,
			1171, 1201, 1229, 1259, 1289, 1303, 1327, 1381,
			1427, 1447, 1471, 1489, 1523, 1553, 1579, 1607,
			1621, 1663, 1697, 1723, 1753, 1787, 1823, 1867,
			1879, 1913, 1951, 1993, 2011, 2039, 2081, 2099,
			2131, 2153, 2207, 2239, 2269, 2293, 2333, 2351,
			2381, 2399, 2437, 2467, 2521, 2549, 2591, 2621,
			2659, 2683, 2699, 2719, 2749, 2789, 2803, 2843,
			2879, 2909, 2953, 2971, 3019, 3049, 3083, 3121,
			3169, 3203, 3229, 3259, 3307, 3329, 3359, 3389,
			3433, 3463, 3499, 3529, 3547, 3581, 3613, 3637,
			3673, 3701, 3733, 3769, 3803, 3847, 3877, 3911,
			3929, 3967, 4007, 4027, 4073, 4099, 4133, 4159,
			4217, 4241, 4261, 4289, 4339, 4373, 4421, 4451,
			4483, 4517, 4549, 4591, 4637, 4651, 4679, 4723,
			4759, 4793, 4817, 4877, 4919, 4943, 4969, 4999,
			5021, 5059, 5099, 5119, 5171, 5209, 5237, 5281,
			5323, 5381, 5407, 5431, 5449, 5483, 5519, 5557,
			5581, 5641, 5657, 5689, 5717, 5749, 5801, 5827,
			5851, 5869, 5903, 5953, 6011, 6047, 6079, 6113,
			6143, 6197, 6217, 6257, 6277, 6311, 6337, 6361,
			6389, 6449, 6481, 6547, 6569, 6599, 6653, 6679,
			6703, 6737, 6781, 6823, 6841, 6871, 6911, 6959,
			6977, 7001, 7039, 7079, 7127, 7177, 7211, 7237,
			7283, 7321, 7351, 7417, 7459, 7489, 7523, 7547,
			7573, 7591, 7639, 7673, 7699, 7727, 7759, 7823,
			7867, 7883, 7927, 7951, 8011, 8059, 8089, 8117,
		};

		constexpr const Size32 prime2[] {
			8231, 8263, 8291, 8317, 8369, 8419, 8443, 8501,
			8537, 8573, 8609, 8641, 8677, 8699, 8731, 8753,
			8803, 8831, 8861, 8893, 8941, 8971, 9011, 9043,
			9091, 9133, 9161, 9199, 9227, 9277, 9311, 9341,
			9377, 9413, 9433, 9463, 9491, 9533, 9587, 9623,
			9649, 9689, 9733, 9767, 9791, 9829, 9857, 9887,
			9929, 9967, 10037, 10069, 10099, 10139, 10163, 10193,
			10247, 10271, 10303, 10333, 10369, 10429, 10459, 10499,
			10531, 10597, 10627, 10657, 10691, 10729, 10771, 10831,
			10859, 10889, 10937, 10973, 11003, 11059, 11087, 11119,
			11161, 11197, 11251, 11279, 11317, 11353, 11399, 11443,
			11483, 11503, 11551, 11597, 11657, 11699, 11731, 11783,
			11813, 11833, 11887, 11923, 11941, 11971, 12011, 12049,
			12101, 12119, 12161, 12211, 12251, 12277, 12323, 12373,
			12401, 12433, 12473, 12497, 12527, 12553, 12589, 12619,
			12653, 12697, 12739, 12781, 12821, 12853, 12907, 12923,
			12967, 13001, 13033, 13063, 13109, 13151, 13177, 13219,
			13259, 13309, 13337, 13397, 13421, 13463, 13499, 13553,
			13597, 13633, 13681, 13697, 13723, 13759, 13799, 13841,
			13879, 13907, 13933, 13999, 14033, 14081, 14143, 14173,
			14221, 14281, 14323, 14369, 14407, 14431, 14461, 14519,
			14549, 14563, 14627, 14653, 14699, 14731, 14753, 14779,
			14821, 14851, 14887, 14929, 14957, 15017, 15073, 15101,
			15137, 15173, 15217, 15259, 15277, 15307, 15331, 15373,
			15401, 15443, 15473, 15527, 15569, 15607, 15643, 15667,
			15727, 15739, 15773, 15803, 15859, 15889, 15919, 15971,
			16007, 16063, 16087, 16111, 16183, 16217, 16249, 16301,
			16349, 16381, 16427, 16453, 16493, 16553, 16603, 16633,
			16661, 16699, 16747, 16811, 16843, 16889, 16927, 16963,
			16993, 17029, 17053, 17107, 17159, 17191, 17231, 17293,
			17327, 17359, 17389, 17419, 17467, 17489, 17519, 17573,
			17599, 17657, 17683, 17737, 17783, 17827, 17863, 17909,
		};

		Size32 hash1{ }, hash2{ }, v{ };
		Byte cnt{ }, index{ };
		for (auto& i : insBlock.iset) {
			index = static_cast<Byte>(i.type);
			v = i.get<Size32>();
			hash1 ^= (prime1[index] ^ v) << cnt;
			hash2 ^= (prime2[index] ^ v) << cnt;
			cnt = (cnt + 8U) % 64U;
		}
		auto hash{ (static_cast<Size>(hash1)) << 32U | static_cast<Size>(hash2) };
		if (ret) hash = ~hash;
		Char str[32]{ }, * p{ str };
		*p++ = u'_'; *p++ = u'_'; *p++ = u'l';
		do {
			*p++ = static_cast<Char>(hash % 10ULL + u'0');
			hash = hash / 10ULL;
		} while (hash);
		*p++ = u'_';
		do {
			*p++ = static_cast<Char>(argc % 10ULL + u'0');
			argc = argc / 10ULL;
		} while (argc);
		*p++ = ret ? u'1' : u'0';
		*p = u'\0';
		return str;
	}
}

namespace hy {
	using CompileFunc = void(*)(InsBlock&, CompileTable&, InsEnv&, ASTNode&) noexcept;

	inline constexpr void CompileNothing(InsBlock&, CompileTable&, InsEnv&, ASTNode&) noexcept {}

	void CompileE(InsBlock& ins, CompileTable& table, InsEnv& env, ASTNode& ast) noexcept;
	void CompileBlock(InsBlock& ins, CompileTable& table, InsEnv& env, ASTNode& ast) noexcept;

	// 编译常值
	inline void CompileC(InsBlock& ins, CompileTable& table, InsEnv&, ASTNode& ast) noexcept {
		switch (ast.lexToken()) {
		case LexToken::INTEGER: ins.push(InsType::PUSH_LITERAL, table.pool.get(CompileInteger(ast))); break;
		case LexToken::DECIMAL: ins.push(InsType::PUSH_LITERAL, table.pool.get(CompileDecimal(ast))); break;
		case LexToken::COMPLEX: ins.push(InsType::PUSH_LITERAL, table.pool.get(CompileComplex(ast))); break;
		case LexToken::LITERAL: ins.push(InsType::PUSH_LITERAL, table.pool.get(CompileString(ast))); break;
		case LexToken::TRUE:
		case LexToken::FALSE: ins.push(InsType::PUSH_BOOLEAN, CompileBoolean(ast)); break;
		case LexToken::THIS: ins.push<InsType::PUSH_THIS>(); break;
		default: ins.push<InsType::PUSH_NULL>(); break;
		}
	}

	// 编译常量
	inline void CompileCV(InsBlock& ins, CompileTable& table, InsEnv&, ASTNode& ast) noexcept {
		ins.push(InsType::PUSH_CONST, table.pool.get(CompileRef(ast)));
	}

	// 编译包引用
	inline void CompileDomREF(InsBlock& ins, CompileTable& table, InsEnv&, ASTNode& ast) noexcept {
		auto names{ CompileRef(ast) };
		if (names.len == 1ULL) {
			names.data.pop_back();
			ins.push(InsType::PUSH_SYMBOL, table.pool.get(names.data));
		}
		else ins.push(InsType::PUSH_REF, table.pool.get(freestanding::move(names)));
	}

	// 编译列表
	inline void CompileList(InsBlock& ins, CompileTable& table, InsEnv& env, ASTNode& ast) noexcept {
		for (auto& elem : ast) CompileE(ins, table, env, elem);
		ins.push(InsType::PUSH_LIST, ast.size());
	}

	// 编译字典
	inline void CompileDict(InsBlock& ins, CompileTable& table, InsEnv& env, ASTNode& ast) noexcept {
		for (auto& elem : ast) CompileE(ins, table, env, elem);
		ins.push(InsType::PUSH_DICT, ast.size() >> 1ULL);
	}

	// 编译向量
	inline void CompileVector(InsBlock& ins, CompileTable& table, InsEnv&, ASTNode& ast) noexcept {
		auto size{ ast.size() };
		auto index{ INone };
		if (size) {
			__Vector vec{ size };
			auto pData{ vec.data() };
			for (auto& elem : ast) *pData++ = CompileDecimal(elem);
			index = table.pool.get(freestanding::move(vec));
		}
		ins.push(InsType::PUSH_VECTOR, index);
	}

	// 编译矩阵
	inline void CompileMatrix(InsBlock& ins, CompileTable& table, InsEnv&, ASTNode& ast) noexcept {
		Size32 row{ static_cast<Size32>(ast.size()) }, col{ }, tmp{ };
		for (auto& elem : ast) {
			tmp = static_cast<Size32>(elem.size());
			if (tmp > col) col = tmp; // 求出最长的列
		}
		auto index{ INone };
		if (col) {
			__Matrix mat{ row, col };
			auto pData{ mat.data() };
			freestanding::initialize_n(pData, 0, static_cast<Size>(row * col));
			Index i{ }, j{ };
			for (auto& elemRow : ast) {
				for (auto& elemCol : elemRow) {
					pData[i * col + j] = CompileDecimal(elemCol);
					++j;
				}
				++i;
				j = 0ULL;
			}
			index = table.pool.get(freestanding::move(mat));
		}
		ins.push(InsType::PUSH_MATRIX, index);
	}

	// 编译范围
	inline void CompileRange(InsBlock& ins, CompileTable& table, InsEnv&, ASTNode& ast) noexcept {
		Int64 start{ CompileInteger(ast[0ULL]) }, step{ CompileInteger(ast[1ULL]) }, end{ };
		if (ast.size() == 3ULL) end = CompileInteger(ast[2ULL]);
		else {
			end = step;
			step = 1LL;
		}
		ins.push(InsType::PUSH_RANGE, table.pool.get(__Range{ start, step, end }));
	}

	// 编译与/或运算符(逻辑短路)
	template<InsType type>
	inline void CompileAndOr(InsBlock& ins, CompileTable& table, InsEnv& env, ASTNode& ast) noexcept {
		CompileE(ins, table, env, ast[0ULL]); // 先编译左子树
		auto jumpPos{ ins.jump_start<type>() }; // 逻辑短路
		ins.push<InsType::POP>(); // 左子树不短路把左子树弹出
		CompileE(ins, table, env, ast[1ULL]); // 再编译右子树
		ins.push<InsType::AS_BOOL>(); // 右子树转换为逻辑值
		ins.jump_end(jumpPos);
	}

	// 编译成员运算符
	inline void CompileMember(InsBlock& ins, CompileTable& table, InsEnv& env, ASTNode& ast) noexcept {
		CompileE(ins, table, env, ast[0ULL]);
		ins.push(InsType::MEMBER, table.pool.get(CompileID(ast[1ULL])));
	}

	// 编译形参列表
	inline Size32 CompileArgs(InsBlock& ins, CompileTable& table, InsEnv& env, ASTNode& ast) noexcept {
		auto size{ ast.size() };
		for (auto& elem : ast) CompileE(ins, table, env, elem);
		return static_cast<Size32>(size);
	}

	// 编译索引运算符
	inline void CompileIndex(InsBlock& ins, CompileTable& table, InsEnv& env, ASTNode& ast) noexcept {
		auto argc{ CompileArgs(ins, table, env, ast[1ULL]) };
		CompileE(ins, table, env, ast[0ULL]);
		ins.push(InsType::INDEX, argc);
	}

	// 编译函数调用
	inline void CompileCall(InsBlock& ins, CompileTable& table, InsEnv& env, ASTNode& ast) noexcept {
		CompileE(ins, table, env, ast[0ULL]); // 函数指针
		auto size{ CompileArgs(ins, table, env, ast[1ULL]) }; // 参数
		ins.check_call();
		ins.push(InsType::CALL, size);
	}

	// 编译Lambda表达式
	inline void CompileLambda(InsBlock& ins, CompileTable& table, InsEnv& env, ASTNode& ast) noexcept {
		__Function func;

		// 函数参数
		auto& ast_targs{ ast[0ULL] };
		__Indexs indexs;
		func.va_targs = static_cast<Index32>(CompileTArgs(table, env, ast_targs, indexs));
		func.index_targs = table.pool.get(freestanding::move(indexs));

		// 函数返回值
		auto& node{ ast[1ULL] };
		auto has_ret{ node.syntaxToken() == SyntaxToken::REF };
		func.index_ret = has_ret ? table.pool.get(CompileRef(node)) : INone;

		// 函数体
		InsEnv envTmp;
		auto& ast_b{ has_ret ? ast[2ULL] : node };
		CompileBlock(func.insFunc, table, envTmp, ast_b);
		func.insFunc.push<InsType::PUSH_NULL>();
		func.insFunc.push<InsType::RETURN>();

		// 生成lambda名称
		auto index_name{ table.pool.get(GetLambdaName(func.insFunc, ast_targs.size(), has_ret)) };
		func.index_name = index_name;

		auto index_func{ table.pool.get(freestanding::move(func)) };
		ins.push(InsType::PRE_LAMBDA, index_func);
		ins.push(InsType::PUSH_SYMBOL, index_name);
	}

	// 编译表达式
	void CompileE(InsBlock& ins, CompileTable& table, InsEnv& env, ASTNode& ast) noexcept {
		if (ast.isRoot()) { // 根结点
			auto& token{ ast.syntaxToken() };
			if (auto opt1{ OPBinaryMap(token) }; opt1 != BOPTType::NONE) {
				CompileE(ins, table, env, ast[0ULL]); // 先编译左子树
				CompileE(ins, table, env, ast[1ULL]); // 再编译右子树
				ins.push(InsType::OP_BINARY, static_cast<Byte>(opt1));
			}
			else if (auto opt2{ OPSingleMap(token) }; opt2 != SOPTType::NONE) {
				CompileE(ins, table, env, ast[0ULL]); // 编译表达式
				ins.push(InsType::OP_SINGLE, static_cast<Byte>(opt2));
			}
			else {
				CompileFunc func{ &CompileNothing };
				switch (token) {
				case SyntaxToken::CV: func = &CompileCV; break;
				case SyntaxToken::REF: func = &CompileDomREF; break;
				case SyntaxToken::List: func = &CompileList; break;
				case SyntaxToken::Dict: func = &CompileDict; break;
				case SyntaxToken::Vector: func = &CompileVector; break;
				case SyntaxToken::Matrix: func = &CompileMatrix; break;
				case SyntaxToken::Range: func = &CompileRange; break;
				case SyntaxToken::AND: func = &CompileAndOr<InsType::JUMP_FALSE>; break;
				case SyntaxToken::OR: func = &CompileAndOr<InsType::JUMP_TRUE>; break;
				case SyntaxToken::MEMBER: func = &CompileMember; break;
				case SyntaxToken::INDEX: func = &CompileIndex; break;
				case SyntaxToken::CALL: func = &CompileCall; break;
				case SyntaxToken::Lambda: func = &CompileLambda; break;
				}
				func(ins, table, env, ast);
			}
		}
		else CompileC(ins, table, env, ast); // 编译常值
	}
}

namespace hy {
	// 编译结构化绑定语句
	void CompileABS(InsBlock& ins, CompileTable& table, InsEnv& env, ASTNode& ast) noexcept {
		auto checkpoint{ ins.check_start(ast.line()) };
		auto names{ CompileAutoBind(ast[0ULL]) };
		auto len{ static_cast<Size32>(names.len) };
		auto index_names{ table.pool.get(freestanding::move(names)) };

		CompileE(ins, table, env, ast[1ULL]);

		ins.push(InsType::UNPACK, len);
		ins.push(InsType::SAVE_PACK, index_names);
		ins.check_end(checkpoint);
	}

	// 编译赋值语句
	void CompileAS(InsBlock& ins, CompileTable& table, InsEnv& env, ASTNode& ast) noexcept {
		auto checkpoint{ ins.check_start(ast.line()) };
		auto type{ ast.back().lexToken() };
		if (type == LexToken::ASSIGN) { // 连续赋值
			auto count{ ast.size() - 2ULL };
			CompileE(ins, table, env, ast[count]); // 先编译值
			for (Index i{ }; i < count; ++i) { // 再逆向编译count - 1个左值
				CompileE(ins, table, env, ast[count - i - 1]);
				ins.set_back_lv();
			}
			ins.push(InsType::ASSIGN, count);
		}
		else { // 运算符赋值
			CompileE(ins, table, env, ast[1ULL]);
			CompileE(ins, table, env, ast[0ULL]);
			ins.set_back_lv();
			ins.push(InsType::ASSIGN_EX, static_cast<Byte>(OPAssignMap(type)));
		}
		ins.check_end(checkpoint);
	}

	// 编译表达式语句
	void CompileES(InsBlock& ins, CompileTable& table, InsEnv& env, ASTNode& ast) noexcept {
		auto checkpoint{ ins.check_start(ast.line()) };
		CompileE(ins, table, env, ast[0ULL]);
		ins.push<InsType::POP>();
		ins.check_end(checkpoint);
	}

	// 编译条件语句
	void CompileCS(InsBlock& ins, CompileTable& table, InsEnv& env, ASTNode& ast) noexcept {
		auto checkpoint{ ins.check_start(ast.line()) };
		CompileE(ins, table, env, ast[0ULL]);
		ins.check_end(checkpoint);
		auto jp1{ ins.jump_start<InsType::JUMP_FALSE_POP>() };
		CompileBlock(ins, table, env, ast[1ULL]);
		if (ast.size() == 3ULL) {
			auto jp2{ ins.jump_start<InsType::JUMP>() };
			ins.jump_end(jp1);
			CompileBlock(ins, table, env, ast[2ULL]);
			ins.jump_end(jp2);
		}
		else ins.jump_end(jp1);
	}

	// 编译循环语句
	void CompileLS(InsBlock& ins, CompileTable& table, InsEnv& env, ASTNode& ast) noexcept {
		auto checkpoint{ ins.check_start(ast.line()) };

		// 开启循环域
		env.loopEnv.push();

		// 循环条件
		auto jrp{ ins.jump_re_end() };
		CompileE(ins, table, env, ast[0ULL]);
		ins.check_end(checkpoint);

		// 循环体
		auto jp{ ins.jump_start<InsType::JUMP_FALSE_POP>() };
		CompileBlock(ins, table, env, ast[1ULL]);

		// 处理Continue跳转表, continue都将引导到CLEAR_DOM
		auto& ct{ env.loopEnv.topCT() };
		auto continue_end{ ins.size() };
		for (auto bct{ ct.data() }, ect{ bct + ct.size() }; bct != ect; ++bct)
			ins.iset[*bct].set(continue_end - *bct);

		// 运行期清理循环域
		ins.jump_re_start<InsType::JUMP_RE>(jrp);
		ins.jump_end(jp);

		// 处理Break跳转表, break都将引导到STOP_DOM
		auto& bt{ env.loopEnv.topBT() };
		auto break_end{ ins.size() };
		for (auto bct{ bt.data() }, ect{ bct + bt.size() }; bct != ect; ++bct)
			ins.iset[*bct].set(break_end - *bct);

		env.loopEnv.pop(); // 编译期循环域终止
	}

	// 编译继续循环语句
	void CompileLCS(InsBlock& ins, CompileTable&, InsEnv& env, ASTNode& ast) noexcept {
		auto checkpoint{ ins.check_start(ast.line()) };
		env.loopEnv.topCT().emplace_back(ins.size());
		ins.push<InsType::JUMP>();
		env.isTerminal = true; // continue终止后续语句编译
		ins.check_end(checkpoint);
	}

	// 编译跳出循环语句
	void CompileLBS(InsBlock& ins, CompileTable&, InsEnv& env, ASTNode& ast) noexcept {
		auto checkpoint{ ins.check_start(ast.line()) };
		auto scnt{ 1LL };
		if (ast.size() == 1ULL) scnt = CompileInteger(ast[0ULL]);
		auto cnt{ static_cast<Uint64>(scnt <= 0LL ? -1LL : scnt) };
		auto& bt{ env.loopEnv.indexBT(cnt) };
		bt.emplace_back(ins.size());
		ins.push<InsType::JUMP>();
		env.isTerminal = true; // break终止后续语句编译
		ins.check_end(checkpoint);
	}

	// 编译迭代语句
	void CompileILS(InsBlock& ins, CompileTable& table, InsEnv& env, ASTNode& ast) noexcept {
		auto checkpoint{ ins.check_start(ast.line()) };

		// 将迭代元名称导入扫描器
		auto index_names{ table.pool.get(CompileAutoBind(ast[0ULL])) };

		// 编译迭代容器
		CompileE(ins, table, env, ast[1ULL]);

		// 取出迭代容器的迭代元
		ins.push<InsType::GET_ITER>();

		// 检查迭代元并保存迭代元解包元素
		auto jrp{ ins.jump_re_end() };
		auto jp{ ins.jump_start<InsType::JUMP_CHECK_ITER>() };
		ins.push(InsType::SAVE_ITER, index_names);

		// 编译迭代体
		CompileBlock(ins, table, env, ast[2ULL]);

		// 结束当前循环并递增迭代元
		ins.push<InsType::ADD_ITER>();
		ins.jump_re_start<InsType::JUMP_RE>(jrp);

		// 结束迭代
		ins.jump_end(jp);

		ins.check_end(checkpoint);
	}

	// 编译分支语句
	void CompileSS(InsBlock& ins, CompileTable& table, InsEnv& env, ASTNode& ast) noexcept {
		auto checkpoint{ ins.check_start(ast.line()) };
		auto hasDefault{ ast.size() == 3ULL };
		auto& sl{ ast[hasDefault ? 2ULL : 1ULL] };

		// 编译分支表达式
		CompileE(ins, table, env, ast[0ULL]);

		// 编译分支语句
		Index i{ }, j{ }, l = sl.size(), pos;
		util::Array<Index> jumpArr{ l >> 1ULL };
		for (; j < l; ) {
			CompileE(ins, table, env, sl[j++]);
			pos = ins.size();
			ins.push<InsType::JUMP_NE_POP>();
			CompileBlock(ins, table, env, sl[j++]);
			jumpArr[i++] = ins.size();
			ins.push<InsType::JUMP>();
			ins.iset[pos].set(ins.size() - pos);
		}

		// 编译缺省语句
		if (hasDefault) CompileBlock(ins, table, env, ast[1ULL]);

		// 结束分支
		auto k{ ins.size() };
		for (auto jp : jumpArr) ins.iset[jp].set(k - jp);
		ins.check_end(checkpoint);

		// 弹出分支表达式
		ins.push<InsType::POP>();
	}

	// 编译函数定义语句
	Index32 CompileFDS(CompileTable& table, InsEnv& env, ASTNode& ast) noexcept {
		__Function func;
		// 函数名
		func.index_name = table.pool.get(CompileID(ast[0ULL]));
		// 函数参数
		__Indexs indexs;
		func.va_targs = static_cast<Index32>(CompileTArgs(table, env, ast[1ULL], indexs));
		func.index_targs = table.pool.get(freestanding::move(indexs));
		// 函数返回值
		auto& node{ ast[2ULL] };
		auto has_ret{ node.syntaxToken() == SyntaxToken::REF };
		func.index_ret = has_ret ? table.pool.get(CompileRef(node)) : INone;
		// 函数体
		InsEnv envTmp;
		CompileBlock(func.insFunc, table, envTmp, has_ret ? ast[3ULL] : node);
		func.insFunc.push<InsType::PUSH_NULL>();
		func.insFunc.push<InsType::RETURN>();

		return table.pool.get(freestanding::move(func));
	}

	// 编译函数返回语句
	void CompileFRS(InsBlock& ins, CompileTable& table, InsEnv& env, ASTNode& ast) noexcept {
		auto checkpoint{ ins.check_start(ast.line()) };
		if (ast.size() == 1ULL) CompileE(ins, table, env, ast[0ULL]);
		else ins.push<InsType::PUSH_NULL>();
		ins.push<InsType::RETURN>();
		env.isTerminal = true; // return终止后续语句编译
		ins.check_end(checkpoint);
	}
}

namespace hy {
	// 编译语句块
	void CompileBlock(InsBlock& ins, CompileTable& table, InsEnv& env, ASTNode& ast) noexcept {
		for (auto& elem : ast) {
			CompileFunc func{ &CompileNothing };
			switch (elem.syntaxToken()) {
			case SyntaxToken::ABS: func = &CompileABS; break;
			case SyntaxToken::AS: func = &CompileAS; break;
			case SyntaxToken::ES: func = &CompileES; break;
			case SyntaxToken::CS: func = &CompileCS; break;
			case SyntaxToken::LS: func = &CompileLS; break;
			case SyntaxToken::LCS: func = &CompileLCS; break;
			case SyntaxToken::LBS: func = &CompileLBS; break;
			case SyntaxToken::ILS: func = &CompileILS; break;
			case SyntaxToken::SS: func = &CompileSS; break;
			case SyntaxToken::FRS: func = &CompileFRS; break;
			}
			func(ins, table, env, elem);
			// 优化: 因continue, break, return提前返回
			// 终止同语句块中后续语句的编译并重置终止标志
			if (env.isTerminal) {
				env.isTerminal = false;
				break;
			}
		}
	}
}

namespace hy {
	// 编译字节码导入语句
	inline void CompileBCImport(CompileTable& table, InsEnv&, ASTNode& ast) noexcept {
		auto& ins{ table.mainCode };
		for (auto& pIS : ast) {
			auto checkpoint{ ins.check_start(pIS.line()) };
			ins.push(pIS.size() == 2ULL ? InsType::PRE_IMPORT_USING : InsType::PRE_IMPORT,
				table.pool.get(CompileRef(pIS[0ULL])));
			ins.check_end(checkpoint);
		}
	}

	// 编译字节码命名空间语句
	inline void CompileBCUsing(CompileTable& table, InsEnv&, ASTNode& ast) noexcept {
		auto& ins = table.mainCode;
		for (auto& pUS : ast) {
			auto checkpoint{ ins.check_start(pUS.line()) };
			auto index_name{ table.pool.get(CompileRef(pUS[0ULL])) };
			if (pUS.size() == 2ULL) { // 软链接
				auto index_id{ table.pool.get(CompileID(pUS[1ULL])) };
				__Indexs indexs{ 2ULL };
				indexs[0ULL] = index_name;
				indexs[1ULL] = index_id;
				ins.push(InsType::PRE_SOFT_LINK, table.pool.get(freestanding::move(indexs)));
			}
			else ins.push(InsType::PRE_USING, index_name); // 命名空间
			ins.check_end(checkpoint);
		}
	}

	// 编译字节码原生语句
	inline void CompileBCNative(CompileTable& table, InsEnv&, ASTNode& ast) noexcept {
		auto& ins = table.mainCode;
		for (auto& pNS : ast) {
			auto checkpoint{ ins.check_start(pNS.line()) };
			ins.push(InsType::PRE_NATIVE, table.pool.get(CompileRef(pNS[0ULL])));
			ins.check_end(checkpoint);
		}
	}

	// 编译字节码常量定义语句
	inline void CompileBCConst(CompileTable& table, InsEnv&, ASTNode& ast) noexcept {
		auto& ins = table.mainCode;
		for (auto& pCVDS : ast) {
			auto checkpoint{ ins.check_start(pCVDS.line()) };
			auto& pc{ pCVDS[0] };
			__Indexs indexs{ 2ULL };
			indexs[0ULL] = table.pool.get(CompileID(pCVDS[1]));
			switch (pc.lexToken()) {
			case LexToken::INTEGER: indexs[1ULL] = table.pool.get(CompileInteger(pc)); break;
			case LexToken::DECIMAL: indexs[1ULL] = table.pool.get(CompileDecimal(pc)); break;
			case LexToken::COMPLEX: indexs[1ULL] = table.pool.get(CompileComplex(pc)); break;
			case LexToken::TRUE:
			case LexToken::FALSE: indexs[1ULL] = table.pool.get(static_cast<Int64>(CompileBoolean(pc))); break;
			case LexToken::LITERAL: indexs[1ULL] = table.pool.get(CompileString(pc)); break;
			default: indexs[1ULL] = INone; break;
			}
			ins.push(InsType::PRE_CONST, table.pool.get(freestanding::move(indexs)));
			ins.check_end(checkpoint);
		}
	}

	// 编译字节码类型定义语句
	inline void CompileBCClass(CompileTable& table, InsEnv& env, ASTNode& ast) noexcept {
		auto& ins = table.mainCode;
		for (auto& pTDS : ast) {
			auto checkpoint{ ins.check_start(pTDS.line()) };
			__Class cls;
			// 类名
			cls.index_name = table.pool.get(CompileID(pTDS[0ULL]));
			// 成员变量
			auto& ast_vars{ pTDS[1ULL] };
			cls.mvs.reserve(ast_vars.size());
			__Ref cpv_vars;
			for (auto& cpv : ast_vars) {
				auto index_vartype{ table.pool.get(CompileRef(cpv[0ULL])) };
				auto index_vars{ table.pool.get(CompileRef(cpv[1ULL])) };
				cls.mvs.emplace_back(index_vartype, index_vars);
			}
			// 成员函数
			auto& ast_funcs{ pTDS[2ULL] };
			cls.mfs.reserve(ast_funcs.size());
			for (auto& cpf : ast_funcs) cls.mfs.emplace_back(CompileFDS(table, env, cpf));

			ins.push(InsType::PRE_CLASS, table.pool.get(freestanding::move(cls)));
			ins.check_end(checkpoint);
		}
	}

	// 编译字节码概念定义语句
	inline void CompileBCConcept(CompileTable& table, InsEnv&, ASTNode& ast) noexcept {
		auto& ins = table.mainCode;
		for (auto& pCDS : ast) {
			auto checkpoint{ ins.check_start(pCDS.line()) };
			__Concept cpt;
			cpt.index_name = table.pool.get(CompileID(pCDS[1ULL]));
			CompileConcept(table, pCDS[0ULL], cpt.subs);
			ins.push(InsType::PRE_CONCEPT, table.pool.get(freestanding::move(cpt)));
			ins.check_end(checkpoint);
		}
	}

	// 编译字节码函数定义语句
	inline void CompileBCFunction(CompileTable& table, InsEnv& env, ASTNode& ast) noexcept {
		auto& ins = table.mainCode;
		for (auto& pFDS : ast) {
			auto checkpoint{ ins.check_start(pFDS.line()) };
			ins.push(InsType::PRE_FUNCTION, CompileFDS(table, env, pFDS));
			ins.check_end(checkpoint);
		}
	}

	// 编译字节码全局变量定义语句
	inline void CompileBCGlobalVariable(CompileTable& table, InsEnv&, ASTNode& ast) noexcept {
		auto& ins = table.mainCode;
		for (auto& pGVDS : ast) {
			auto checkpoint{ ins.check_start(pGVDS.line()) };
			ins.push(InsType::PRE_GLOBAL, table.pool.get(CompileRef(pGVDS[0ULL])));
			ins.check_end(checkpoint);
		}
	}

	// 编译字节码普通语句
	inline void CompileBCNormal(CompileTable& table, InsEnv& env, ASTNode& ast) noexcept {
		CompileBlock(table.mainCode, table, env, ast);
	}
}

namespace hy {
	inline void Clean(Compiler* compiler) noexcept {
		compiler->mBytes.clear();
	}

	inline void CompileAST(ASTNode& root, CompileTable& table) noexcept {
		InsEnv env;
		CompileBCImport(table, env, root[Syntaxer::AST_IS]);
		CompileBCUsing(table, env, root[Syntaxer::AST_US]);
		CompileBCNative(table, env, root[Syntaxer::AST_NS]);
		CompileBCConst(table, env, root[Syntaxer::AST_CVDS]);
		CompileBCClass(table, env, root[Syntaxer::AST_TDS]);
		CompileBCConcept(table, env, root[Syntaxer::AST_CDS]);
		CompileBCFunction(table, env, root[Syntaxer::AST_FDS]);
		CompileBCGlobalVariable(table, env, root[Syntaxer::AST_GVDS]);
		CompileBCNormal(table, env, root[Syntaxer::AST_S]);
	}

	LIB_EXPORT void CompilerCompile(Compiler * compiler, Syntaxer * syntaxer) noexcept {
		Clean(compiler);
		auto& root{ syntaxer->root };
		if (root.size() == Syntaxer::AST_COUNT) {
			CompileTable table;
			CompileAST(root, table);
			serialize::WriteByteCode(table, compiler->mBytes, compiler->cfg, syntaxer->source);
		}
	}
}