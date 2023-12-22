#include "../hy.vm.impl.h"

namespace hy {
	struct NullStaticData {
		NullObject instance;

		NullStaticData(TypeObject* type) noexcept : instance{ type } {
			instance.link();
		}
	};

	void f_class_create_null(TypeObject* type) noexcept {
		type->v_static = new NullStaticData{ type };
	}

	void f_class_delete_null(TypeObject* type) noexcept {
		delete static_cast<NullStaticData*>(type->v_static);
	}

	Object* f_allocate_null(TypeObject* type, Memory, Memory) noexcept {
		return &static_cast<NullStaticData*>(type->v_static)->instance;
	}

	IResult<void> f_string_null(HVM, Object*, String* str) noexcept {
		str->append(u"null");
		return IResult<void>(true);
	}
}

namespace hy {
	TypeObject* AddNullPrototype(TypeObject* prototype) noexcept {
		auto type{ new TypeObject(prototype) };
		type->v_id = TypeId::Null;
		type->v_name = u"null";

		type->v_static = nullptr;
		type->v_cls = nullptr;
		type->v_cps = nullptr;

		type->a_mutable = false;
		type->a_def = true;
		type->a_unused1 = type->a_unused2 = false;

		type->f_class_create = &f_class_create_null;
		type->f_class_delete = &f_class_delete_null;
		type->f_class_clean = nullptr;
		type->f_allocate = &f_allocate_null;
		type->f_deallocate = &f_deallocate_empty;
		type->f_implement = nullptr;

		type->f_float = nullptr;
		type->f_bool = nullptr;
		type->f_string = &f_string_null;

		type->f_hash = &f_hash_fixed<0ULL>;
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