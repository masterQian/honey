#include "../hy.vm.impl.h"

namespace hy {
	struct BoolStaticData {
		BoolObject bTrue;
		BoolObject bFalse;

		BoolStaticData(TypeObject* type) noexcept : bTrue(type), bFalse(type) {
			bTrue.value = true;
			bTrue.link();
			bFalse.value = false;
			bFalse.link();
		}
	};

	void f_class_create_bool(TypeObject* type) noexcept {
		type->v_static = new BoolStaticData(type);
	}

	void f_class_delete_bool(TypeObject* type) noexcept {
		delete static_cast<BoolStaticData*>(type->v_static);
	}

	// arg: Int64 真值
	Object* f_allocate_bool(TypeObject* type, Memory arg, Memory) noexcept {
		auto staticData{ static_cast<BoolStaticData*>(type->v_static) };
		return static_cast<bool>(arg) ? &staticData->bTrue : &staticData->bFalse;
	}

	bool f_bool_bool(Object* obj) noexcept {
		return obj_cast<BoolObject>(obj)->value;
	}

	IResult<void> f_string_bool(HVM, Object* obj, String* str) noexcept {
		str->append(obj_cast<BoolObject>(obj)->value ? u"true" : u"false");
		return IResult<void>(true);
	}

	IResult<Size> f_hash_bool(HVM, Object* obj) noexcept {
		return IResult<Size>(obj_cast<BoolObject>(obj)->value ? 1ULL : 0ULL);
	}

	IResult<void> f_construct_bool(HVM hvm, TypeObject* type, ObjArgsView args) noexcept {
		auto vm{ vm_cast(hvm) };
		auto staticData{ static_cast<BoolStaticData*>(type->v_static) };
		if (auto argc{ args.size() }; argc == 0ULL) {
			vm->objectStack.push_link(&staticData->bFalse);
			return IResult<void>(true);
		}
		else if (argc == 1ULL) {
			if (auto arg{ args[0] }; arg->type->f_bool) {
				vm->objectStack.push_link(arg->type->f_bool(arg) ? &staticData->bTrue : &staticData->bFalse);
				return IResult<void>(true);
			}
		}
		return SetError(&SetError_UnmatchedCall, vm, type->v_name, args);
	}

	IResult<Size> f_scan_bool(HVM hvm, TypeObject* type, const StringView str) noexcept {
		auto vm{ vm_cast(hvm) };
		auto staticData{ static_cast<BoolStaticData*>(type->v_static) };
		auto start{ str.data() }, p{ start };
		while (freestanding::cvt::isblank(*p)) ++p;
		if (p[0] == u't' && p[1] == u'r' && p[2] == u'u' && p[3] == u'e') {
			vm->objectStack.push_link(&staticData->bTrue);
			return IResult<Size>(p + 4 - start);
		}
		else if (p[0] == u'f' && p[1] == u'a' && p[2] == u'l' && p[3] == u's' && p[4] == u'e') {
			vm->objectStack.push_link(&staticData->bFalse);
			return IResult<Size>(p + 5 - start);
		}
		return SetError<Size>(&SetError_ScanError, vm, type);
	}

	IResult<bool> f_equal_bool(HVM, Object* obj1, Object* obj2) noexcept {
		if(obj2->type->v_id == TypeId::Bool) return IResult<bool>(obj1 == obj2);
		return IResult<bool>(false);
	}
}

namespace hy {
	TypeObject* AddBoolPrototype(TypeObject* prototype) noexcept {
		auto type{ new TypeObject(prototype) };
		type->v_id = TypeId::Bool;
		type->v_name = u"bool";

		type->v_static = nullptr;
		type->v_cls = nullptr;
		type->v_cps = nullptr;

		type->a_mutable = false;
		type->a_def = true;
		type->a_unused1 = type->a_unused2 = false;

		type->f_class_create = &f_class_create_bool;
		type->f_class_delete = &f_class_delete_bool;
		type->f_class_clean = nullptr;
		type->f_allocate = &f_allocate_bool;
		type->f_deallocate = &f_deallocate_empty;
		type->f_implement = nullptr;

		type->f_float = nullptr;
		type->f_bool = &f_bool_bool;
		type->f_string = &f_string_bool;

		type->f_hash = &f_hash_bool;
		type->f_len = nullptr;
		type->f_member = nullptr;
		type->f_index = nullptr;
		type->f_unpack = nullptr;

		type->f_construct = &f_construct_bool;
		type->f_full_copy = nullptr;
		type->f_copy = nullptr;
		type->f_write = nullptr;
		type->f_scan = &f_scan_bool;
		type->f_calcassign = nullptr;

		type->f_assign_lv_data = nullptr;
		type->f_calcassign_lv_data = nullptr;
		type->f_free_lv_data = nullptr;

		type->f_sopt_calc = nullptr;
		type->f_bopt_calc = nullptr;
		type->f_equal = &f_equal_bool;
		type->f_compare = nullptr;

		type->f_iter_get = nullptr;
		type->f_iter_save = nullptr;
		type->f_iter_add = nullptr;
		type->f_iter_check = nullptr;
		type->f_iter_free = nullptr;

		return type;
	}
}