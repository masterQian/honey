#include "../hy.vm.impl.h"

namespace hy {
	struct LVStaticData {
		Vector<LVObject*> pool;
	};

	void f_class_create_lv(TypeObject* type) noexcept {
		type->v_static = new LVStaticData;
	}

	void f_class_delete_lv(TypeObject* type) noexcept {
		auto staticData{ static_cast<LVStaticData*>(type->v_static) };
		for (auto obj : staticData->pool) delete obj;
		delete staticData;
	}

	void f_class_clean_lv(TypeObject* type) noexcept {
		auto staticData{ static_cast<LVStaticData*>(type->v_static) };
		for (auto obj : staticData->pool) delete obj;
		staticData->pool = Vector<LVObject*>();
	}

	Object* f_allocate_lv(TypeObject* type, Memory, Memory) noexcept {
		auto& pool{ static_cast<LVStaticData*>(type->v_static)->pool };
		LVObject* obj;
		if (pool.empty()) obj = new LVObject(type);
		else {
			obj = pool.back();
			pool.pop_back();
		}
		return obj;
	}

	void f_deallocate_lv(Object* obj) noexcept {
		auto& pool{ static_cast<LVStaticData*>(obj->type->v_static)->pool };
		auto lvobj{ obj_cast<LVObject>(obj) };
		auto parent{ lvobj->parent };
		if (lvobj->isData() && parent->type->f_free_lv_data)
			parent->type->f_free_lv_data(lvobj);
		parent->unlink();
		pool.emplace_back(lvobj);
	}
}

namespace hy {
	TypeObject* AddLVPrototype(TypeObject* prototype) noexcept {
		auto type{ new TypeObject(prototype) };
		type->v_id = TypeId::LV;
		type->v_name = u"leftvalue";

		type->v_static = nullptr;
		type->v_cls = nullptr;
		type->v_cps = nullptr;

		type->a_mutable = true;
		type->a_def = false;
		type->a_unused1 = type->a_unused2 = false;

		type->f_class_create = &f_class_create_lv;
		type->f_class_delete = &f_class_delete_lv;
		type->f_class_clean = &f_class_clean_lv;
		type->f_allocate = &f_allocate_lv;
		type->f_deallocate = &f_deallocate_lv;
		type->f_implement = nullptr;

		type->f_float = nullptr;
		type->f_bool = nullptr;
		type->f_string = &f_string_empty;

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