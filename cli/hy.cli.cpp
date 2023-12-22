#include "hy.cli.h"
#include "../public/hy.strings.h"

#include <fast_io/fast_io.h>

using namespace hy;

namespace Env {
#define ENVSTR(name, value) inline constexpr const Char name[] = u""#value
	ENVSTR(PROP_HELP, -h);
	ENVSTR(PROP_HELPEX, -help);
	ENVSTR(PROP_VERSION, -v);
	ENVSTR(PROP_VERSIONEX, -version);
	ENVSTR(PROP_TEST, -test);
	ENVSTR(PROP_COMPILE, -b);
	ENVSTR(KEY_SOURCE, src);
	ENVSTR(KEY_OUT, out);
	ENVSTR(KEY_LIBPATH, libpath);
	ENVSTR(KEY_WORKPATH, workpath);
#undef ENVSTR
}

namespace Device {
	fast_io::u8native_io_observer StandardInputHandle { fast_io::u8in() };
	fast_io::u8native_io_observer StandardOutputHandle { fast_io::u8out() };
	fast_io::u8native_io_observer StandardErrorHandle { fast_io::u8err() };

	bool CLICharInputFunc(String* str) noexcept {
		try {
			std::u8string u8str;
			scan(Device::StandardInputHandle, fast_io::mnp::line_get(u8str));
			if (u8str.back() == u'\r') u8str.pop_back();
			print(fast_io::u16ostring_ref(str), fast_io::mnp::code_cvt(u8str));
		}
		catch (fast_io::error&) {
			return false;
		}
		return true;
	}

	bool CLICharOutputFunc(const StringView str) noexcept {
		print(Device::StandardOutputHandle, fast_io::mnp::code_cvt(str));
		return true;
	}

	bool CLIErrorFunc(CallStackTrace* cst) noexcept {
		String message;
		fast_io::u16ostring_ref msgRef{ &message };
		println(msgRef, u"\n[ 0x", fast_io::mnp::uhexfull(static_cast<Token>(cst->error)),
			u" ] ", cst->msg);
		for (auto& info : cst->errStack) println(msgRef, u"\t< 行 ", info.line,
			u" > 模块 ", info.mod, u" | 函数 ", info.name);
		message.pop_back();
		print(Device::StandardErrorHandle, fast_io::mnp::code_cvt(message));
		return true;
	}
}

// 非法命令
void RunStop() noexcept {
	static String msg { fast_io::u16concat(
		u"不支持的命令参数序列, 请使用honey ",
		Env::PROP_HELP,
		u"查看帮助") };
	Device::CLICharOutputFunc(msg);
}

// 版本信息 -v | -version
void RunVersion() noexcept {
	static String msg { fast_io::u16concat(
		u"汉念(honey) 版本 ",
		fast_io::mnp::os_c_str(strings::VERSION_NAME),
		u" 64位   作者: ",
		fast_io::mnp::os_c_str(strings::AUTHOR)) };
	Device::CLICharOutputFunc(msg);
}

// 帮助 -h | -help
void RunHelp() noexcept {
	static String msg { uR"(
汉念命令行:

[属性命令]
-v | -version            查看汉念核心版本
-h | -help               查看汉念命令行帮助
-b                       编译成字节码

[键值命令]
src:[.hy | .hyb]         源文件路径
out:[]                   编译目标文件路径
)"
	};
	Device::CLICharOutputFunc(msg);
}

namespace hy {

}

// 测试 -test
#define OFF_OPTIMIZE 0
#if OFF_OPTIMIZE
#pragma optimize("", off)
#endif
void RunTest() noexcept {
#define TEST1 1

	constexpr auto N{ 500000ULL };
	auto t{ fast_io::posix_clock_gettime(fast_io::posix_clock_id::realtime) };

#if TEST1

#else


#endif

	println("\ntime: ", fast_io::posix_clock_gettime(fast_io::posix_clock_id::realtime) - t);
}
#if OFF_OPTIMIZE
#pragma optimize("", on)
#endif

// 运行 -b src:
void RunCompileSrc(util::Args& env) noexcept {
	// 编译主模块源码
	util::Path srcPath{ env.getView(Env::KEY_SOURCE) };
	util::Path workPath { env.getView(Env::KEY_WORKPATH) };
	if (workPath.empty() || workPath.isRelative()) workPath = platform::GetWorkPath();
	if (srcPath.isRelative()) srcPath = workPath + srcPath;
	auto mainName{ srcPath.getFileNameNoExt().toString() };
	if (srcPath.getExtension() == u".hy") {
		String code;
		if (platform::BrowserFile(srcPath, code)) {
			Lexer lexer;
			Syntaxer syntaxer;
			Compiler compiler; compiler.cfg.debugMode = true;
			auto cr{ api::hyc.LexerAnalyse(&lexer, code.data()) };
			if (cr) cr = api::hyc.SyntaxerAnalyse(&syntaxer, &lexer);
			if (cr) api::hyc.CompilerCompile(&compiler, &syntaxer);
			if (cr) {
				auto outPath { workPath + util::Path(mainName + strings::BYTECODE_NAME) };
				if (env.hasValue(Env::KEY_OUT)) {
					util::Path targetPath{ env.getView(Env::KEY_OUT) };
					if (targetPath.isRelative()) targetPath = workPath + targetPath;
					outPath = util::Path(targetPath.toString() + strings::BYTECODE_NAME);
				}
				if (!platform::SaveFile(outPath, compiler.mBytes))
					Device::CLICharOutputFunc(u"目标路径不合法或写出文件失败");
			}
			else {
				String msg;
				api::hyvm.GetCompileErrorString(mainName, cr, &msg);
				Device::CLICharOutputFunc(msg);
			}
		}
		else Device::CLICharOutputFunc(srcPath.toString() + u" 文件不存在");
	}
	else Device::CLICharOutputFunc(srcPath.toString() + u" 不是合法的源码文件");
}

// 运行 src:
void RunVMSrc(util::Args& env) noexcept {
	// 虚拟机初始化
	VM vm{ env };
	vm.libPath = vm.argv.getView(Env::KEY_LIBPATH);
	if (vm.libPath.empty() || vm.libPath.isRelative()) vm.libPath = platform::GetLibPath();
	vm.workPath = vm.argv.getView(Env::KEY_WORKPATH);
	if (vm.workPath.empty() || vm.workPath.isRelative()) vm.workPath = platform::GetWorkPath();
	platform::SetWorkPath(vm.workPath);
	vm.stdDevice = {
		&Device::CLICharInputFunc, &Device::CLICharOutputFunc,
		&Device::CLIErrorFunc, nullptr,
	};

	CallStackTrace cst;
	api::hyvm.VMInitialize(&vm);

	util::ByteArray hyb;

	// 编译主模块源码
	util::Path srcPath{ vm.argv.getView(Env::KEY_SOURCE) };
	if (srcPath.isRelative()) srcPath = vm.workPath + srcPath;
	auto mainName{ srcPath.getFileNameNoExt().toString() };

	if (srcPath.getExtension() == strings::BYTECODE_NAME) {
		if (!platform::BrowserFile(srcPath, hyb)) api::hyvm.SetError_FileNotExists(&vm, srcPath.toView());
	}
	else {
		String code;
		if (platform::BrowserFile(srcPath, code)) {
			Lexer lexer;
			Syntaxer syntaxer;
			Compiler compiler; compiler.cfg.debugMode = true;
			CodeResult cr{ api::hyc.LexerAnalyse(&lexer, code.data()) };
			if (cr) cr = api::hyc.SyntaxerAnalyse(&syntaxer, &lexer);
			if (cr) {
				api::hyc.CompilerCompile(&compiler, &syntaxer);
				freestanding::swap(hyb, compiler.mBytes);
			}
			else api::hyvm.SetError_CompileError(&vm, cr, mainName);
		}
		else api::hyvm.SetError_FileNotExists(&vm, srcPath.toView());
	}
	// 运行主模块字节码
	if (vm.ok()) {
		RefView mainNameRef{ mainName };
		if (vm.moduleTree.find(mainNameRef)) api::hyvm.SetError_RedefinedID(&vm, mainName);
		else {
			auto mainModule{ vm.moduleTree.add(mainNameRef, mainName, srcPath.getParent(), false) };
			api::hyvm.RunByteCode(&vm, mainModule, &hyb, true);
		}
	}
	// VM结束
	if (vm.error()) {
		api::hyvm.VMStackTrace(&cst, &vm);
		Device::CLIErrorFunc(&cst);
	}
	api::hyvm.VMDestroy(&vm);
}

int wmain(int argc, Str argv[]) {
	platform::AutoCurrentFolder();
	platform::SetConsoleUTF8();
	if (api::hyc.load() && api::hyvm.load()) {
		util::Args env;
		for (auto argi{ 1 }; argi < argc; ++argi) env.setView(argv[argi]);
		if (env.hasValue(Env::KEY_SOURCE)) {
			if (env.hasProp(Env::PROP_COMPILE)) RunCompileSrc(env);
			else RunVMSrc(env);
		}
		else {
			if (env.count() == 1ULL) {
				if (env.hasProp(Env::PROP_VERSION) || env.hasProp(Env::PROP_VERSIONEX)) RunVersion();
				else if (env.hasProp(Env::PROP_HELP) || env.hasProp(Env::PROP_HELPEX)) RunHelp();
				else if (env.hasProp(Env::PROP_TEST)) RunTest();
				else RunStop();
			}
			else RunStop();
		}
		api::hyvm.free();
		api::hyc.free();
	}
	return 0;
}