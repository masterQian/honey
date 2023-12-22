#include "../hy.vm.impl.h"

namespace hy {
	void f_class_create_type(TypeObject* type) noexcept {
		type->v_static = new TypeStaticData;
	}

	void f_class_delete_type(TypeObject* type) noexcept {
		auto staticData{ static_cast<TypeStaticData*>(type->v_static) };
		auto& builtinTypes{ staticData->builtinTypes };
		for (auto i{ TypeIdBuiltinCount - 1ULL }; i; --i) builtinTypes[i]->unlink();
		delete staticData;
	}

	void f_deallocate_type(Object* obj) noexcept {
		auto type{ obj_cast<TypeObject>(obj) };
		if (type->v_cls) {
			for (auto& [key, fobj] : type->v_cls->funcs) fobj->unlink();
			delete type->v_cls;
		}
		if (type->v_cps) delete type->v_cps;
		if (type->f_class_delete) type->f_class_delete(type);
		delete type;
	}

	IResult<void> f_string_type(HVM, Object* obj, String* str) noexcept {
		str->append(obj_cast<TypeObject>(obj)->v_name);
		return IResult<void>(true);
	}
}

namespace hy {
	TypeObject* AddTypePrototype() noexcept {
		auto type{ new TypeObject(nullptr) };
		type->type = type;
		type->v_id = TypeId::Type;
		type->v_name = u"type";

		type->v_static = nullptr;
		type->v_cls = nullptr;
		type->v_cps = nullptr;

		type->a_mutable = false;
		type->a_def = false;
		type->a_unused1 = type->a_unused2 = false;

		type->f_class_create = &f_class_create_type;
		type->f_class_delete = &f_class_delete_type;
		type->f_class_clean = nullptr;
		type->f_allocate = &f_allocate_default<TypeObject>;
		type->f_deallocate = &f_deallocate_type;
		type->f_implement = nullptr;

		type->f_float = nullptr;
		type->f_bool = nullptr;
		type->f_string = &f_string_type;

		type->f_hash = nullptr;
		type->f_len = nullptr;
		type->f_member = nullptr;
		type->f_index = nullptr;
		type->f_unpack = nullptr;

		type->f_construct = nullptr;
		type->f_full_copy = nullptr;
		type->f_copy = nullptr;
		type->f_write = nullptr;
		type->f_scan = nullptr;
		type->f_calcassign = nullptr;

		type->f_assign_lv_data = nullptr;
		type->f_calcassign_lv_data = nullptr;
		type->f_free_lv_data = nullptr;

		type->f_sopt_calc = nullptr;
		type->f_bopt_calc = nullptr;
		type->f_equal = &f_equal_ref;
		type->f_compare = nullptr;

		type->f_iter_get = nullptr;
		type->f_iter_save = nullptr;
		type->f_iter_add = nullptr;
		type->f_iter_check = nullptr;
		type->f_iter_free = nullptr;

		return type;
	}
}