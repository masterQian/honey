/**************************************************
*
* @文件				hy.serializer.writer
* @作者				钱浩宇
* @创建时间			2021-11-30
* @更新时间			2022-11-30
* @摘要
* 汉念编译器生成字节码的序列化的声明
*
**************************************************/

#pragma once

#include "../compiler/hy.compiler.h"
#include "../compiler/hy.compiler.impl.h"

namespace hy::serialize {
	void WriteByteCode(CompileTable& table, util::ByteArray& bs, CompilerConfig& config, const StringView source) noexcept;
}