/**************************************************
*
* @文件				hy.serializer.reader
* @作者				钱浩宇
* @创建时间			2021-11-30
* @更新时间			2022-11-30
* @摘要
* 汉念编译器生成字节码的反序列化的声明
*
**************************************************/

#pragma once

#include "hy.serializer.bytecode.h"

namespace hy::serialize {
	bool ReadByteCode(util::ByteArray&& ba, ByteCode& bc) noexcept;
	bool ReadByteCode(util::ByteArray& ba, ByteCode& bc) noexcept;
}