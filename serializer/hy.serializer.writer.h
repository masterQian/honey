/**************************************************
*
* @�ļ�				hy.serializer.writer
* @����				Ǯ����
* @����ʱ��			2021-11-30
* @����ʱ��			2022-11-30
* @ժҪ
* ��������������ֽ�������л�������
*
**************************************************/

#pragma once

#include "../compiler/hy.compiler.h"
#include "../compiler/hy.compiler.impl.h"

namespace hy::serialize {
	void WriteByteCode(CompileTable& table, util::ByteArray& bs, CompilerConfig& config, const StringView source) noexcept;
}