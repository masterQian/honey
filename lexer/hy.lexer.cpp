/**************************************************
*
* @文件				hy.lexer
* @作者				钱浩宇
* @创建时间			2021-11-05
* @更新时间			2022-11-29
* @摘要
* 汉念词法分析器的实现
*
**************************************************/

#include "hy.lexer.h"
#include "../public/hy.error.h"

namespace hy {
    struct TokenPos {
        LexToken token;
        CStr end;
    };

    struct PosLineError {
        CStr end;
        Index32 line;
        HYError error;
    };

    struct TokenPosError {
        LexToken token;
        CStr end;
        HYError error;
    };

    inline constexpr bool IsAriOptWithoutSub(Char ch) noexcept {
        return ch == u'+' || ch == u'*' || ch == u'/' || ch == u'%';
    }

    inline constexpr bool IsLogOpt(Char ch) noexcept {
        return ch == u'<' || ch == u'>' || ch == u'=' || ch == u'!';
    }

    inline constexpr bool IsChinese(Char ch) noexcept {
        return ch >= 0x4E00U && ch <= 0x9FA5U;
    }

    inline constexpr bool IsIdHead(Char ch) noexcept {
        return IsChinese(ch) || freestanding::cvt::isalpha(ch) || ch == u'_';
    }

    inline constexpr bool is_id(Char ch) noexcept {
        return IsChinese(ch) || freestanding::cvt::isalnum(ch) || ch == u'_';
    }

    inline constexpr bool IsNumber(LexToken token) noexcept {
        using enum LexToken;
        return token == INTEGER || token == DECIMAL || token == COMPLEX;
    }

    inline constexpr bool IsConsteval(LexToken token) noexcept {
        using enum LexToken;
        return IsNumber(token) || token == LITERAL || token == TRUE || token == FALSE;
    }

    inline constexpr bool IsLvNocRight(LexToken token) noexcept {
        using enum LexToken;
        return token == ID || token == THIS || token == MID_RIGHT || token == SMALL_RIGHT || token == LARGE_RIGHT;
    }

    inline constexpr bool IsExpRight(LexToken token) noexcept {
        return IsConsteval(token) || IsLvNocRight(token);
    }

    inline constexpr bool IsKeyword(LexToken token) noexcept {
        using enum LexToken;
        return token >= KEYWORD_START && token <= KEYWORD_END;
    }

    // 解析算术符号
    // 已确定start == + | * | / | %
    inline constexpr TokenPos TestAriOpt(CStr start) noexcept {
        Index offset{ };
        LexToken type{ };
        if (start[1] == u'=') ++offset;
        switch (*start) {
        case u'+': type = offset ? LexToken::ADD_ASSIGN : LexToken::ADD; break;
        case u'-': type = offset ? LexToken::SUBTRACT_ASSIGN : LexToken::SUBTRACT; break;
        case u'*': type = offset ? LexToken::MULTIPLE_ASSIGN : LexToken::MULTIPLE; break;
        case u'/': type = offset ? LexToken::DIVIDE_ASSIGN : LexToken::DIVIDE; break;
        case u'%': type = offset ? LexToken::MOD_ASSIGN : LexToken::MOD; break;
        default: break;
        }
        return { type, start + offset };
    }

    // 解析逻辑符号
    // 已确定start == < | > | = | !
    inline constexpr TokenPos TestLogOpt(CStr start) noexcept {
        Index offset{ };
        LexToken type{ };
        if (start[1] == u'=') ++offset;
        switch (*start) {
        case u'>': type = offset ? LexToken::GE : LexToken::ACUTE_RIGHT; break;
        case u'=': type = offset ? LexToken::EQ : LexToken::ASSIGN; break;
        case u'<': type = offset ? LexToken::LE : LexToken::ACUTE_LEFT; break;
        case u'!': type = offset ? LexToken::NE : LexToken::NOT; break;
        default: break;
        }
        return { type, start + offset };
    }

    // 解析标识符及关键字
    inline TokenPos TestId(LexerConfig& config, CStr start) noexcept {
        auto p{ start + 1 };
        while (is_id(*p)) ++p;
        if (auto key{ config.kwMap.get(StringView(start, p - start)) })
            return { *key, p - 1 };
        return { LexToken::ID, p - 1 };
    }

    // 解析数字
    // 已确定start是-号或数字
    inline constexpr TokenPosError TestDigit(CStr start) noexcept {
        Status status{ };
        auto type{ LexToken::INTEGER };
        for (auto p{ start };; ++p) {
            switch (status) {
            case 0: {
                if (freestanding::cvt::isdigit(*p)) status = 1;
                else if (*p == u'-') status = 7;
                else return { type, p, HYError::UNKNOWN_SYMBOL };
                break;
            }
            case 1: {
                if (freestanding::cvt::isdigit(*p)) continue;
                else if (*p == u'e' || *p == u'E') {
                    type = LexToken::DECIMAL;
                    status = 4;
                }
                else if (*p == u'.') {
                    type = LexToken::DECIMAL;
                    status = 2;
                }
                else if (*p == u'i') return { LexToken::COMPLEX, p, HYError::NO_ERROR };
                else return { type, p - 1, HYError::NO_ERROR };
                break;
            }
            case 2: {
                if (freestanding::cvt::isdigit(*p)) status = 3;
                else return { type, p - 1, HYError::UNKNOWN_SYMBOL };
                break;
            }
            case 3: {
                if (freestanding::cvt::isdigit(*p)) continue;
                else if (*p == u'e' || *p == u'E') status = 4;
                else if (*p == u'i') return { LexToken::COMPLEX, p, HYError::NO_ERROR };
                else return { type, p - 1, HYError::NO_ERROR };
                break;
            }
            case 4: {
                if (freestanding::cvt::isdigit(*p)) status = 6;
                else if (*p == u'+' || *p == u'-') status = 5;
                else return { type, p - 1, HYError::UNKNOWN_SYMBOL };
                break;
            }
            case 5: {
                if (freestanding::cvt::isdigit(*p)) status = 6;
                else return { type, p - 1, HYError::UNKNOWN_SYMBOL };
                break;
            }
            case 6: {
                if (freestanding::cvt::isdigit(*p)) continue;
                else if (*p == u'i') return { LexToken::COMPLEX, p, HYError::NO_ERROR };
                else return { type, p - 1, HYError::NO_ERROR };
            }
            case 7: {
                if (freestanding::cvt::isdigit(*p)) status = 1;
                else return { type, p - 1, HYError::UNKNOWN_SYMBOL };
                break;
            }
            }
        }
    }

    // 验证数字是否超出表示范围
    inline constexpr HYError CheckRange(CStr start, LexToken token) noexcept {
        if (token == LexToken::INTEGER)
            if (freestanding::cvt::check_stol(start)) return HYError::INT_OUT_OF_RANGE;
        return HYError::NO_ERROR;
    }

    // 解析空白
    // 已确定start是空白字符
    inline constexpr CStr TestDelim(CStr start) noexcept {
        for (++start; freestanding::cvt::isblank(*start); ++start);
        return start - 1;
    }

    // 解析点
    // 已确定start是.号
    inline constexpr TokenPos TestDot(CStr start) noexcept {
        if (start[1] == u'.' && start[2] == u'.')
            return { LexToken::ETC, start + 2 };
        return { LexToken::DOT, start };
    }

    // 解析注释
    // 已确定start是#号
    inline constexpr PosLineError TestAnnotate(CStr start) noexcept {
        if (start[1] == u'#') {
            Index32 line{ };
            for (++start; *start; ++start) {
                if (*start == u'\n') ++line;
                else if (*start == u'#' && start[1] == u'#')
                    return { start + 1, line, HYError::NO_ERROR };
            }
            return { start - 1, line, HYError::MISSING_ANNOTATE };
        }
        else {
            for (++start; *start && *start != u'\n'; ++start);
            if (*start) return { start, 1U, HYError::NO_ERROR };
            return { start - 1, 0U, HYError::NO_ERROR };
        }
    }

    // 解析字符串
    // 已确定start是"
    inline constexpr PosLineError TestLiteral(CStr start) noexcept {
        CStr p{ };
        Index32 line{ };
        for (p = ++start; *p; ++p) {
            if (*p == u'\\' && *(p + 1)) ++p; // 跳过转义符
            else if (*p == u'\n') ++line; // 新的一行
            else if (*p == u'\"') return { p, line, HYError::NO_ERROR };
        }
        if (p - start > sizeof(Char) * 6) p = start + 5; // 防止引号内容过多则部分省略
        return { p, line, HYError::MISSING_QUOTE }; // 缺失另一个双引号
    }

    // 普通标记映射(中文全角符号兼容)
    inline constexpr LexToken GetNormalLexToken(Char c) noexcept {
        switch (c) {
        case u'(':
        case u'\xff08': return LexToken::SMALL_LEFT;
        case u')':
        case u'\xff09': return LexToken::SMALL_RIGHT;
        case u'[': return LexToken::MID_LEFT;
        case u']': return LexToken::MID_RIGHT;
        case u'{': return LexToken::LARGE_LEFT;
        case u'}': return LexToken::LARGE_RIGHT;
        case u';':
        case u'\xff1b': return LexToken::SEMICOLON;
        case u',':
        case u'\xff0c': return LexToken::COMMA;
        case u'^': return LexToken::POWER;
        case u'~': return LexToken::CONNECTOR;
        case u'@': return LexToken::AT;
        case u'&': return LexToken::AND;
        case u'|': return LexToken::OR;
        default: return LexToken::END_LEX;
        }
    }
}

namespace hy {
    void CheckKeywords(LexerConfig& config) noexcept {
        static util::StringMap<LexToken> hy_default_keywords;
        if (hy_default_keywords.empty()) {
#define KS(key) hy_default_keywords.set(HY_KEYWORD_##key, LexToken::key)
            KS(IF); KS(ELSE); KS(WHILE); KS(FOR); KS(CONTINUE);
            KS(BREAK); KS(SWITCH); KS(DEFAULT); KS(STATIC); KS(CONST);
            KS(TRUE); KS(FALSE); KS(IMPORT); KS(NATIVE); KS(USING);
            KS(FUNCTION); KS(RETURN); KS(CLASS); KS(THIS); KS(CONCEPT);
            KS(GLOBAL);
#undef KS
        }
        auto useDefault{ true };
        if (config.kwMap.size() == HY_KEYWORD_COUNT) {
            HashSet<LexToken> keyset;
            for (auto& [key, value] : config.kwMap) {
                if (value >= LexToken::KEYWORD_START && value <= LexToken::KEYWORD_END)
                    keyset.emplace(value);
                else break;
            }
            useDefault = keyset.size() != HY_KEYWORD_COUNT;
        }
        if (useDefault) config.kwMap = hy_default_keywords;
    }

    inline void Clean(Lexer* lexer) noexcept {
        lexer->line = 1U;
        lexer->lexes.clear();
    }

    inline Lex* GetLast(Lexer* lexer) noexcept {
        return lexer->lexes.empty() ? nullptr : &lexer->lexes.back();
    }

    LIB_EXPORT CodeResult LexerAnalyse(Lexer * lexer, CStr code) noexcept {
        auto& [line, lexes, source, cfg] { *lexer };
        Clean(lexer);
        CheckKeywords(cfg);
        auto start{ code };
        for (CStr end{ }; *start;) {
            if (IsAriOptWithoutSub(*start)) { // 分析算术运算符
                auto [token, end_] { TestAriOpt(start) };
                end = end_;
                lexes.emplace_back(token, line, start, end);
            }
            else if (IsLogOpt(*start)) { // 分析逻辑运算符
                auto [token, end_] { TestLogOpt(start) };
                end = end_;
                lexes.emplace_back(token, line, start, end);
            }
            else if (IsIdHead(*start)) { // 分析标识符及关键字
                auto [token, end_] { TestId(cfg, start) };
                end = end_;
                lexes.emplace_back(token, line, start, end);
            }
            else if (freestanding::cvt::isdigit(*start)) { // 分析数字
                auto [token, end_, err] { TestDigit(start) };
                end = end_;
                if (!err) return CodeResult{ err, line, start, end };
                err = CheckRange(start, token);
                if (!err) return CodeResult{ err, line, start, end };
                if (Lex* pLast{ GetLast(lexer) }; pLast && pLast->token == LexToken::NEGATIVE) {
                    pLast->end = end; // 如果是负号则合并两者
                    pLast->token = token;
                }
                else lexes.emplace_back(token, line, start, end);
            }
            else if (freestanding::cvt::isblank(*start)) end = TestDelim(start); // 分析空白
            else { // 分析其他字符
                switch (*start) {
                case u'\r':
                case u'\n': { // 换行符
                    if (*start == u'\n') ++line;
                    end = start;
                    break;
                }
                case u'.': { // 点
                    auto [token, end_] = TestDot(start); end = end_;
                    lexes.emplace_back(token, line, start, end);
                    break;
                }
                case u':':
                case u'\xff1a': { // 冒号
                    end = start;
                    if (start[1] == u':' || start[1] == u'\xff1a')
                        lexes.emplace_back(LexToken::DOM, line, start, ++end);
                    else lexes.emplace_back(LexToken::COLON, line, start, end);
                    break;
                }
                case u'#': { // 注释
                    auto [end_, line_, err] { TestAnnotate(start) };
                    end = end_;
                    if (!err) return CodeResult{ err, line, start, end };
                    line += line_;
                    break;
                }
                case u'-': { // 减号
                    if (start[1] == u'>') { // 指向符
                        end = start + 1;
                        lexes.emplace_back(LexToken::POINTER, line, start, end);
                    }
                    else {
                        auto neg{ true };
                        if (auto pLast{ GetLast(lexer) }) {
                            auto lastToken{ pLast->token };
                            // 若上一个是E的右部识别为算术运算符-/-= , 否则识别为数字的负号
                            if (IsExpRight(lastToken)) {
                                auto [token, end_] { TestAriOpt(start) };
                                end = end_;
                                lexes.emplace_back(token, line, start, end);
                                neg = false;
                            }
                            else {
                                auto [token, end_, err] = TestDigit(start); end = end_;
                                if (~err) {
                                    err = CheckRange(start, token);
                                    if (!err) return CodeResult{ err, line, start, end };
                                    lexes.emplace_back(token, line, start, end); // 无异常时负号并入数字
                                    neg = false;
                                }
                            }
                        }
                        if (neg) { // 若之前无token或-后是数字但存在异常时识别为负号
                            end = start;
                            lexes.emplace_back(LexToken::NEGATIVE, line, start, end);
                        }
                    }
                    break;
                }
                case u'\"': { // 字符串
                    auto [end_, line_, err] { TestLiteral(start) };
                    end = end_;
                    if (~err) lexes.emplace_back(LexToken::LITERAL, line, start, end);
                    else return CodeResult{ err, line, start, end };
                    line += line_;
                    break;
                }
                default: {
                    auto token{ GetNormalLexToken(*start) };
                    end = start;
                    if (token != LexToken::END_LEX) [[likely]]
                        lexes.emplace_back(token, line, start, end);
                    else return CodeResult{ HYError::UNKNOWN_SYMBOL, line, start, end };
                    break;
                }
                }
            }
            start = end + 1;
        }
        lexes.emplace_back(LexToken::END_LEX, line, nullptr, nullptr); // 词法分析结束
        source.assign(code, start);
        return CodeResult{ HYError::NO_ERROR, line, nullptr, nullptr };
    }
}