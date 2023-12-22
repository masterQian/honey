#include "hy.vm.impl.h"
#include "../public/hy.strings.h"
#include "../serializer/hy.serializer.reader.h"

namespace hy {
	// 模块内查找符号
	inline Object* FindSymbolInModule(Module* mod, const StringView sv) noexcept {
		// 查找顺序: 全局符号表 -> 软链接
		if (auto obj{ mod->dom.symbols.getSymbol(sv) }) return obj;
		if (auto pObj{ mod->dom.links.get(sv) }) return *pObj;
		return nullptr;
	}

	// 模块全局域查找符号
	inline Object* FindSymbolInModules(VM* vm, const StringView sv) noexcept {
		// 查找顺序: 当前模块 -> builtin模块 -> 其他已引用模块
		auto curMod{ vm->callStack.top().mod };
		Object* obj{ };
		if (obj = FindSymbolInModule(curMod, sv)) return obj; // 当前模块
		if (obj = FindSymbolInModule(vm->moduleTree.builtin, sv)) return obj; // builtin模块
		for (auto pMod : vm->moduleTree.modTable.usingList) { // 除当前模块外的其他已引用模块
			if (pMod != curMod) {
				if (obj = FindSymbolInModule(pMod, sv)) return obj;
			}
		}
		return obj;
	}

	// 全局查找符号
	inline Object* FindSymbol(VM* vm, const StringView sv) noexcept {
		// 查找顺序: 域顶符号表 -> 模块列表
		if (auto obj{ vm->callStack.top().dom.symbols.getSymbol(sv) }) return obj;
		return FindSymbolInModules(vm, sv);
	}

	// 模块内查找符号左值
	inline Object** FindSymbolLVInModule(Module* mod, const StringView sv) noexcept {
		// 查找顺序: 全局符号表
		return mod->dom.symbols.getSymbolLV(sv);
	}

	// 模块全局域查找符号左值
	inline Object** FindSymbolLVInModules(VM* vm, const StringView sv) noexcept {
		// 查找顺序: 当前模块 -> builtin模块 -> 其他已引用模块
		auto curMod{ vm->callStack.top().mod };
		Object** pObj{ };
		if (pObj = FindSymbolLVInModule(curMod, sv)) return pObj;
		if (pObj = FindSymbolLVInModule(vm->moduleTree.builtin, sv)) return pObj;
		for (auto pMod : vm->moduleTree.modTable.usingList) {
			if (pMod != curMod) {
				if (pObj = FindSymbolLVInModule(pMod, sv)) return pObj;
			}
		}
		return pObj;
	}

	// 全局查找符号左值
	inline Object** FindSymbolLV(VM* vm, const StringView sv) noexcept {
		// 查找顺序: 域顶符号表 -> 模块列表
		if (auto pObj{ vm->callStack.top().dom.symbols.getSymbolLV(sv) }) return pObj;
		return FindSymbolLVInModules(vm, sv);
	}

	// 查找引用
	inline Object* FindRef(VM* vm, RefView refView) noexcept {
		RefView modName;
		auto varName{ refView.detach(modName) };
		Object* obj{ };
		if (modName.empty()) obj = FindSymbolInModules(vm, varName); // 单标识符
		else { // 模块标识符引用
			if (auto mod = vm->moduleTree.find(modName)) obj = FindSymbolInModule(mod, varName);
		}
		return obj;
	}

	// 查找引用左值
	inline Object** FindRefLV(VM* vm, RefView refView) noexcept {
		RefView modName;
		auto varName{ refView.detach(modName) };
		Object** pObj{ };
		if (modName.empty()) pObj = FindSymbolLVInModules(vm, varName);
		else {
			if (auto mod = vm->moduleTree.find(modName)) pObj = FindSymbolLVInModule(mod, varName);
		}
		return pObj;
	}

	// 查找常量
	inline Index32* FindConst(VM* vm, RefView refView) noexcept {
		auto curMod{ vm->callStack.top().mod };
		RefView modName;
		auto varName{ refView.detach(modName) };
		Index32* entry{ };
		if (modName.empty()) { // 单标识符
			if (entry = curMod->dom.consts.get(varName)) return entry;
			if (entry = vm->moduleTree.builtin->dom.consts.get(varName)) return entry;
			for (auto pMod : vm->moduleTree.modTable.usingList) {
				if (pMod != curMod) {
					if (entry = pMod->dom.consts.get(varName)) return entry;
				}
			}
		}
		else {
			if (auto mod{ vm->moduleTree.find(modName) }) entry = mod->dom.consts.get(varName);
		}
		return entry;
	}
}

namespace hy {
	// 校验字节码
	inline IResult<void> VerifyByteCode(VM* vm, ByteCode* bc) noexcept {
		auto& source { bc->source };
		auto header{ bc->pHeader };
		// 校验字节码标识
		if (header->magic[0] != 0x20 || header->magic[1] != 0x01 ||
			header->magic[2] != 0x11 || header->magic[3] != 0x05)
			return SetError(&SetError_ByteCodeBroken, vm);
		// 校验字节码哈希值长度
		auto hashStart{ source.data() + ByteCode::MIN_SIZE };
		if (source.size() < ByteCode::MIN_SIZE + header->hashCount)
			return SetError(&SetError_ByteCodeBroken, vm);
		// 校验字节码哈希值
		auto hash{ freestanding::cvt::hash_bytes<Size32>(hashStart, hashStart + header->hashCount) };
		if (hash != bc->pHeader->hash)
			return SetError(&SetError_ByteCodeBroken, vm);
		// 校验字节码版本号
		if (freestanding::compare(header->version, strings::VERSION_ID, freestanding::size(header->version)) != 0)
			return SetError(&SetError_UnmatchedVersion, vm, header->version, strings::VERSION_NAME);
		// 校验操作系统平台
		if (header->platform.os != PLATFORM_TYPE)
			return SetError(&SetError_UnmatchedPlatform, vm, static_cast<PlatformType>(header->platform.os), PLATFORM_TYPE);
		return IResult<void>(true);
	}

	// 检查参数合法性
	// 返回 n > 0, targs与args前部匹配, args的后n个参数将以拓展可变参数包形式存入Call中
	// 返回 n == 0, targs与args完全匹配, 无拓展可变参数包
	// 返回 n == -1, 参数不合法
	inline Int64 CheckArgs(ObjTArgs* targs, ObjArgsView args) noexcept {
		auto targc{ targs->args.size() }, argc{ args.size() };
		if (argc < targc || (argc > targc && !targs->va_args)) return -1;
		for (Index i{ }; i < targc; ++i) {
			auto type1{ targs->args[i].type }; // 可空不比较
			auto type2{ args[i]->type };
			// type2 -> type1 是否合法
			if (type1 && !type1->canImplement(type2)) return -1;
		}
		return static_cast<Int64>(argc - targc);
	}

	// 检查原生函数参数合法性
	inline bool CheckNativeArgs(ObjTArgs* targs, ObjArgsView args) noexcept {
		auto targc{ targs->args.size() };
		if (targc != args.size()) return false;
		for (Index i{ }; i < targc; ++i) {
			auto type1{ targs->args[i].type };
			if (!type1) continue;
			if (type1 != args[i]->type) return false;
		}
		return true;
	}

	// 普通函数调用
	IResult<void> CallFunction(VM* vm, FunctionObject* fobj, ObjArgsView args, Object* thisObject) noexcept {
		auto& cst{ vm->callStack };
		// 检查函数参数合法性
		auto& nf{ fobj->data.normal };
		auto diff{ CheckArgs(&fobj->targs, args) };
		if (diff == -1) return SetError(&SetError_UnmatchedCall, vm, fobj->name, args);
		// 处理栈溢出
		if (cst.size() > vm->cfg.MaxStackDepth) return SetError(&SetError_StackOverflow, vm);
		// 函数调用开始
		auto& newCall{ cst.push(mod_cast(nf.hmod), nf.ret, nf.insView, fobj->name, thisObject) };
		// 参数转移
		auto i{ 0ULL }, targc{ fobj->targs.args.size() };
		for (i = 0ULL; i < targc; ++i)
			newCall.dom.symbols.setSymbol(fobj->targs.args[i].name, args[i]);
		// 拓展参数转移
		if (auto count_ex{ static_cast<Size>(diff) }) {
			newCall.args_extra.resize(count_ex);
			for (i = 0ULL; i < count_ex; ++i) {
				auto arg_extra{ args[targc + i] };
				newCall.args_extra[i] = arg_extra;
				arg_extra->link();
			}
		}
		return IResult<void>(true);
	}

	// 注册变量包
	inline void RegisterPackVariable(SymbolTable& symbols, ObjArgsView args, RefView refView) noexcept {
		Size i{ };
		for (auto strView : refView) {
			auto arg{ args[i] };
			StringView sv{ strView.data(), strView.size() };
			if (!sv.empty()) { // 参数非...省略
				if (auto pObj{ symbols.getSymbolLV(sv) }) { // 存在同名变量
					arg->link();
					(*pObj)->unlink();
					*pObj = arg;
				}
				else symbols.setSymbol(sv, arg); // 注册新变量
			}
			++i;
		}
	}

	// 提取函数结构
	IResult<FunctionObject*> FetchFunctionView(VM* vm, Module* mod, FunctionView& fv, const StringView name) noexcept {
		auto& ls{ mod->bc.values };
		// 函数返回值类型
		auto type_ret{ vm->getType(TypeId::Null) };
		if (fv.hasRet()) {
			auto refView_ret{ ls.getRef(fv.index_ret()) };
			if (auto obj{ FindRef(vm, refView_ret) }) {
				if (obj->type->v_id != TypeId::Type)
					return SetError<FunctionObject*>(&SetError_NotType, vm, refView_ret);
				type_ret = obj_cast<TypeObject>(obj);
			}
			else return SetError<FunctionObject*>(&SetError_UndefinedRef, vm, refView_ret);
		}

		// 函数参数
		HashSet<StringView> argFilter;
		auto index_targs{ ls.getIndexs(fv.index_targs()) };
		ObjTArgs targs;
		targs.va_args = fv.vaTargs();
		auto size { index_targs.size() };
		if (size) {
			targs.args.resize(static_cast<Size>(size >> 1U));
			for (Size32 i{ }, j{ }; j < size; ++i) {
				auto& targ{ targs.args[i] };
				auto argName{ ls.getString(index_targs[j++]) };
				if (argFilter.contains(argName))
					return SetError<FunctionObject*>(&SetError_RedefinedID, vm, argName);
				argFilter.emplace(argName);
				targ.name = argName;
				auto index_type{ index_targs[j++] };
				if (index_type == INone) targ.type = nullptr;
				else {
					auto refView_targ{ ls.getRef(index_type) };
					auto obj{ FindRef(vm, refView_targ) };
					if (obj) {
						if (obj->type->v_id != TypeId::Type)
							return SetError<FunctionObject*>(&SetError_NotType, vm, refView_targ);
						targ.type = obj_cast<TypeObject>(obj);
					}
					else return SetError<FunctionObject*>(&SetError_UndefinedRef, vm, refView_targ);
				}
			}
		}

		// 创建函数对象
		auto fobj{ obj_allocate<FunctionObject>(vm->getType(TypeId::Function)) };
		fobj->ft = FunctionType::NORMAL;
		fobj->name = name;
		fobj->targs = freestanding::move(targs);
		fobj->data.normal.hmod = mod;
		fobj->data.normal.ret = type_ret;
		fobj->data.normal.insView = &fv.insView;

		return IResult<FunctionObject*>(fobj);
	}

	// 运行指令集
	IResult<void> RunCallStack(VM* vm, Size cstCount) noexcept {
		auto& ost{ vm->objectStack };
		auto& cst{ vm->callStack };
		while (cst.size() != cstCount) {
			auto& topCall{ cst.top() };
			auto& topMod{ topCall.mod };
			auto& ls{ topMod->bc.values };

			// 函数调用结束
			if (topCall.pIns == topCall.insView->insView.cend()) {
				cst.pop();
				continue;
			}

			// 取指令执行
			auto& ins{ *topCall.pIns };

			switch (ins.type) {
			case InsType::NOP: break; // 空指令
			case InsType::PUSH_LITERAL: {
				auto& view{ ls[ins.get<Index32>()] };
				Object* obj{ };
				switch (view.type) {
				case LiteralType::INT:
					obj = obj_allocate(vm->getType(TypeId::Int), arg_cast(view.v.vInt)); break;
				case LiteralType::FLOAT:
					obj = obj_allocate(vm->getType(TypeId::Float), arg_cast(view.v.vFloat)); break;
				case LiteralType::COMPLEX:
					obj = obj_allocate(vm->getType(TypeId::Complex), arg_cast(view.v.vComplex.re), arg_cast(view.v.vComplex.im));
					break;
				case LiteralType::STRING:
					obj = obj_allocate(vm->getType(TypeId::String), arg_cast(view.v.vString.data()), arg_cast(static_cast<Size>(view.v.vString.size())));
					break;
				default: return SetError(&SetError_ByteCodeBroken, vm);
				}
				ost.push_link(obj);
				break;
			}
			case InsType::PUSH_BOOLEAN: {
				ost.push_link(obj_allocate(vm->getType(TypeId::Bool), arg_cast(static_cast<Int64>(ins.get<bool>()))));
				break;
			}
			case InsType::PUSH_THIS: {
				ost.push_link(topCall.thisObject);
				break;
			}
			case InsType::PUSH_NULL: {
				ost.push_link(obj_allocate(vm->getType(TypeId::Null)));
				break;
			}
			case InsType::PUSH_SYMBOL: {
				auto name{ ls.getString(ins) };
				if (auto obj{ FindSymbol(vm, name) }) ost.push_link(obj);
				else return SetError(&SetError_UndefinedID, vm, name);
				break;
			}
			case InsType::PUSH_REF: {
				auto refView{ ls.getRef(ins) };
				if (auto obj{ FindRef(vm, refView) }) ost.push_link(obj);
				else return SetError(&SetError_UndefinedRef, vm, refView);
				break;
			}
			case InsType::PUSH_SYMBOL_LV: {
				auto name{ ls.getString(ins) };
				auto pObj{ FindSymbolLV(vm, name) };
				if (!pObj) pObj = topCall.dom.symbols.setSymbol(name, obj_allocate(vm->getType(TypeId::Null)));
				auto lv{ obj_allocate<LVObject>(vm->getType(TypeId::LV)) };
				lv->parent = *pObj;
				lv->parent->link();
				lv->lvType = LVType::ADDRESS;
				lv->storage.address = pObj;
				ost.push_link(lv);
				break;
			}
			case InsType::PUSH_REF_LV: {
				auto refView{ ls.getRef(ins) };
				if (auto pObj{ FindRefLV(vm, refView) }) {
					auto lv{ obj_allocate<LVObject>(vm->getType(TypeId::LV)) };
					lv->parent = *pObj;
					lv->parent->link();
					lv->lvType = LVType::ADDRESS;
					lv->storage.address = pObj;
					ost.push_link(lv);
				}
				else return SetError(&SetError_UndefinedRef, vm, refView);
				break;
			}
			case InsType::PUSH_CONST: {
				// 取常量的名称
				auto refView{ ls.getRef(ins) };
				auto entry{ FindConst(vm, refView) };
				if (!entry) return SetError(&SetError_UndefinedConst, vm, refView);
				auto& view{ ls[*entry] };
				Object* obj{ };
				// 判断常量类型
				switch (view.type) {
				case LiteralType::INT:
					obj = obj_allocate(vm->getType(TypeId::Int), arg_cast(view.v.vInt)); break;
				case LiteralType::FLOAT:
					obj = obj_allocate(vm->getType(TypeId::Float), arg_cast(view.v.vFloat)); break;
				case LiteralType::COMPLEX:
					obj = obj_allocate(vm->getType(TypeId::Complex), arg_cast(view.v.vComplex.re), arg_cast(view.v.vComplex.im));
					break;
				case LiteralType::STRING:
					obj = obj_allocate(vm->getType(TypeId::String), arg_cast(view.v.vString.data()), arg_cast(static_cast<Size>(view.v.vString.size())));
					break;
				default: return SetError(&SetError_ByteCodeBroken, vm);
				}
				// 常量转换成对象, 链接, 入栈
				ost.push_link(obj);
				break;
			}
			case InsType::PUSH_LIST: {
				auto count{ static_cast<Size>(ins.get<Size32>()) };
				auto obj{ obj_allocate<ListObject>(vm->getType(TypeId::List)) };
				obj->objects.resize(count);
				// POP对象已被链接一次, 进入list时无需链接
				for (Size i{ }; i < count; ++i) obj->objects[count - i - 1] = ost.pop_normal();
				ost.push_link(obj);
				break;
			}
			case InsType::PUSH_DICT: {
				auto count{ static_cast<Size>(ins.get<Size32>()) };
				ObjArgs keys{ count }, values{ count };
				for (Size i{ }; i < count; ++i) {
					values[count - i - 1] = ost.pop_normal();
					keys[count - i - 1] = ost.pop_normal();
				}
				auto mobj{ obj_allocate<MapObject>(vm->getType(TypeId::Map)) };
				mobj->link();
				for (Size i{ }; i < count; ++i) {
					impl::Map_Set(vm, mobj, keys[i], values[i]);
					if (vm->error()) break;
				}
				for (auto key : keys) key->unlink();
				for (auto value : values) value->unlink();
				ost.push_normal(mobj);
				if (vm->error()) return IResult<void>();
				break;
			}
			case InsType::PUSH_VECTOR: {
				auto index{ ins.get<Index32>() };
				Memory data{ }, size{ };
				if (index != INone) {
					auto vectorView{ ls.getVector(ins) };
					data = vectorView.data();
					size = arg_cast(static_cast<Size>(vectorView.size()));
				}
				ost.push_link(obj_allocate(vm->getType(TypeId::Vector), data, size));
				break;
			}
			case InsType::PUSH_MATRIX: {
				if (auto index{ ins.get<Index32>() }; index != INone) {
					auto matrixView{ ls.getMatrix(ins) };
					Size sizeData[] { static_cast<Size>(matrixView.row()), static_cast<Size>(matrixView.col()) };
					ost.push_link(obj_allocate(vm->getType(TypeId::Matrix), matrixView.data(), sizeData));
				}
				else ost.push_link(obj_allocate(vm->getType(TypeId::Matrix)));
				break;
			}
			case InsType::PUSH_RANGE: {
				auto rangeView{ ls.getRange(ins) };
				auto data{ reinterpret_cast<Int64*>(rangeView.data()) };
				if (RangeObject::check(data[0], data[1], data[2]))
					ost.push_link(obj_allocate(vm->getType(TypeId::Range), data));
				else return SetError(&SetError_IllegalRange, vm, data[0], data[1], data[2]);
				break;
			}
			case InsType::SAVE_PACK: {
				auto refView{ ls.getRef(ins) };
				auto count{ refView.count() };
				// 逆向读取解包参数
				ObjArgs args{ count };
				for (Size i{ }; i < count; ++i) args[count - i - 1] = ost.pop_normal();
				RegisterPackVariable(topCall.dom.symbols, ObjArgsView{ args.data(), count }, refView);
				for (auto arg : args) arg->unlink();
				break;
			}
			case InsType::POP: {
				ost.pop_unlink();
				break;
			}
			case InsType::ASSIGN: {
				auto count{ ins.get<Uint32>() };
				// 取出count个赋值参数
				ObjArgs args{ count };
				for (auto& arg : args) arg = ost.pop_normal();
				// 取出值
				auto value = ost.pop_normal();
				// 依次赋值
				for (auto i{ 1ULL }; i <= count; ++i) {
					// 左值引用
					auto lv{ obj_cast<LVObject>(args[count - i]) };
					if (lv->parent->type->a_def) {
						if (lv->lvType == LVType::ADDRESS) {
							// 左值地址赋值
							auto addressObj{ lv->getAddressObject<Object>() };
							value->link();
							addressObj->unlink();
							lv->setAddress(value);
						}
						else {
							// 左值数据赋值
							auto parentType{ lv->parent->type };
							if (parentType->f_assign_lv_data) parentType->f_assign_lv_data(vm, lv, value);
							else SetError_IncompatibleAssign(vm, parentType, value->type);
							if (vm->error()) break;
						}
					}
					else {
						SetError_IncompatibleAssign(vm, lv->parent->type, value->type);
						break;
					}
				}
				// 被赋值对象, 值解除链接
				for (auto arg : args) arg->unlink();
				value->unlink();
				if (vm->error()) return IResult<void>();
				break;
			}
			case InsType::ASSIGN_EX: {
				auto opt{ static_cast<AssignType>(ins.get<Byte>()) }; // 运算符
				auto obj2{ ost.pop_normal() }; // 左值
				auto obj1{ ost.pop_normal() }; // 表达式
				// 左值引用
				auto lv{ obj_cast<LVObject>(obj2) };
				if (lv->parent->type->a_def) {
					if (lv->lvType == LVType::ADDRESS) {
						// 左值地址赋值
						auto type{ lv->getAddressObject<Object>()->type };
						if (type->f_calcassign) type->f_calcassign(vm, lv, obj1, opt);
						else SetError_IncompatibleCalcAssign(vm, type, obj1->type, opt);
					}
					else {
						// 左值数据赋值
						auto type{ lv->parent->type };
						if (type->f_calcassign_lv_data) type->f_calcassign_lv_data(vm, lv, obj1, opt);
						else SetError_IncompatibleCalcAssign(vm, type, obj1->type, opt);
					}
				}
				else SetError_IncompatibleCalcAssign(vm, lv->parent->type, obj1->type, opt);
				// 操作数解除链接
				obj1->unlink();
				obj2->unlink();
				if (vm->error()) return IResult<void>();
				break;
			}
			case InsType::MEMBER:
			case InsType::MEMBER_LV: {
				// 取出栈顶对象
				auto obj{ ost.pop_normal() };
				auto type{ obj->type };
				// 取成员的名称
				auto name{ ls.getString(ins) };
				// 取对象的成员
				if (type->f_member) type->f_member(vm, ins.type == InsType::MEMBER_LV, obj, name);
				else SetError_UnmatchedMember(vm, type, name);
				// 对象解除链接
				obj->unlink();
				if (vm->error()) return IResult<void>();
				break;
			}
			case InsType::INDEX:
			case InsType::INDEX_LV: {
				// 取出栈顶对象
				auto obj{ ost.pop_normal() };
				auto type{ obj->type };
				// 依次取出栈顶索引参数对象并逆向组成参数列表
				auto argc{ static_cast<Size>(ins.get<Size32>()) };
				ObjArgs args{ argc };
				ObjArgsView argsView{ args.data(), argc };
				for (auto i{ argc }; i > 0; --i) args[i - 1] = ost.pop_normal();
				// 取对象索引结果
				if (type->f_index) type->f_index(vm, ins.type == InsType::INDEX_LV, obj, argsView);
				else SetError_UnmatchedIndex(vm, type->v_name, argsView);
				// 参数列表解除链接, 对象解除链接
				for (auto i{ argc }; i > 0; --i) args[i - 1]->unlink();
				obj->unlink();
				if (vm->error()) return IResult<void>();
				break;
			}
			case InsType::CALL: {
				// 函数参数数目
				auto argc{ static_cast<Size>(ins.get<Size32>()) };
				// 依次取出栈顶函数参数对象并逆向组成参数列表
				ObjArgs args{ argc };
				ObjArgsView argsView{ args.data(), argc };
				for (auto i{ argc }; i > 0; --i) args[i - 1] = ost.pop_normal();
				// 取出栈顶对象
				auto obj{ ost.pop_normal() };
				if (obj->type->v_id == TypeId::Function || obj->type->v_id == TypeId::MemberFunction) { // 函数调用
					FunctionObject* fobj{ };
					Object* thisObject{ };
					if (obj->type->v_id == TypeId::Function) fobj = obj_cast<FunctionObject>(obj); // 全局函数
					else { // 成员函数
						auto mfobj{ obj_cast<MemberFunctionObject>(obj) };
						fobj = mfobj->funcObject;
						thisObject = mfobj->thisObject;
					}
					if (fobj->ft == FunctionType::NATIVE) { // 原生函数
						if (fobj->data.native.checkArgs) { // 检验参数
							if (CheckNativeArgs(&fobj->targs, argsView)) fobj->data.native.func(vm, argsView, thisObject);
							else SetError_UnmatchedCall(vm, fobj->name, argsView);
						}
						else fobj->data.native.func(vm, argsView, thisObject);
					}
					else CallFunction(vm, fobj, argsView, thisObject); // 普通函数
				}
				else if (obj->type->v_id == TypeId::Type) { // 类型
					auto prototype{ obj_cast<TypeObject>(obj) };
					if (prototype->f_construct) prototype->f_construct(vm, prototype, argsView); // 调用构造函数
					else SetError_UnmatchedCall(vm, prototype->v_name, argsView);
				}
				else SetError_UnmatchedCall(vm, obj->type->v_name, argsView);
				// 参数列表解除链接, 对象解除链接
				for (auto i{ argc }; i > 0; --i) args[i - 1]->unlink();
				obj->unlink();
				if (vm->error()) return IResult<void>();
				break;
			}
			case InsType::UNPACK: {
				auto obj{ ost.pop_normal() };
				auto type{ obj->type };
				auto count{ static_cast<Size>(ins.get<Size32>()) };
				if (type->f_unpack) type->f_unpack(vm, obj, count);
				else SetError_UnsupportedUnpack(vm, obj->type);
				obj->unlink();
				if (vm->error()) return IResult<void>();
				break;
			}
			case InsType::AS_BOOL: {
				// 取出栈顶对象
				auto obj{ ost.pop_normal() };
				// 转换为逻辑值
				if (obj->type->f_bool) ost.push_link(obj_allocate(vm->getType(TypeId::Bool),
					arg_cast(static_cast<Int64>(obj->type->f_bool(obj)))));
				else SetError_NotBoolean(vm, obj->type);
				obj->unlink();
				if (vm->error()) return IResult<void>();
				break;
			}
			case InsType::OP_SINGLE: {
				auto opt{ static_cast<SOPTType>(ins.get<Byte>()) }; // 运算符
				auto obj{ ost.pop_normal() }; // 操作数
				if (opt == SOPTType::NOT) { // 非运算直接使用逻辑值
					if (obj->type->f_bool) ost.push_link(obj_allocate(vm->getType(TypeId::Bool),
						arg_cast(static_cast<Int64>(!obj->type->f_bool(obj)))));
					else SetError_UnsupportedSOPT(vm, obj->type, opt);
				}
				else {
					if (obj->type->f_sopt_calc) obj->type->f_sopt_calc(vm, obj, opt);
					else SetError_UnsupportedSOPT(vm, obj->type, opt);
				}
				obj->unlink();
				if (vm->error()) return IResult<void>();
				break;
			}
			case InsType::OP_BINARY: {
				auto opt{ static_cast<BOPTType>(ins.get<Byte>()) }; // 运算符
				auto obj2{ ost.pop_normal() }; // 操作数2
				auto obj1{ ost.pop_normal() }; // 操作数1
				switch (opt) {
				case BOPTType::ADD:
				case BOPTType::SUBTRACT:
				case BOPTType::MULTIPLE:
				case BOPTType::DIVIDE:
				case BOPTType::MOD:
				case BOPTType::POWER: {
					if (obj1->type->f_bopt_calc) obj1->type->f_bopt_calc(vm, obj1, obj2, opt);
					else SetError_UnsupportedBOPT(vm, obj1->type, obj2->type, opt);
					break;
				}
				case BOPTType::EQ:
				case BOPTType::NE: {
					if (auto ir{ obj1->type->f_equal(vm, obj1, obj2) })
						ost.push_link(obj_allocate(vm->getType(TypeId::Bool),
							arg_cast(static_cast<Int64>(opt == BOPTType::NE ? !ir.data : ir.data))));
					break;
				}
				default: {
					if (obj1->type->f_compare) {
						if (auto ir{ obj1->type->f_compare(vm, obj1, obj2, opt) })
							ost.push_link(obj_allocate(vm->getType(TypeId::Bool),
								arg_cast(static_cast<Int64>(ir.data))));
					}
					else SetError_UnsupportedBOPT(vm, obj1->type, obj2->type, opt);
					break;
				}
				}
				obj1->unlink();
				obj2->unlink();
				if (vm->error()) return IResult<void>();
				break;
			}
			case InsType::GET_ITER: {
				auto obj{ ost.pop_normal() };
				if (obj->type->f_iter_get) obj->type->f_iter_get(vm, obj);
				else SetError_UnsupportedIterator(vm, obj->type);
				obj->unlink();
				if (vm->error()) return IResult<void>();
				break;
			}
			case InsType::SAVE_ITER: {
				auto iter{ obj_cast<IteratorObject>(ost.top()) };
				auto refType{ iter->ref->type };
				if (refType->f_iter_save) {
					auto refView{ ls.getRef(ins) };
					auto argc{ refView.count() };
					if (refType->f_iter_save(vm, iter, argc)) {
						ObjArgs args{ argc };
						for (auto i{ argc }; i > 0; --i) args[i - 1] = ost.pop_normal();
						RegisterPackVariable(topCall.dom.symbols, ObjArgsView(args.data(), argc), refView);
						for (auto arg : args) arg->unlink();
					}
				}
				else SetError_UnsupportedIterator(vm, refType);
				if (vm->error()) return IResult<void>();
				break;
			}
			case InsType::ADD_ITER: {
				auto iter{ obj_cast<IteratorObject>(ost.top()) };
				auto refType{ iter->ref->type };
				if (refType->f_iter_add) refType->f_iter_add(vm, iter);
				else SetError_UnsupportedIterator(vm, refType);
				if (vm->error()) return IResult<void>();
				break;
			}
			case InsType::JUMP_CHECK_ITER: {
				auto iter{ obj_cast<IteratorObject>(ost.top()) };
				auto refType{ iter->ref->type };
				if (refType->f_iter_check) {
					if (auto ir{ refType->f_iter_check(vm, iter) }) {
						if (!ir.data) {
							ost.pop_unlink();
							topCall.pIns += ins.get<Uint32>() - 1;
						}
					}
				}
				else SetError_UnsupportedIterator(vm, refType);
				if (vm->error()) return IResult<void>();
				break;
			}
			case InsType::JUMP_NE_POP: {
				auto obj{ ost.pop_normal() };
				auto value{ ost.top() };
				if (auto ir{ value->type->f_equal(vm, value, obj) }) {
					if (!ir.data) topCall.pIns += ins.get<Uint32>() - 1;
				}
				obj->unlink();
				if (vm->error()) return IResult<void>();
				break;
			}
			case InsType::JUMP: {
				// 无条件跳转
				topCall.pIns += ins.get<Uint32>() - 1; // for循环结束会指令自增与-1抵消
				break;
			}
			case InsType::JUMP_TRUE:
			case InsType::JUMP_FALSE: {
				// 读取栈顶对象
				auto obj{ ost.top() };
				// 检查是否符合条件
				if (!obj->type->f_bool) return SetError(&SetError_NotBoolean, vm, obj->type);
				auto ret{ obj->type->f_bool(obj) };
				// 决定是否跳转(同或关系)
				// JUMP_TRUE | ret	| isJump
				//     0     |  0   |   1
				//     0     |  1   |   0
				//     1     |  0   |   0
				//     1     |  1   |   1
				if ((ins.type == InsType::JUMP_TRUE) == ret)
					topCall.pIns += ins.get<Uint32>() - 1; // 指令指针偏移量
				break;
			}
			case InsType::JUMP_TRUE_POP:
			case InsType::JUMP_FALSE_POP: {
				// 取出栈顶对象
				auto obj{ ost.pop_normal() };
				// 检查是否符合条件
				if (obj->type->f_bool) {
					auto ret{ obj->type->f_bool(obj) };
					// 决定是否跳转
					if ((ins.type == InsType::JUMP_TRUE_POP) == ret)
						topCall.pIns += ins.get<Uint32>() - 1; // 指令指针偏移量
				}
				else SetError_NotBoolean(vm, obj->type);
				// 对象解除链接
				obj->unlink();
				if (vm->error()) return IResult<void>();
				break;
			}
			case InsType::JUMP_RE: {
				// 无条件跳转
				topCall.pIns -= ins.get<Uint32>() + 1; // for循环结束会指令自增与+1抵消
				break;
			}
			case InsType::RETURN: {
				// 返回值判断
				auto ret{ ost.top() };
				auto matchType{ topCall.retType };
				if (matchType->canImplement(ret->type)) {
					cst.pop();
					continue; // 结束指令块的执行
				}
				else return SetError(&SetError_UnmatchedType, vm, ret->type, matchType);
			}
			case InsType::PRE_IMPORT:
			case InsType::PRE_IMPORT_USING: {
				auto isUsing{ ins.type == InsType::PRE_IMPORT_USING };
				auto refView{ ls.getRef(ins) };
				if (auto findMod{ vm->moduleTree.find(refView) }) {
					if (isUsing) vm->moduleTree.setUsing(findMod);
				}
				else {
					auto modName{ refView.toString<u'.'>() };
					auto modPath{ vm->libPath + util::Path(refView.toString<util::Path::SLASH>() + strings::BYTECODE_NAME) };
					util::ByteArray ba;
					if (platform::Platform_ReadFile(&modPath, &ba)) {
						auto newModule{ vm->moduleTree.add(refView, modName, modPath, isUsing) };
						RunByteCode(vm, newModule, &ba, true);
						if (vm->error()) return IResult<void>();
					}
					else return SetError(&SetError_FileNotExists, vm, modPath.toView());
				}
				break;
			}
			case InsType::PRE_USING: {
				auto refView{ ls.getRef(ins) };
				if (auto findMod{ vm->moduleTree.find(refView) }) vm->moduleTree.setUsing(findMod);
				else return SetError(SetError_UndefinedRef, vm, refView);
				break;
			}
			case InsType::PRE_SOFT_LINK: {
				auto indexs{ ls.getIndexs(ins) };
				auto refView{ ls.getRef(indexs[0ULL]) };
				if (auto obj{ FindRef(vm, refView) }) {
					auto name{ ls.getString(indexs[1ULL]) };
					if (FindSymbolInModule(topMod, name))
						return SetError(&SetError_RedefinedID, vm, name);
					else topMod->dom.links.try_emplace(name, obj);
				}
				else return SetError(SetError_UndefinedRef, vm, refView);
				break;
			}
			case InsType::PRE_NATIVE: {
				auto namePath{ util::Path(ls.getRef(ins).toString<util::Path::SLASH>()) };
				auto dllPath{ (topMod->modulePath.getParent() + namePath).toString() };
				if (!topMod->dllPaths.contains(dllPath)) {
					if (auto handle{ platform::Platform_LoadDll(dllPath) }) {
						topMod->dllPaths.emplace(dllPath);
						if (auto initFunc{ static_cast<DllInitFunc>(
							platform::Platform_GetDllFunction(handle, strings::HONEY_DLL_INIT_NAME)) }) {
							if (initFunc(vm, topMod)) break;
							else return IResult<void>();
						}
					}
					return SetError(&SetError_DllError, vm, dllPath);
				}
				break;
			}
			case InsType::PRE_CONST: {
				auto& constTable{ topMod->dom.consts };
				auto indexs{ ls.getIndexs(ins) };
				auto name{ ls.getString(indexs[0]) };
				if (constTable.has(name)) return SetError(&SetError_RedefinedConst, vm, name);
				topMod->dom.consts.set(name, indexs[1]);
				break;
			}
			case InsType::PRE_GLOBAL: {
				auto refView{ ls.getRef(ins) };
				auto nullType{ vm->getType(TypeId::Null) };
				for (auto strView : refView) {
					StringView name{ strView.data(), strView.size() };
					if (FindSymbolInModule(topMod, name)) return SetError(&SetError_RedefinedID, vm, name);
					topMod->dom.symbols.setSymbol(name, obj_allocate(nullType));
				}
				break;
			}
			case InsType::PRE_FUNCTION:
			case InsType::PRE_LAMBDA: {
				auto setFunc{ true };
				auto& funcView{ ls.getFunction(ins.get<Size32>()) };
				auto name{ ls.getString(funcView.index_name()) };
				if (FindSymbolInModule(topMod, name)) {
					if (ins.type == InsType::PRE_LAMBDA) setFunc = false;
					else return SetError(&SetError_RedefinedID, vm, name);
				}
				if (setFunc) {
					String fullName{ topMod->name };
					fullName += u"::";
					fullName += name;
					if (auto ir{ FetchFunctionView(vm, topMod, funcView, fullName) })
						topMod->dom.symbols.setSymbol(name, ir.data);
					else return IResult<void>();
				}
				break;
			}
			case InsType::PRE_CLASS: {
				auto& clsView{ ls.getClass(ins.get<Size32>()) };
				// 类名称
				auto className{ ls.getString(clsView.index_name) };
				if (FindSymbolInModule(topMod, className))
					return SetError(&SetError_RedefinedID, vm, className);

				TypeClassStruct classStruct; // 类类型结构
				HashSet<StringView> nameFilter; // 名称过滤器
				// 成员变量
				if (!clsView.index_mv.empty()) {
					Index i{ };
					for (auto& varView : clsView.index_mv) {
						// 成员变量类型
						auto refView{ ls.getRef(varView.index_type) };
						auto varTypeObject{ FindRef(vm, refView) };
						if (!varTypeObject)
							return SetError(&SetError_UndefinedRef, vm, refView); // 未定义
						if (varTypeObject->type->v_id != TypeId::Type)
							return SetError(&SetError_NotType, vm, refView); // 不是类型
						auto varType{ obj_cast<TypeObject>(varTypeObject) };
						if (!varType->a_def)
							return SetError(&SetError_IllegalMemberType, vm, varType); // 非法成员类型
						// 成员变量名称
						auto varNames{ ls.getRef(varView.index_names) };
						for (auto strView : varNames) {
							StringView varName{ strView.data(), strView.size() };
							if (nameFilter.contains(varName))
								return SetError(&SetError_RedefinedID, vm, varName); // 重定义
							nameFilter.emplace(varName);
							classStruct.members.try_emplace(varName, varType, i++);
						}
					}
				}

				// 创建类 (因为成员函数中可能使用此类, 需提前定义, 而基类及成员不允许是此类)
				auto fullClassName{ topMod->name };
				fullClassName += u"::";
				fullClassName.append(className);
				auto tobj{ AddObjectPrototype(vm, fullClassName) };
				topMod->dom.symbols.setSymbol(className, tobj);

				// 成员函数
				if (!clsView.index_mf.empty()) {
					// 普通成员函数
					for (auto index_func : clsView.index_mf) {
						auto& funcView{ ls.getFunction(index_func) };
						// 取函数对象名称
						auto funcName{ ls.getString(funcView.index_name()) };
						if (nameFilter.contains(funcName)) { // 重定义
							SetError_RedefinedID(vm, funcName);
							for (auto& iter : classStruct.funcs) iter.second->unlink();
							return IResult<void>();
						}
						nameFilter.emplace(funcName);
						String fullFuncName{ className };
						fullFuncName += u".";
						fullFuncName += funcName;
						if (auto ir{ FetchFunctionView(vm, topMod, funcView, fullFuncName) }) {
							auto fobj{ ir.data };
							fobj->link();
							classStruct.funcs.try_emplace(funcName, fobj);
						}
						else {
							for (auto& iter : classStruct.funcs) iter.second->unlink();
							return IResult<void>();
						}
					}
				}

				// 设置类属性
				SetObjectPrototype(vm, tobj, new TypeClassStruct(freestanding::move(classStruct)));
				break;
			}
			case InsType::PRE_CONCEPT: {
				auto& cptView{ ls.getConcept(ins.get<Size32>()) };
				auto name{ ls.getString(cptView.index_name) };
				if (FindSymbolInModule(topMod, name))
					return SetError(&SetError_RedefinedID, vm, name);
				// 生成概念约束列表
				auto count{ cptView.subView.size() };
				TypeConceptStruct tcs{ count };
				for (Index i{ }; i < count; ++i) {
					auto& sce{ cptView.subView[static_cast<Size32>(i)] };
					auto& ci{ tcs[i] };
					ci.sct = sce.type;
					switch (sce.type) {
					case SubConceptElementType::NOT: break;
					case SubConceptElementType::JUMP_TRUE:
					case SubConceptElementType::JUMP_FALSE: ci.v.offset = static_cast<Index>(sce.arg); break;
					case SubConceptElementType::SET: {
						auto refView{ ls.getRef(sce.arg) };
						auto typeObject{ FindRef(vm, refView) };
						if (!typeObject)
							return SetError(&SetError_UndefinedRef, vm, refView);  // 未定义
						if (typeObject->type->v_id != TypeId::Type)
							return SetError(&SetError_NotType, vm, refView); // 不是类型
						ci.v.type = obj_cast<TypeObject>(typeObject);
						break;
					}
					default: return SetError(&SetError_ByteCodeBroken, vm);
					}
				}

				// 创建概念
				String fullName{ topMod->name };
				fullName += u"::";
				fullName += name;
				auto cobj{ AddInterfacePrototype(vm, fullName, new TypeConceptStruct(freestanding::move(tcs))) };
				topMod->dom.symbols.setSymbol(name, cobj);
				break;
			}
			default: return SetError(&SetError_ByteCodeBroken, vm);
			}
			++topCall.pIns;
		}
		return IResult<void>(true);
	}

	// 运行字节码
	void RunByteCode(VM* vm, Module* mod, util::ByteArray* ba, bool movebc) noexcept {
		if (auto ret{ movebc ? serialize::ReadByteCode(freestanding::move(*ba), mod->bc)
			: serialize::ReadByteCode(*ba, mod->bc) }) {
			if (VerifyByteCode(vm, &mod->bc)) { // 校验字节码
				auto nullType{ vm->getType(TypeId::Null) };
				vm->objectStack.push_link(obj_allocate(nullType));
				vm->callStack.push(mod, nullType, &mod->bc.mainCode, mod->name, nullptr);
				RunCallStack(vm, 0ULL);
				if (vm->ok()) vm->objectStack.pop_unlink();
			}
		}
		else SetError_ByteCodeBroken(vm); // 字节码长度不足MIN_SIZE
	}
}

namespace hy {
	Memory SysCall(const StringView name) noexcept {
		static const HashMap<StringView, Memory> syscallmap {
			{ u"SetError_UnmatchedCall", &SetError_UnmatchedCall },
		};
		if (auto iter{ syscallmap.find(name) }; iter != syscallmap.cend()) return iter->second;
		return nullptr;
	}

	LIB_EXPORT void VMInitialize(VM * vm) noexcept {
		// 1. 内置模块创建
		auto mod{ new Module(static_cast<Index>(-1), strings::BUILTIN_NAME, { }) };
		vm->moduleTree.builtin = mod;

		// 2. 内置类型初始化
		auto prototype{ AddTypePrototype() };
		vm->moduleTree.prototype = prototype;
		prototype->f_class_create(prototype);
		prototype->link();

		auto& builtinTypes{ static_cast<TypeStaticData*>(prototype->v_static)->builtinTypes };
		builtinTypes[static_cast<Token>(prototype->v_id)] = prototype;

		using AptFuncs = TypeObject * (*)(TypeObject*) noexcept;

		struct AptFuncsBind {
			AptFuncs func;
			bool onSymbol;
		}aptFuncsBind[] {
			{ &AddNullPrototype, false },
			{ &AddIntPrototype, true },
			{ &AddFloatPrototype, true },
			{ &AddComplexPrototype, true },
			{ &AddBoolPrototype, true },
			{ &AddLVPrototype, false },
			{ &AddIteratorPrototype, false },
			{ &AddFunctionPrototype, false },
			{ &AddMemberFunctionPrototype, false },
			{ &AddStringPrototype, true },
			{ &AddListPrototype, true },
			{ &AddMapPrototype, true },
			{ &AddVectorPrototype, true },
			{ &AddMatrixPrototype, true },
			{ &AddRangePrototype, true },
			{ &AddArrayPrototype, true },
			{ &AddBinPrototype, true },
			{ &AddHashSetPrototype, true },
		};

		for (auto& aptFunc : aptFuncsBind) {
			auto type{ aptFunc.func(prototype) };
			if (type->f_class_create) type->f_class_create(type);
			type->link();
			builtinTypes[static_cast<Token>(type->v_id)] = type;
			if (aptFunc.onSymbol) mod->dom.symbols.setSymbol(type->v_name, type);
		}

		// 3. 内置概念初始化
		AddBuiltinConcept(vm);

		// 4. 内置函数初始化
		AddBuiltinFunction(vm);
		
		// 5. 系统调用初始化
		vm->syscall = &SysCall;
	}

	LIB_EXPORT void VMClean(VM* vm) noexcept {
		// 1. 清理对象栈残留对象
		vm->objectStack.clear();

		// 2. 清理调用堆栈
		vm->callStack.clear();

		// 3. 逆向清理所有模块
		auto& modTable{ vm->moduleTree.modTable };
		auto& modData{ modTable.modData };
		while (!modData.empty()) {
			auto& mod{ modData.back() };
			// 释放DLL注册表
			for (auto& dllPath : mod.dllPaths) {
				if (auto handle{ platform::Platform_GetDll(dllPath) })
					platform::Platform_FreeDll(handle);
			}
			modData.pop_back();
		}
		modTable.modMap.clear();
		modTable.usingList.clear();
	}

	LIB_EXPORT void VMDestroy(VM * vm) noexcept {
		// 1. VM清理
		VMClean(vm);

		// 2. 清理builtin模块
		delete vm->moduleTree.builtin;
		vm->moduleTree.prototype->unlink();
	}
}