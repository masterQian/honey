#include "../hy.vm.impl.h"

namespace hy {
	struct IteratorStaticData {
		Vector<IteratorObject*> pool;
	};

	void f_class_create_iterator(TypeObject* type) noexcept {
		type->v_static = new IteratorStaticData;
	}

	void f_class_delete_iterator(TypeObject* type) noexcept {
		auto staticData{ static_cast<IteratorStaticData*>(type->v_static) };
		for (auto obj : staticData->pool) delete obj;
		delete staticData;
	}

	void f_class_clean_iterator(TypeObject* type) noexcept {
		auto staticData{ static_cast<IteratorStaticData*>(type->v_static) };
		for (auto obj : staticData->pool) delete obj;
		staticData->pool = Vector<IteratorObject*>();
	}

	Object* f_allocate_iterator(TypeObject* type, Memory, Memory) noexcept {
		auto& pool{ static_cast<IteratorStaticData*>(type->v_static)->pool };
		IteratorObject* obj;
		if (pool.empty()) obj = new IteratorObject(type);
		else {
			obj = pool.back();
			pool.pop_back();
		}
		return obj;
	}

	void f_deallocate_iterator(Object* obj) noexcept {
		auto& pool{ static_cast<IteratorStaticData*>(obj->type->v_static)->pool };
		auto iter{ obj_cast<IteratorObject>(obj) };
		auto ref{ iter->ref };
		ref->unlink();
		if (ref->type->f_iter_free) ref->type->f_iter_free(iter);
		pool.emplace_back(iter);
	}
}

namespace hy {
	TypeObject* AddIteratorPrototype(TypeObject* prototype) noexcept {
		auto type{ new TypeObject(prototype) };
		type->v_id = TypeId::Iterator;
		type->v_name = u"iterator";

		type->v_static = nullptr;
		type->v_cls = nullptr;
		type->v_cps = nullptr;

		type->a_mutable = true;
		type->a_def = false;
		type->a_unused1 = type->a_unused2 = false;

		type->f_class_create = &f_class_create_iterator;
		type->f_class_delete = &f_class_delete_iterator;
		type->f_class_clean = &f_class_clean_iterator;
		type->f_allocate = &f_allocate_iterator;
		type->f_deallocate = &f_deallocate_iterator;
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