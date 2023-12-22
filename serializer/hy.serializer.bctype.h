/**************************************************
*
* @文件				hy.serializer.bctype
* @作者				钱浩宇
* @创建时间			2021-12-12
* @更新时间			2022-12-12
* @摘要
* 包含了字节码中的通用类型定义
*
**************************************************/

#pragma once

#include "../public/hy.types.h"

namespace hy {
	constexpr auto INone{ 0xFFFFFFU };
	constexpr auto INone32{ 0xFFFFFFFFU };
	constexpr auto INone64{ 0xFFFFFFFFFFFFFFFFULL };

	// 平台类型
	enum class PlatformType : Byte {
		Unknown = 0U,
		Windows = 1U,
		Linux = 2U,
		Mac = 4U,
		Android = 8U
	};

	// 字节码头
	struct BCHeader {
		Byte magic[4]; // 标识
		Size32 hash; // 字节码哈希值
		Size hashCount; // 哈希字节长度
		Byte version[4]; // 版本号
		Byte minVer[4]; // 要求最低虚拟机版本
		Byte maxVer[4]; // 要求最高虚拟机版本
		struct Platform {
			PlatformType os;	// 操作系统
			Byte extra1;		// 附加信息1
			Byte extra2;		// 附加信息2
			Byte extra3;		// 附加信息3
		}platform; // 平台
		Byte unused[16]; // 未使用
	};

	// 字面值类型
	enum class LiteralType : Byte {
		INT, // 整数
		FLOAT, // 浮点数
		COMPLEX, // 复数
		INDEXS, // 索引组
		STRING, // 字符串
		REF, // 引用组
		VECTOR, // 向量
		MATRIX, // 矩阵
		RANGE, // 范围
		FUNCTION, // 函数
		CLASS, // 类
		CONCEPT, // 概念
	};

	// 子概念类型
	enum class SubConceptElementType : Token {
		SET,
		NOT,
		JUMP_TRUE,
		JUMP_FALSE
	};

	// 子概念
	struct SubConceptElement {
		SubConceptElementType type; // 子概念类型
		Index32 arg; // 子概念名 | jump距离
		SubConceptElement() noexcept : type{ SubConceptElementType::NOT }, arg{ } {}
		explicit SubConceptElement(Index32 index) noexcept : type{ SubConceptElementType::SET }, arg{ index } {}
		SubConceptElement(SubConceptElementType scet, Index32 v) noexcept : type{ scet }, arg{ v } {}
	};
}

namespace hy {
	inline constexpr PlatformType PLATFORM_TYPE =
#if defined(_WIN64)
		PlatformType::Windows
#elif defined(__APPLE__)
		PlatformType::Mac
#elif defined(__linux__)
		PlatformType::Linux
#elif defined(__ANDROID__)
		PlatformType::Android
#else
		PlatformType::Unknown
#endif
		;

	inline constexpr StringView GetPlatformTypeString(PlatformType type) noexcept {
		switch (type) {
		case PlatformType::Windows: return u"Windows";
		case PlatformType::Mac: return u"mac";
		case PlatformType::Linux: return u"linux";
		case PlatformType::Android: return u"android";
		default: return u"unknown";
		}
	}
}