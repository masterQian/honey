#include "hy.vm.impl.h"

#include <fast_io/fast_io.h>

namespace hy {
	void Builtin_Write(HVM hvm, ObjArgsView args, Object*) noexcept {
		auto vm{ vm_cast(hvm) };
		if (args.size() > 1ULL && args[0]->type->f_write) {
			auto obj{ args[0] };
			++args.mView;
			--args.mSize;
			if (obj->type->f_write(vm, obj, args)) vm->objectStack.push_link(obj);
		}
		else SetError_UnmatchedCall(vm, u"write", args);
	}

	template<bool newLine>
	void Builtin_Print(HVM hvm, ObjArgsView args, Object*) noexcept {
		auto vm{ vm_cast(hvm) };
		if (vm->stdDevice.charOutputFunc) {
			String totalString;
			if (impl::String_Concat(vm, &totalString, args, newLine)) {
				vm->stdDevice.charOutputFunc(totalString);
				vm->objectStack.push_link(obj_allocate(vm->getType(TypeId::Null)));
			}
		}
		else SetError_IODeviceError(vm);
	}

	void Builtin_Scan(HVM hvm, ObjArgsView args, Object*) noexcept {
		auto vm{ vm_cast(hvm) };
		if (vm->stdDevice.charInputFunc) {
			String scanString;
			if (vm->stdDevice.charInputFunc(&scanString)) {
				auto begin{ scanString.data() };
				auto count{ scanString.size() };
				auto lobj{ obj_allocate<ListObject>(vm->getType(TypeId::List)) };
				lobj->link();
				lobj->objects.reserve(args.size());
				for (auto arg : args) {
					if (arg->type->v_id != TypeId::Type) {
						SetError_UnmatchedCall(vm, u"scan", args);
						lobj->unlink();
						return;
					}
					auto type{ obj_cast<TypeObject>(arg) };
					if (!type->f_scan) {
						SetError_UnsupportedScan(vm, type);
						lobj->unlink();
						return;
					}
					if (auto ir{ type->f_scan(vm, type, StringView{ begin, count }) }) {
						count -= ir.data;
						begin += ir.data;
						lobj->objects.emplace_back(vm->objectStack.pop_normal());
					}
					else {
						lobj->unlink();
						return;
					}
				}
				vm->objectStack.push_normal(lobj);
				return;
			}
		}
		SetError_IODeviceError(vm);
	}

	void Builtin_Prototype(HVM hvm, ObjArgsView args, Object*) noexcept {
		auto vm{ vm_cast(hvm) };
		vm->objectStack.push_link(args[0]->type);
	}

	void Builtin_Address(HVM hvm, ObjArgsView args, Object*) noexcept {
		auto vm{ vm_cast(hvm) };
		auto obj{ obj_allocate(vm->getType(TypeId::Int), args[0]) };
		vm->objectStack.push_link(obj);
	}

	void Builtin_Implements(HVM hvm, ObjArgsView args, Object*) noexcept {
		auto vm{ vm_cast(hvm) };
		auto t1{ args[0]->type }, t2{ obj_cast<TypeObject>(args[1]) };
		auto v{ t2->f_implement ? t2->f_implement(t2, t1) : t2 == t1 };
		auto obj{ obj_allocate(vm->getType(TypeId::Bool), arg_cast(v ? 1LL : 0LL)) };
		vm->objectStack.push_link(obj);
	}

	void Builtin_Hash(HVM hvm, ObjArgsView args, Object*) noexcept {
		auto vm{ vm_cast(hvm) };
		if (auto arg{ args[0] }; arg->type->f_hash) {
			if (auto ir{ arg->type->f_hash(vm, arg) }) {
				auto obj{ obj_allocate(vm->getType(TypeId::Int), arg_cast(ir.data)) };
				vm->objectStack.push_link(obj);
			}
		}
		else SetError_UnmatchedCall(vm, u"hash", args);
	}

	void Builtin_Len(HVM hvm, ObjArgsView args, Object*) noexcept {
		auto vm{ vm_cast(hvm) };
		if (auto arg{ args[0] }; arg->type->f_len) {
			if (auto ir{ arg->type->f_len(vm, arg) }) {
				auto obj{ obj_allocate(vm->getType(TypeId::Int), arg_cast(ir.data)) };
				vm->objectStack.push_link(obj);
			}
		}
		else SetError_UnmatchedCall(vm, u"len", args);
	}

	void Builtin_Copy(HVM hvm, ObjArgsView args, Object*) noexcept {
		auto vm{ vm_cast(hvm) };
		auto arg{ args[0] };
		auto obj{ arg->type->f_full_copy ? arg->type->f_full_copy(arg) : arg };
		vm->objectStack.push_link(obj);
	}

	void Builtin_Args(HVM hvm, ObjArgsView, Object*) noexcept {
		auto vm{ vm_cast(hvm) };
		auto& args_extra{ vm->callStack.top().args_extra };
		auto ret{ obj_allocate<ListObject>(vm->getType(TypeId::List)) };
		auto argc{ args_extra.size() };
		if (argc) {
			ret->objects.resize(argc);
			freestanding::copy_n(ret->objects.data(), args_extra.data(), argc);
			for (auto arg : ret->objects) arg->link();
		}
		vm->objectStack.push_link(ret);
	}

	void Builtin_Assert(HVM hvm, ObjArgsView args, Object*) noexcept {
		auto vm{ vm_cast(hvm) };
		for (Index i{ }, l{ args.size() }; i < l; ++i) {
			auto obj{ args[i] };
			if (obj->type->f_bool) {
				if (!obj->type->f_bool(obj)) {
					SetError_AssertFalse(vm, i + 1ULL);
					break;
				}
			}
			else {
				SetError_UnmatchedCall(vm, u"assert", args);
				break;
			}
		}
		vm->objectStack.push_link(obj_allocate(vm->getType(TypeId::Null)));
	}
}

namespace hy {
	void AddBuiltinFunction(VM* vm) noexcept {
		auto& st{ vm->moduleTree.builtin->dom.symbols };
		auto funcType{ vm->getType(TypeId::Function) };
		auto typeType{ funcType->type };

		Object* __any[] { nullptr, typeType };
		ObjArgsView empty, any1{ __any, 1ULL }, any1_tt{ __any, 2ULL };

		st.setSymbol(u"write", MakeNative(funcType, u"write", &Builtin_Write, empty));
		st.setSymbol(u"print", MakeNative(funcType, u"print", &Builtin_Print<false>, empty));
		st.setSymbol(u"println", MakeNative(funcType, u"println", &Builtin_Print<true>, empty));
		st.setSymbol(u"scan", MakeNative(funcType, u"scan", &Builtin_Scan, empty));
		st.setSymbol(u"prototype", MakeNative<true>(funcType, u"prototype", &Builtin_Prototype, any1));
		st.setSymbol(u"address", MakeNative<true>(funcType, u"address", &Builtin_Address, any1));
		st.setSymbol(u"implements", MakeNative<true>(funcType, u"implements", &Builtin_Implements, any1_tt));
		st.setSymbol(u"hash", MakeNative<true>(funcType, u"hash", &Builtin_Hash, any1));
		st.setSymbol(u"len", MakeNative<true>(funcType, u"len", &Builtin_Len, any1));
		st.setSymbol(u"copy", MakeNative<true>(funcType, u"copy", &Builtin_Copy, any1));
		st.setSymbol(u"args", MakeNative<true>(funcType, u"args", &Builtin_Args, empty));
		st.setSymbol(u"assert", MakeNative(funcType, u"assert", &Builtin_Assert, empty));
	}
}