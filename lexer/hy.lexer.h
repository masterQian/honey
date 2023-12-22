/**************************************************
*
* @文件				hy.lexer
* @作者				钱浩宇
* @创建时间			2021-11-05
* @更新时间			2022-11-29
* @摘要
* 汉念词法分析器的声明
*
**************************************************/

#pragma once

#include "../public/hy.util.h"

namespace hy {
    constexpr auto HY_KEYWORD_IF{ u"if" };
    constexpr auto HY_KEYWORD_ELSE{ u"else" };
    constexpr auto HY_KEYWORD_WHILE{ u"while" };
    constexpr auto HY_KEYWORD_FOR{ u"for" };
    constexpr auto HY_KEYWORD_CONTINUE{ u"continue" };

    constexpr auto HY_KEYWORD_BREAK{ u"break" };
    constexpr auto HY_KEYWORD_SWITCH{ u"switch" };
    constexpr auto HY_KEYWORD_DEFAULT{ u"default" };
    constexpr auto HY_KEYWORD_STATIC{ u"static" };
    constexpr auto HY_KEYWORD_CONST{ u"const" };

    constexpr auto HY_KEYWORD_TRUE{ u"true" };
    constexpr auto HY_KEYWORD_FALSE{ u"false" };
    constexpr auto HY_KEYWORD_IMPORT{ u"import" };
    constexpr auto HY_KEYWORD_NATIVE{ u"native" };
    constexpr auto HY_KEYWORD_USING{ u"using" };

    constexpr auto HY_KEYWORD_FUNCTION{ u"function" };
    constexpr auto HY_KEYWORD_RETURN{ u"return" };
    constexpr auto HY_KEYWORD_CLASS{ u"class" };
    constexpr auto HY_KEYWORD_THIS{ u"this" };
    constexpr auto HY_KEYWORD_CONCEPT{ u"concept" };

    constexpr auto HY_KEYWORD_GLOBAL{ u"global" };

    constexpr auto HY_KEYWORD_COUNT{ 21ULL };

    // 词符号
    enum class LexToken : Token {
        /********终结符********/
        END_LEX = 0x0000U,  // 结束符
        COMMA = 0x0001U,  // 逗号
        SEMICOLON = 0x0002U,  // 分号
        COLON = 0x0003U,  // 冒号
        DOT = 0x0004U,  // 点
        POINTER = 0x0005U,  // 指向
        CONNECTOR = 0x0006U,  // 区间符
        ACUTE_LEFT = 0x0007U,  // 左尖括号
        ACUTE_RIGHT = 0x0008U,  // 右尖括号
        SMALL_LEFT = 0x0009U,  // 左小括号
        SMALL_RIGHT = 0x000AU,  // 右小括号
        MID_LEFT = 0x000BU,  // 左中括号
        MID_RIGHT = 0x000CU,  // 右中括号
        LARGE_LEFT = 0x000DU,  // 左大括号
        LARGE_RIGHT = 0x000EU,  // 右大括号
        LITERAL = 0x000FU,  // 字符串字面量
        INTEGER = 0x0010U,  // 整数
        DECIMAL = 0x0011U,  // 浮点数
        COMPLEX = 0x0012U,  // 复数
        ASSIGN = 0x0013U,  // 赋值
        ADD = 0x0014U,  // 加号
        SUBTRACT = 0x0015U,  // 减号
        MULTIPLE = 0x0016U,  // 乘号
        DIVIDE = 0x0017U,  // 除号
        MOD = 0x0018U,  // 百分号
        POWER = 0x0019U,  // 幂号
        NEGATIVE = 0x001AU,  // 负号
        ADD_ASSIGN = 0x001BU,  // 加赋值
        SUBTRACT_ASSIGN = 0x001CU,  // 减赋值
        MULTIPLE_ASSIGN = 0x001DU,  // 乘赋值
        DIVIDE_ASSIGN = 0x001EU,  // 除赋值
        MOD_ASSIGN = 0x001FU,  // 模赋值
        EQ = 0x0020U,  // 等于号
        GE = 0x0021U,  // 大于等于号
        LE = 0x0022U,  // 小于等于号
        NE = 0x0023U,  // 不等于号
        AND = 0x0024U,  // 与
        OR = 0x0025U,  // 或
        NOT = 0x0026U,  // 非
        ID = 0x0027U,  // 标识符
        AT = 0x0028U,  // @符
        ETC = 0x0029U,  // 省略号
        DOM = 0x002AU,  // 域引用符

        SIGN_START = END_LEX,  // 词符号开始
        SIGN_END = DOM,       // 词符号结束

        /********关键字********/
        IF = 0x8001U,      // 如果
        ELSE = 0x8002U,      // 否则
        WHILE = 0x8003U,      // 循环
        FOR = 0x8004U,      // 迭代
        CONTINUE = 0x8005U,      // 步过循环
        BREAK = 0x8006U,      // 跳出循环
        SWITCH = 0x8007U,      // 分支
        DEFAULT = 0x8008U,      // 默认
        STATIC = 0x8009U,      // 静态
        CONST = 0x800AU,      // 常量
        TRUE = 0x800BU,      // 真
        FALSE = 0x800CU,      // 假
        IMPORT = 0x800DU,      // 导入包
        NATIVE = 0x800EU,      // 定义函数
        USING = 0x800FU,      // 使用
        FUNCTION = 0x8010U,      // 定义函数
        RETURN = 0x8011U,      // 返回
        CLASS = 0x8012U,      // 定义类
        THIS = 0x8013U,      // 自身对象
        CONCEPT = 0x8014U,      // 概念
        GLOBAL = 0x8015U,       // 全局

        KEYWORD_START = IF,           // 关键字开始
        KEYWORD_END = GLOBAL,      // 关键字结束

        LEX_START = SIGN_START,   // 词开始
        LEX_END = KEYWORD_END,  // 词结束
    };

    // 词
    struct Lex {
        LexToken token;     // 词符号
        Index32 line;      // 行号
        CStr start;       // 词起始指针
        CStr end;         // 词结束指针
    };

    // 词法分析器配置
    struct LexerConfig {
        util::StringMap<LexToken> kwMap; // 关键字集
    };

    // 词法分析器
    struct Lexer {
        Index32 line;
        Vector<Lex> lexes;
        String source;
        LexerConfig cfg;
        Lexer() noexcept : line{ 1U } {}
        Lexer(const Lexer&) = delete;
        Lexer& operator = (const Lexer&) = delete;
    };
}