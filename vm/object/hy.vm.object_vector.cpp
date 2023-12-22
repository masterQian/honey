#include "../hy.vm.impl.h"

#include <fast_io/fast_io.h>

namespace hy {
	struct VectorStaticData {
		Vector<VectorObject*> pool;
	};

	void f_class_create_vector(TypeObject* type) noexcept {
		type->v_static = new VectorStaticData;
	}

	void f_class_delete_vector(TypeObject* type) noexcept {
		auto staticData{ static_cast<VectorStaticData*>(type->v_static) };
		for (auto obj : staticData->pool) delete obj;
		delete staticData;
	}

	void f_class_clean_vector(TypeObject* type) noexcept {
		auto staticData{ static_cast<VectorStaticData*>(type->v_static) };
		for (auto obj : staticData->pool) delete obj;
		staticData->pool = Vector<VectorObject*>();
	}

	// arg1: Float64* 向量数据(可空)
	// arg2: Size 向量长度
	Object* f_allocate_vector(TypeObject* type, Memory arg1, Memory arg2) noexcept {
		auto& pool{ static_cast<VectorStaticData*>(type->v_static)->pool };
		auto data{ arg_recast<Float64*>(arg1) };
		auto size{ arg_recast<Size>(arg2) };
		VectorObject* obj;
		if (pool.empty()) {
			obj = new VectorObject(type);
			if (size) obj->data.resize(size);
		}
		else {
			obj = pool.back();
			pool.pop_back();
			if (size) {
				if (obj->data.mSize >= size) obj->data.mSize = size;
				else {
					obj->data.release();
					obj->data.resize(size);
				}
			}
			else obj->data.release();
		}
		if (data) freestanding::copy_n(obj->data.mData, data, size);
		return obj;
	}

	void f_deallocate_vector(Object* obj) noexcept {
		auto& pool{ static_cast<VectorStaticData*>(obj->type->v_static)->pool };
		pool.emplace_back(obj_cast<VectorObject>(obj));
	}

	bool f_bool_vector(Object* obj) noexcept {
		return !obj_cast<VectorObject>(obj)->data.empty();
	}

	IResult<void> f_string_vector(HVM, Object* obj, String* str) noexcept {
		fast_io::u16ostring_ref strRef{ str };
		str->push_back(u'[');
		auto vobj{ obj_cast<VectorObject>(obj) };
		for (auto item : vobj->data) print(strRef, item, u",");
		if (vobj->data.empty()) str->push_back(u']');
		else str->back() = u']';
		return IResult<void>(true);
	}

	IResult<Size> f_hash_vector(HVM, Object* obj) noexcept {
		std::hash<Float64> hasher;
		auto hash{ 0ULL };
		auto vobj{ obj_cast<VectorObject>(obj) };
		for (auto item : vobj->data) hash ^= hasher(item);
		return IResult<Size>(hash);
	}

	IResult<Size> f_len_vector(HVM, Object* obj) noexcept {
		return IResult<Size>(obj_cast<VectorObject>(obj)->data.size());
	}

	constexpr auto VECTOR_LVID_ELEM{ 0ULL };

	IResult<void> f_index_vector(HVM hvm, bool isLV, Object* obj, ObjArgsView args) noexcept {
		auto vm{ vm_cast(hvm) };
		auto vobj{ obj_cast<VectorObject>(obj) };
		if (auto argc{ args.size() }; argc == 1ULL) {
			if (auto arg{ args[0] }; arg->type->v_id == TypeId::Int) {
				auto length{ static_cast<Int64>(vobj->data.size()) };
				auto index{ obj_cast<IntObject>(arg)->value };
				if (index > 0LL && index <= length) {
					auto actualIndex{ static_cast<Index>(index - 1) };
					if (isLV) {
						auto ret{ obj_allocate<LVObject>(vm->getType(TypeId::LV)) };
						ret->parent = obj;
						ret->parent->link();
						ret->setData<VECTOR_LVID_ELEM>(actualIndex);
						vm->objectStack.push_link(ret);
					}
					else vm->objectStack.push_link(obj_allocate(vm->getType(TypeId::Float), arg_cast(vobj->data[actualIndex])));
					return IResult<void>(true);
				}
				else SetError_IndexOutOfRange(vm, index, length);
			}
			else SetError_UnmatchedIndex(vm, obj->type->v_name, args);
		}
		else SetError_UnmatchedIndex(vm, obj->type->v_name, args);
		return IResult<void>();
	}

	IResult<void> f_unpack_vector(HVM hvm, Object* obj, Size argc) noexcept {
		auto vm{ vm_cast(hvm) };
		auto vobj{ obj_cast<VectorObject>(obj) };
		if (auto size{ vobj->data.size() }; argc <= size) {
			auto type{ vm->getType(TypeId::Float) };
			for (Index i{ }; i < argc; ++i)
				vm->objectStack.push_link(obj_allocate(type, arg_cast(vobj->data[i])));
			return IResult<void>(true);
		}
		else return SetError(&SetError_UnmatchedUnpack, vm, size, argc);
	}

	IResult<void> f_construct_vector(HVM hvm, TypeObject* type, ObjArgsView args) noexcept {
		auto vm{ vm_cast(hvm) };
		if (auto argc{ args.size() }; argc == 0ULL) {
			vm->objectStack.push_link(obj_allocate(type));
			return IResult<void>(true);
		}
		return SetError(&SetError_UnmatchedCall, vm, type->v_name, args);
	}

	Object* f_full_copy_vector(Object* obj) noexcept {
		auto vobj{ obj_cast<VectorObject>(obj) };
		return obj_allocate(obj->type, arg_cast(vobj->data.data()), arg_cast(vobj->data.size()));
	}

	IResult<void> f_calcassign_vector(HVM hvm, LVObject* lv, Object* obj2, AssignType opt) noexcept {
		auto vm{ vm_cast(hvm) };
		auto vobj1{ lv->getAddressObject<VectorObject>() };
		if (opt == AssignType::MOD_ASSIGN || obj2->type->v_id != TypeId::Vector)
			return SetError(&SetError_IncompatibleCalcAssign, vm, vobj1->type, obj2->type, opt);
		auto vobj2{ obj_cast<VectorObject>(obj2) };
		auto size1{ vobj1->data.size() };
		auto size2{ vobj2->data.size() };
		if (size1 != size2) return SetError(&SetError_UnmatchedLen, vm, size1, size2);
		else {
			switch (opt) {
			case AssignType::ADD_ASSIGN: {
				for (Size i{ }; i < size1; ++i) vobj1->data[i] += vobj2->data[i];
				break;
			}
			case AssignType::SUBTRACT_ASSIGN: {
				for (Size i{ }; i < size1; ++i) vobj1->data[i] -= vobj2->data[i];
				break;
			}
			case AssignType::MULTIPLE_ASSIGN: {
				for (Size i{ }; i < size1; ++i) vobj1->data[i] *= vobj2->data[i];
				break;
			}
			case AssignType::DIVIDE_ASSIGN: {
				for (Size i{ }; i < size1; ++i) vobj1->data[i] /= vobj2->data[i];
				break;
			}
			}
			return IResult<void>(true);
		}
	}

	IResult<void> f_assign_lv_data_vector(HVM hvm, LVObject* lv, Object* obj) noexcept {
		auto vm{ vm_cast(hvm) };
		if (obj->type->f_float) {
			lv->getParent<VectorObject>()->data[lv->getData<0ULL, Index>()] = obj->type->f_float(obj);
			return IResult<void>(true);
		}
		return SetError(&SetError_IncompatibleAssign, vm, vm->getType(TypeId::Float), obj->type);
	}

	IResult<void> f_calcassign_lv_data_vector(HVM hvm, LVObject* lv, Object* obj, AssignType opt) noexcept {
		auto vm{ vm_cast(hvm) };
		if (obj->type->f_float) {
			auto value{ obj->type->f_float(obj) };
			auto& ref{ lv->getParent<VectorObject>()->data[lv->getData<0ULL, Index>()] };
			switch (opt) {
			case AssignType::ADD_ASSIGN: ref += value; break;
			case AssignType::SUBTRACT_ASSIGN: ref -= value; break;
			case AssignType::MULTIPLE_ASSIGN: ref *= value; break;
			case AssignType::DIVIDE_ASSIGN: ref /= value; break;
			}
			return IResult<void>(true);
		}
		return SetError(&SetError_IncompatibleCalcAssign, vm, vm->getType(TypeId::Float), obj->type, opt);
	}

	IResult<void> f_sopt_calc_vector(HVM hvm, Object* obj, SOPTType) noexcept {
		auto vm{ vm_cast(hvm) };
		auto vobj{ obj_cast<VectorObject>(obj) };
		auto ret{ obj_allocate<VectorObject>(obj->type, arg_cast(vobj->data.data()), arg_cast(vobj->data.size())) };
		for (auto& elem : ret->data) elem = -elem;
		vm->objectStack.push_link(ret);
		return IResult<void>(true);
	}

	IResult<void> f_bopt_calc_vector(HVM hvm, Object* obj1, Object* obj2, BOPTType opt) noexcept {
		auto vm{ vm_cast(hvm) };
		if (opt == BOPTType::MOD || obj2->type->v_id != TypeId::Vector)
			return SetError(&SetError_UnsupportedBOPT, vm, obj1->type, obj2->type, opt);
		auto vobj1{ obj_cast<VectorObject>(obj1) };
		auto vobj2{ obj_cast<VectorObject>(obj2) };
		auto size1{ vobj1->data.size() };
		auto size2{ vobj2->data.size() };
		if (size1 != size2) return SetError(&SetError_UnmatchedLen, vm, size1, size2);
		auto ret{ obj_allocate<VectorObject>(obj1->type, arg_cast(vobj1->data.data()), arg_cast(size1)) };
		switch (opt) {
		case BOPTType::ADD: {
			for (Size i{ }; i < size1; ++i) ret->data[i] += vobj2->data[i];
			break;
		}
		case BOPTType::SUBTRACT: {
			for (Size i{ }; i < size1; ++i) ret->data[i] -= vobj2->data[i];
			break;
		}
		case BOPTType::MULTIPLE: {
			for (Size i{ }; i < size1; ++i) ret->data[i] *= vobj2->data[i];
			break;
		}
		case BOPTType::DIVIDE: {
			for (Size i{ }; i < size1; ++i) ret->data[i] /= vobj2->data[i];
			break;
		}
		case BOPTType::POWER: {
			for (Size i{ }; i < size1; ++i) ret->data[i] = ::pow(ret->data[i], vobj2->data[i]);
			break;
		}
		}
		vm->objectStack.push_link(ret);
		return IResult<void>(true);
	}

	IResult<bool> f_equal_vector(HVM, Object* obj1, Object* obj2) noexcept {
		if (obj2->type->v_id != TypeId::Vector) return IResult<bool>(false);
		auto& data1{ obj_cast<VectorObject>(obj1)->data };
		auto& data2{ obj_cast<VectorObject>(obj2)->data };
		return IResult<bool>(data1.size() == data2.size() &&
			freestanding::compare(data1.data(), data2.data(), data1.size() * sizeof(Float64)) == 0);
	}

	constexpr auto VECTOR_ITER_ELEM{ 0ULL };

	IResult<void> f_iter_get_vector(HVM hvm, Object* obj) noexcept {
		auto vm{ vm_cast(hvm) };
		auto iter{ obj_allocate<IteratorObject>(vm->getType(TypeId::Iterator)) };
		iter->ref = obj;
		iter->ref->link();
		iter->setData<VECTOR_ITER_ELEM>(0ULL);
		vm->objectStack.push_link(iter);
		return IResult<void>(true);
	}

	IResult<void> f_iter_save_vector(HVM hvm, IteratorObject* iter, Size argc) noexcept {
		auto vm{ vm_cast(hvm) };
		auto vobj{ iter->getReference<VectorObject>() };
		if (argc == 1ULL || argc == 2ULL) {
			auto pos{ iter->getData<0ULL, Index>() };
			vm->objectStack.push_link(obj_allocate(vm->getType(TypeId::Float), arg_cast(vobj->data[pos])));
			if (argc == 2ULL) vm->objectStack.push_link(obj_allocate(vm->getType(TypeId::Int),
				arg_cast(static_cast<Int64>(pos + 1ULL))));
			return IResult<void>(true);
		}
		return SetError(&SetError_UnmatchedUnpack, vm, 1ULL, argc);
	}

	IResult<void> f_iter_add_vector(HVM, IteratorObject* iter) noexcept {
		iter->resetData<0ULL, Index>((iter->getData<0ULL, Index>() + 1ULL));
		return IResult<void>(true);
	}

	IResult<bool> f_iter_check_vector(HVM, IteratorObject* iter) noexcept {
		return IResult<bool>(iter->getData<0ULL, Index>() != iter->getReference<VectorObject>()->data.size());
	}
}

namespace hy {
	TypeObject* AddVectorPrototype(TypeObject* prototype) noexcept {
		auto type{ new TypeObject(prototype) };
		type->v_id = TypeId::Vector;
		type->v_name = u"vector";

		type->v_static = nullptr;
		type->v_cls = nullptr;
		type->v_cps = nullptr;

		type->a_mutable = true;
		type->a_def = true;
		type->a_unused1 = type->a_unused2 = false;

		type->f_class_create = &f_class_create_vector;
		type->f_class_delete = &f_class_delete_vector;
		type->f_class_clean = &f_class_clean_vector;
		type->f_allocate = &f_allocate_vector;
		type->f_deallocate = &f_deallocate_vector;
		type->f_implement = nullptr;

		type->f_float = nullptr;
		type->f_bool = &f_bool_vector;
		type->f_string = &f_string_vector;

		type->f_hash = &f_hash_vector;
		type->f_len = &f_len_vector;
		type->f_member = nullptr;
		type->f_index = &f_index_vector;
		type->f_unpack = &f_unpack_vector;

		type->f_construct = &f_construct_vector;
		type->f_full_copy = &f_full_copy_vector;
		type->f_copy = &f_full_copy_vector;
		type->f_write = nullptr;
		type->f_scan = nullptr;
		type->f_calcassign = &f_calcassign_vector;

		type->f_assign_lv_data = &f_assign_lv_data_vector;
		type->f_calcassign_lv_data = &f_calcassign_lv_data_vector;
		type->f_free_lv_data = nullptr;

		type->f_bopt_calc = &f_bopt_calc_vector;
		type->f_sopt_calc = &f_sopt_calc_vector;
		type->f_equal = &f_equal_vector;
		type->f_compare = nullptr;

		type->f_iter_get = &f_iter_get_vector;
		type->f_iter_save = &f_iter_save_vector;
		type->f_iter_add = &f_iter_add_vector;
		type->f_iter_check = &f_iter_check_vector;
		type->f_iter_free = nullptr;

		return type;
	}
}