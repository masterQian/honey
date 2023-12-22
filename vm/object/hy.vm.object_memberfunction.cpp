#include "../hy.vm.impl.h"

namespace hy {
	struct MemberFunctionStaticData {
		Vector<MemberFunctionObject*> pool;
	};

	void f_class_create_memberfunction(TypeObject* type) noexcept {
		type->v_static = new MemberFunctionStaticData;
	}

	void f_class_delete_memberfunction(TypeObject* type) noexcept {
		auto staticData{ static_cast<MemberFunctionStaticData*>(type->v_static) };
		for (auto obj : staticData->pool) delete obj;
		delete staticData;
	}

	void f_class_clean_memberfunction(TypeObject* type) noexcept {
		auto staticData{ static_cast<MemberFunctionStaticData*>(type->v_static) };
		for (auto obj : staticData->pool) delete obj;
		staticData->pool = Vector<MemberFunctionObject*>();
	}

	// arg1: FunctionObject* 函数对象
	// arg2: Object* this指针
	Object* f_allocate_memberfunction(TypeObject* type, Memory arg1, Memory arg2) noexcept {
		auto& pool{ static_cast<MemberFunctionStaticData*>(type->v_static)->pool };
		MemberFunctionObject* obj;
		if (pool.empty()) obj = new MemberFunctionObject(type);
		else {
			obj = pool.back();
			pool.pop_back();
		}
		obj->funcObject = arg_recast<FunctionObject*>(arg1);
		obj->thisObject = arg_recast<Object*>(arg2);
		if (obj->thisObject) obj->thisObject->link();
		return obj;
	}

	void f_deallocate_memberfunction(Object* obj) noexcept {
		auto& pool{ static_cast<MemberFunctionStaticData*>(obj->type->v_static)->pool };
		auto* mfobj{ obj_cast<MemberFunctionObject>(obj) };
		mfobj->thisObject->unlink();
		pool.emplace_back(mfobj);
	}

	IResult<void> f_string_memberfunction(HVM, Object* obj, String* str) noexcept {
		str->append(obj_cast<MemberFunctionObject>(obj)->funcObject->name);
		return IResult<void>(true);
	}
}

namespace hy {
	TypeObject* AddMemberFunctionPrototype(TypeObject* prototype) noexcept {
		auto type{ new TypeObject(prototype) };
		type->v_id = TypeId::MemberFunction;
		type->v_name = u"memberfunction";

		type->v_static = nullptr;
		type->v_cls = nullptr;
		type->v_cps = nullptr;

		type->a_mutable = true;
		type->a_def = false;
		type->a_unused1 = type->a_unused2 = false;

		type->f_class_create = &f_class_create_memberfunction;
		type->f_class_delete = &f_class_delete_memberfunction;
		type->f_class_clean = &f_class_clean_memberfunction;
		type->f_allocate = &f_allocate_memberfunction;
		type->f_deallocate = &f_deallocate_memberfunction;
		type->f_implement = nullptr;

		type->f_float = nullptr;
		type->f_bool = nullptr;
		type->f_string = &f_string_memberfunction;

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