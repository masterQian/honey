/**************************************************
*
* @文件				hy.compiler
* @作者				钱浩宇
* @创建时间			2021-11-05
* @更新时间			2022-11-30
* @摘要
* 汉念编译器的声明
*
**************************************************/

#pragma once

#include "../public/hy.util.h"

namespace hy {
	// 资源集
	using ResourceSet = util::StringMap<util::ByteArray>;

	// 编译器配置
	struct CompilerConfig {
		ResourceSet resMap; // 资源集
		bool debugMode{ }; // 调试模式
		Byte minVer[4]{ }; // 最低虚拟机版本
		Byte maxVer[4]{ }; // 最高虚拟机版本
	};

	// 编译器
	struct Compiler {
		util::ByteArray mBytes;
		CompilerConfig cfg;

		Compiler() noexcept = default;
		Compiler(const Compiler&) = delete;
		Compiler& operator = (const Compiler&) = delete;
	};
}