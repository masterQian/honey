#pragma once

#include "../lexer/hy.lexer.h"
#include "../syntaxer/hy.syntaxer.h"
#include "../compiler/hy.compiler.h"
#include "../vm/hy.vm.h"

#include "hy.cli.platform.h"

namespace hy::api {
#define LOADFUNC(name) name = static_cast<decltype(name)>(get(#name))

	class LinkLib {
	protected:
		Memory handle;

		bool load(const StringView fp) noexcept {
			handle = platform::LoadDll(fp);
			return handle != nullptr;
		}

		void* get(const char* name) noexcept {
			return handle ? platform::GetDllFunction(handle, name) : nullptr;
		}

	public:
		LinkLib() : handle{ } {}

		void free() noexcept {
			if (handle) platform::FreeDll(handle);
		}
	};

	inline struct LibHYC : LinkLib {
		CodeResult(*LexerAnalyse)(Lexer* lexer, CStr code) noexcept {};
		CodeResult(*SyntaxerAnalyse)(Syntaxer* syntaxer, Lexer* lexer) noexcept {};
		void (*CompilerCompile)(Compiler* compiler, Syntaxer* syntaxer) noexcept {};

		bool load() noexcept {
			auto ret{ LinkLib::load(u"hyc") };
			if (ret) {
				LOADFUNC(LexerAnalyse);
				LOADFUNC(SyntaxerAnalyse);
				LOADFUNC(CompilerCompile);
			}
			return ret;
		}
	}hyc;

	inline struct LibHYVM : LinkLib {
		void (*VMInitialize)(VM* vm) noexcept {};
		void (*VMClean)(VM* vm) noexcept {};
		void (*VMDestroy)(VM* vm) noexcept {};

		void (*RunByteCode)(VM* vm, Module* mod, util::ByteArray* bc, bool movebc) noexcept {};

		void (*SetError_CompileError)(VM* vm, CodeResult cr, const StringView name) noexcept {};
		void (*SetError_FileNotExists)(VM* vm, const StringView name) noexcept {};
		void (*SetError_RedefinedID)(VM* vm, const StringView name) noexcept {};
		void (*GetCompileErrorString)(const StringView name, CodeResult cr, String* str) noexcept {};
		void (*VMStackTrace)(CallStackTrace* cst, VM* vm) noexcept {};

		bool load() noexcept {
			auto ret{ LinkLib::load(u"hyvm") };
			if (ret) {
				LOADFUNC(VMInitialize);
				LOADFUNC(VMClean);
				LOADFUNC(VMDestroy);

				LOADFUNC(RunByteCode);

				LOADFUNC(SetError_CompileError);
				LOADFUNC(SetError_FileNotExists);
				LOADFUNC(SetError_RedefinedID);
				LOADFUNC(GetCompileErrorString);
				LOADFUNC(VMStackTrace);
			}
			return ret;
		}
	}hyvm;

#undef LOADFUNC
}