/**************************************************
*
* @文件				hy.strings
* @作者				钱浩宇
* @创建时间			2021-11-05
* @更新时间			2022-11-29
* @摘要
* 汉念常量字符串
*
**************************************************/

#pragma once

#include "hy.types.h"

namespace hy::strings {
	constexpr const char* VERSION_ID{ "\x3\x0\x0\x0" };
	constexpr const CStr VERSION_NAME{ u"3.0.0.0" };
	constexpr const CStr AUTHOR{ u"钱浩宇" };
	constexpr const CStr LIB_NAME{ u"libs" };
	constexpr const CStr LIB_PATH_NAME{ u"libs\\" };
	constexpr const CStr SOURCE_NAME{ u".hy" };
	constexpr const CStr BYTECODE_NAME{ u".hyb" };
	constexpr const CStr BUILTIN_NAME{ u"builtin" };
	constexpr const char* HONEY_DLL_INIT_NAME{ "HoneyDllInitialize" };
	constexpr const char* HONEY_DLL_EXIT_NAME{ "HoneyDllExit" };
}