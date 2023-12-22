#include "../hy.vm.impl.h"

namespace hy {
	IResult<void> f_string_function(HVM, Object* obj, String* str) noexcept {
		str->append(obj_cast<FunctionObject>(obj)->name);
		return IResult<void>(true);
	}
}

namespace hy {
	TypeObject* AddFunctionPrototype(TypeObject* prototype) noexcept {
		auto type{ new TypeObject(prototype) };
		type->v_id = TypeId::Function;
		type->v_name = u"function";

		type->v_static = nullptr;
		type->v_cls = nullptr;
		type->v_cps = nullptr;

		type->a_mutable = false;
		type->a_def = false;
		type->a_unused1 = type->a_unused2 = false;

		type->f_class_create = nullptr;
		type->f_class_delete = nullptr;
		type->f_class_clean = nullptr;
		type->f_allocate = &f_allocate_default<FunctionObject>;
		type->f_deallocate = &f_deallocate_default<FunctionObject>;
		type->f_implement = nullptr;

		type->f_float = nullptr;
		type->f_bool = nullptr;
		type->f_string = &f_string_function;

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