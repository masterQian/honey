/**************************************************
*
* @文件				hy.vm
* @作者				钱浩宇
* @创建时间			2022-12-16
* @更新时间			2022-12-16
* @摘要
* 汉念虚拟机的声明
*
**************************************************/

#pragma once

#include "hy.vm.object.h"

namespace hy {
	// 符号表
	struct SymbolTable : private util::StringMap<Object*> {
		SymbolTable() noexcept = default;

		~SymbolTable() noexcept {
			for (auto& [name, obj] : *this) obj->unlink();
		}

		Object* getSymbol(const StringView name) noexcept {
			if (auto pObject{ get(name) }) return *pObject;
			return nullptr;
		}

		Object** getSymbolLV(const StringView name) noexcept {
			return get(name);
		}

		Object** setSymbolNotLink(const StringView name, Object* obj) noexcept {
			if (auto pObject{ get(name) }) return nullptr;
			else return &set(name, obj);
		}

		Object** setSymbol(const StringView name, Object* obj) noexcept {
			auto pObject{ setSymbolNotLink(name, obj) };
			if (pObject) obj->link();
			return pObject;
		}

		bool hasSymbol(const StringView name) const noexcept {
			return has(name);
		}
	};

	// 常量表
	using ConstTable = util::StringViewMap<Index32>;

	// 软链接表
	using LinkTable = util::StringViewMap<Object*>;

	// 作用域
	struct Dom {
		SymbolTable symbols; // 符号表
	};

	// 全局域
	struct GlobalDom {
		SymbolTable symbols; // 全局符号表
		ConstTable consts; // 常量表
		LinkTable links; // 软链接表
	};

	// 模块
	struct Module {
		Index id; // 模块编号
		String name; // 模块名
		util::Path modulePath; // 模块路径
		ByteCode bc; // 字节码
		HashSet<String> dllPaths; // 动态链接库路径表
		GlobalDom dom; // 全局域

		Module(Index i, StringView modName, const util::Path& modPath) noexcept : id{ i }, name(modName), modulePath(modPath) {}
	};

	// Dll接口
	using DllInitFunc = bool(*)(HVM hvm, Module* mod) noexcept;

	// 模块表
	struct ModuleTable {
		LinkListEx<Module> modData;
		util::StringMap<Module*> modMap;
		LinkListEx<Module*> usingList;
	};

	// 模块树
	struct ModuleTree {
		Module* builtin;
		TypeObject* prototype;
		ModuleTable modTable;

		ModuleTree() noexcept : builtin{ }, prototype{ } {}

		Module* add(RefView refView, StringView modName, const util::Path& modPath, bool use) noexcept {
			auto modRef{ refView.toString<u'.'>() };
			if (auto pMod{ modTable.modMap.get(modRef) }) return nullptr;
			auto id{ modTable.modData.size() };
			auto mod{ &modTable.modData.emplace_back(id, modName, modPath) };
			modTable.modMap.set(modRef, mod);
			if (use) modTable.usingList.emplace_back(mod);
			return mod;
		}

		Module* find(RefView refView) noexcept {
			if (auto pMod{ modTable.modMap.get(refView.toString<u'.'>()) }) return *pMod;
			return nullptr;
		}

		void setUsing(Module* mod) noexcept {
			auto pos{ modTable.usingList.cbegin() };
			for (auto end{ modTable.usingList.cend() }; pos != end; ++pos) {
				auto curMod{ *pos };
				if (curMod->id == mod->id) return; // 模块已引用无需插入
				else if (curMod->id > mod->id) break; // 找到模块按序插入位置
			}
			modTable.usingList.insert(pos, mod);
		}

		Size count() const noexcept {
			return modTable.modData.size();
		}
	};
}

namespace hy {
	// 调用
	struct Call {
		Module* mod; // 所在模块
		TypeObject* retType; // 返回值类型
		InsView* insView; // 指令视图
		const Ins* pIns; // 指令指针
		Object* thisObject; // this指针
		ObjArgs args_extra; // 拓展参数包
		String name; // 调用名
		Dom dom; // 作用域

		Call(Module* m, TypeObject* rt, InsView* iv, const StringView sv, Object* thisObj) noexcept :
			mod{ m }, retType{ rt }, insView{ iv }, pIns{ insView->insView.data() }, name{ sv }, thisObject{ thisObj } {}

		~Call() noexcept {
			for (auto arg_extra : args_extra) arg_extra->unlink();
		}
	};

	// 调用堆栈
	struct CallStack : private LinkList<Call> {
		Size cnt;

		CallStack() noexcept : cnt{ } {}

		Call& push(Module* mod, TypeObject* rt, InsView* iv, const StringView sv, Object* thisObj) noexcept {
			++cnt;
			return emplace_front(mod, rt, iv, sv, thisObj);
		}

		Call& top() noexcept {
			return front();
		}

		void pop() noexcept {
			--cnt;
			pop_front();
		}

		LinkList<Call>::iterator begin() noexcept {
			return LinkList<Call>::begin();
		}

		LinkList<Call>::iterator end() noexcept {
			return LinkList<Call>::end();
		}

		bool empty() const noexcept {
			return cnt == 0ULL;
		}

		Size size() const noexcept {
			return cnt;
		}

		void clear() noexcept {
			LinkList<Call>::clear();
			cnt = 0ULL;
		}
	};

	// 调用栈信息
	struct CallInfo {
		Index32 line; // 所在行
		String mod; // 模块名
		String name; // 调用名称

		CallInfo(Index32 l, const StringView m, const StringView n) noexcept : line{ l }, mod(m), name(n) {}
	};

	// 调用堆栈记录
	struct CallStackTrace {
		HYError error; // 错误ID
		String msg; // 错误信息
		Vector<CallInfo> errStack; // 调用栈信息
	};

	// 对象栈
	struct ObjectStack : util::TrivialStack<Object*> {
		void push_normal(Object* obj) noexcept {
			push(obj);
		}

		void push_link(Object* obj) noexcept { 
			obj->link();
			push(obj);
		}

		Object* pop_normal() noexcept {
			return pop_move();
		}

		void pop_unlink() noexcept { 
			pop_move()->unlink();
		}

		void clear() noexcept {
			for (auto p = mTop; p != mBase; --p) (*(p - 1))->unlink();
		}
	};
}

namespace hy {
	// 标准字符输入函数
	using CharInputFunc = bool (*)(String* str) noexcept;

	// 标准字符输出函数
	using CharOutputFunc = bool (*)(const StringView str) noexcept;

	// 标准错误函数
	using ErrorFunc = bool (*)(CallStackTrace* cst) noexcept;

	// 标准图形输出函数
	using GraphicOutputFunc = bool(*)() noexcept;

	// 标准设备
	struct StandardDevice {
		CharInputFunc charInputFunc{ };
		CharOutputFunc charOutputFunc{ };
		ErrorFunc errorFunc{ };
		GraphicOutputFunc graphicOutputFunc{ };
	};
}

namespace hy {
	// 虚拟机配置
	struct VMConfig {
		Size MaxStackDepth{ 0x1000ULL };
	};

	// 虚拟机
	struct VM {
		VMConfig cfg; // 虚拟机配置

		util::Path libPath; // 库路径
		util::Path workPath; // 工作路径
		util::Args argv; // 环境变量

		ModuleTree moduleTree; // 模块树

		RuntimeResult result; // 运行结果

		ObjectStack objectStack; // 对象栈

		CallStack callStack; // 调用堆栈

		StandardDevice stdDevice; // 标准设备

		Memory(*syscall)(const StringView) noexcept; // 系统调用

		VM(const util::Args& args) noexcept {
			result.error = HYError::NO_ERROR;
			argv = freestanding::move(args);
			syscall = nullptr;
		}

		VM(const VM&) = delete;
		VM& operator = (const VM&) = delete;

		bool ok() const noexcept {
			return result.error == HYError::NO_ERROR;
		}

		bool error() const noexcept {
			return result.error != HYError::NO_ERROR;
		}

		TypeObject* getType(TypeId id) noexcept {
			return static_cast<TypeStaticData*>(moduleTree.prototype->v_static)->builtinTypes[static_cast<Token>(id)];
		}
	};

	inline constexpr VM* vm_cast(HVM hvm) noexcept { return static_cast<VM*>(hvm); }
	inline constexpr Module* mod_cast(HMOD hmod) noexcept { return static_cast<Module*>(hmod); }
}