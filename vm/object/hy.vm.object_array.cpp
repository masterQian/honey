#include "../hy.vm.impl.h"

#include <fast_io/fast_io.h>

namespace hy::impl {
	LIB_EXPORT void Array_Push(HVM hvm, ObjArgsView args, Object* thisObject) noexcept {
		auto vm{ vm_cast(hvm) };
		auto aobj{ obj_cast<ArrayObject>(thisObject) };
		auto v{ obj_cast<IntObject>(args[0])->value };
		aobj->data.emplace_back(v);
		vm->objectStack.push_link(aobj);
	}

	LIB_EXPORT void Array_Insert(HVM hvm, ObjArgsView args, Object* thisObject) noexcept {
		auto vm{ vm_cast(hvm) };
		auto aobj{ obj_cast<ArrayObject>(thisObject) };
		auto index{ obj_cast<IntObject>(args[0])->value }, len{ static_cast<Int64>(aobj->data.size() + 1) };
		if (index > 0 && index <= len) {
			auto v{ obj_cast<IntObject>(args[1])->value };
			aobj->data.insert(aobj->data.cbegin() + index - 1, v);
			vm->objectStack.push_link(aobj);
		}
		else SetError_IndexOutOfRange(vm, index, len);
	}

	LIB_EXPORT void Array_Pop(HVM hvm, ObjArgsView, Object* thisObject) noexcept {
		auto vm{ vm_cast(hvm) };
		auto aobj{ obj_cast<ArrayObject>(thisObject) };
		if (aobj->data.empty()) SetError_EmptyContainer(vm, u"array::pop", aobj->type);
		else {
			auto v{ aobj->data.back() };
			aobj->data.pop_back();
			vm->objectStack.push_link(obj_allocate(vm->getType(TypeId::Int), arg_cast(v)));
		}
	}

	LIB_EXPORT void Array_Delete(HVM hvm, ObjArgsView args, Object* thisObject) noexcept {
		auto vm{ vm_cast(hvm) };
		auto aobj{ obj_cast<ArrayObject>(thisObject) };
		auto index{ obj_cast<IntObject>(args[0])->value }, len{ static_cast<Int64>(aobj->data.size()) };
		if (index > 0 && index <= len) {
			auto v{ aobj->data[index - 1] };
			aobj->data.erase(aobj->data.cbegin() + index - 1);
			vm->objectStack.push_link(obj_allocate(vm->getType(TypeId::Int), arg_cast(v)));
		}
		else SetError_IndexOutOfRange(vm, index, len);
	}

	LIB_EXPORT void Array_Clear(HVM hvm, ObjArgsView, Object* thisObject) noexcept {
		auto vm{ vm_cast(hvm) };
		auto aobj{ obj_cast<ArrayObject>(thisObject) };
		aobj->data.clear();
		vm->objectStack.push_link(thisObject);
	}

	LIB_EXPORT void Array_Reverse(HVM hvm, ObjArgsView, Object* thisObject) noexcept {
		auto vm{ vm_cast(hvm) };
		auto aobj{ obj_cast<ArrayObject>(thisObject) };
		std::reverse(aobj->data.begin(), aobj->data.end());
		vm->objectStack.push_link(thisObject);
	}

	LIB_EXPORT void Array_Sort(HVM hvm, ObjArgsView, Object* thisObject) noexcept {
		auto vm{ vm_cast(hvm) };
		auto aobj{ obj_cast<ArrayObject>(thisObject) };
		std::sort(aobj->data.begin(), aobj->data.end());
		vm->objectStack.push_link(thisObject);
	}

	LIB_EXPORT void Array_Contains(HVM hvm, ObjArgsView args, Object* thisObject) noexcept {
		auto vm{ vm_cast(hvm) };
		auto aobj{ obj_cast<ArrayObject>(thisObject) };
		auto& data{ aobj->data };
		auto iobj{ obj_cast<IntObject>(args[0]) };
		auto v{ std::find(data.cbegin(), data.cend(), iobj->value) != data.cend() };
		vm->objectStack.push_link(obj_allocate(vm->getType(TypeId::Bool), arg_cast(static_cast<Int64>(v))));
	}

	LIB_EXPORT void Array_Find(HVM hvm, ObjArgsView args, Object* thisObject) noexcept {
		auto vm{ vm_cast(hvm) };
		auto aobj{ obj_cast<ArrayObject>(thisObject) };
		auto& data{ aobj->data };
		auto iobj{ obj_cast<IntObject>(args[0]) };
		auto ret{ obj_allocate<IntObject>(iobj->type) };
		auto iter{ std::find(data.cbegin(), data.cend(), iobj->value) };
		if (iter == data.cend()) ret->value = 0LL;
		else ret->value = static_cast<Int64>(iter - data.cbegin()) + 1LL;
		vm->objectStack.push_link(ret);
	}

	LIB_EXPORT void Array_Count(HVM hvm, ObjArgsView args, Object* thisObject) noexcept {
		auto vm{ vm_cast(hvm) };
		auto aobj{ obj_cast<ArrayObject>(thisObject) };
		auto& data{ aobj->data };
		auto iobj{ obj_cast<IntObject>(args[0]) };
		auto cnt{ static_cast<Size>(std::count(data.cbegin(), data.cend(), iobj->value)) };
		vm->objectStack.push_link(obj_allocate<IntObject>(iobj->type, arg_cast(cnt)));
	}
}

namespace hy {
	struct ArrayStaticData {
		Vector<ArrayObject*> pool;
		FunctionTable ft;

		ArrayStaticData(TypeObject* type) noexcept {
			auto funcType{ type->__getType(TypeId::Function) };
			auto intType{ type->__getType(TypeId::Int) };

			Object* __any[] { intType, intType, nullptr };
			ObjArgsView empty, it{ __any, 1ULL }, it2{ __any, 2ULL };

			ft.try_emplace(u"push", MakeNative<true, true>(funcType, u"array::push", &impl::Array_Push, it));
			ft.try_emplace(u"insert", MakeNative<true, true>(funcType, u"array::insert", &impl::Array_Insert, it2));
			ft.try_emplace(u"pop", MakeNative<true, true>(funcType, u"array::pop", &impl::Array_Pop, empty));
			ft.try_emplace(u"delete", MakeNative<true, true>(funcType, u"array::delete", &impl::Array_Delete, it));
			ft.try_emplace(u"clear", MakeNative<true, true>(funcType, u"array::clear", &impl::Array_Clear, empty));
			ft.try_emplace(u"reverse", MakeNative<true, true>(funcType, u"array::reverse", &impl::Array_Reverse, empty));
			ft.try_emplace(u"sort", MakeNative<true, true>(funcType, u"array::sort", &impl::Array_Sort, empty));
			ft.try_emplace(u"contains", MakeNative<true, true>(funcType, u"array::contains", &impl::Array_Contains, it));
			ft.try_emplace(u"find", MakeNative<true, true>(funcType, u"array::find", &impl::Array_Find, it));
			ft.try_emplace(u"count", MakeNative<true, true>(funcType, u"array::count", &impl::Array_Count, it));
		}
	};

	void f_class_create_array(TypeObject* type) noexcept {
		type->v_static = new ArrayStaticData(type);
	}

	void f_class_delete_array(TypeObject* type) noexcept {
		auto staticData{ static_cast<ArrayStaticData*>(type->v_static) };
		for (auto obj : staticData->pool) delete obj;
		for (auto& [_, fobj] : staticData->ft) delete fobj;
		delete staticData;
	}

	void f_class_clean_array(TypeObject* type) noexcept {
		auto staticData{ static_cast<ArrayStaticData*>(type->v_static) };
		for (auto obj : staticData->pool) delete obj;
		staticData->pool = Vector<ArrayObject*>();
	}

	// arg1: Int64* 数组数据(可空)
	// arg2: Size 数组长度
	Object* f_allocate_array(TypeObject* type, Memory arg1, Memory arg2) noexcept {
		auto& pool{ static_cast<ArrayStaticData*>(type->v_static)->pool };
		auto data{ arg_recast<Int64*>(arg1) };
		auto size{ arg_recast<Size>(arg2) };
		ArrayObject* obj;
		if (pool.empty()) obj = new ArrayObject(type);
		else {
			obj = pool.back();
			pool.pop_back();
		}
		obj->data.resize(size);
		if (data) freestanding::copy_n(obj->data.data(), data, size);
		return obj;
	}

	void f_deallocate_array(Object* obj) noexcept {
		auto& pool{ static_cast<ArrayStaticData*>(obj->type->v_static)->pool };
		pool.emplace_back(obj_cast<ArrayObject>(obj));
	}

	bool f_bool_array(Object* obj) noexcept {
		return !obj_cast<ArrayObject>(obj)->data.empty();
	}

	IResult<void> f_string_array(HVM, Object* obj, String* str) noexcept {
		fast_io::u16ostring_ref strRef{ str };
		str->push_back(u'[');
		auto aobj{ obj_cast<ArrayObject>(obj) };
		for (auto item : aobj->data) print(strRef, item, u",");
		if (aobj->data.empty()) str->push_back(u']');
		else str->back() = u']';
		return IResult<void>(true);
	}

	IResult<Size> f_hash_array(HVM, Object* obj) noexcept {
		std::hash<Int64> hasher;
		Size hash{ };
		auto aobj{ obj_cast<ArrayObject>(obj) };
		for (auto item : aobj->data) hash ^= hasher(item);
		return IResult<Size>(hash);
	}

	IResult<Size> f_len_array(HVM, Object* obj) noexcept {
		return IResult<Size>(obj_cast<ArrayObject>(obj)->data.size());
	}

	IResult<void> f_member_array(HVM hvm, bool isLV, Object* obj, const StringView member) noexcept {
		auto vm{ vm_cast(hvm) };
		if (!isLV) {
			auto& ft{ static_cast<ArrayStaticData*>(obj->type->v_static)->ft };
			if (auto pFobj{ ft.get(member) }) {
				auto fobj{ obj_allocate(vm->getType(TypeId::MemberFunction), *pFobj, obj) };
				vm->objectStack.push_link(fobj);
				return IResult<void>(true);
			}
			else return SetError(&SetError_UnmatchedMember, vm, obj->type, member);
		}
		else return SetError(&SetError_NotLeftValue, vm, member);
	}

	constexpr auto ARRAY_LVID_ELEM{ 0ULL };

	IResult<void> f_index_array(HVM hvm, bool isLV, Object* obj, ObjArgsView args) noexcept {
		auto vm{ vm_cast(hvm) };
		auto aobj{ obj_cast<ArrayObject>(obj) };
		auto argc{ args.size() };
		if (argc == 1ULL) {
			if (auto arg{ args[0] }; arg->type->v_id == TypeId::Int) {
				auto length{ static_cast<Int64>(aobj->data.size()) };
				auto index{ obj_cast<IntObject>(arg)->value };
				if (index > 0LL && index <= length) {
					auto actualIndex{ static_cast<Index>(index - 1) };
					if (isLV) {
						auto ret{ obj_allocate<LVObject>(vm->getType(TypeId::LV)) };
						ret->parent = obj;
						ret->parent->link();
						ret->setData<ARRAY_LVID_ELEM>(actualIndex);
						vm->objectStack.push_link(ret);
					}
					else vm->objectStack.push_link(obj_allocate(arg->type, arg_cast(aobj->data[actualIndex])));
					return IResult<void>(true);
				}
				else SetError_IndexOutOfRange(vm, index, length);
			}
			else SetError_UnmatchedIndex(vm, obj->type->v_name, args);
		}
		else SetError_UnmatchedIndex(vm, obj->type->v_name, args);
		return IResult<void>();
	}

	IResult<void> f_unpack_array(HVM hvm, Object* obj, Size argc) noexcept {
		auto vm{ vm_cast(hvm) };
		auto aobj{ obj_cast<ArrayObject>(obj) };
		auto size{ aobj->data.size() };
		if (argc <= size) {
			auto type{ vm->getType(TypeId::Int) };
			for (Size i{ }; i < argc; ++i)
				vm->objectStack.push_link(obj_allocate(type, arg_cast(aobj->data[i])));
			return IResult<void>(true);
		}
		else return SetError(&SetError_UnmatchedUnpack, vm, size, argc);
	}

	IResult<void> f_construct_array(HVM hvm, TypeObject* type, ObjArgsView args) noexcept {
		auto vm{ vm_cast(hvm) };
		auto argc{ args.size() };
		if (argc == 0ULL) {
			vm->objectStack.push_link(obj_allocate(type));
			return IResult<void>(true);
		}
		else if (argc == 1ULL) {
			if (auto arg{ args[0] }; arg->type->v_id == TypeId::Vector) {
				auto& vec{ obj_cast<VectorObject>(arg)->data };
				auto size{ vec.size() };
				auto aobj{ obj_allocate<ArrayObject>(type, nullptr, arg_cast(vec.size())) };
				auto data{ aobj->data.data() };
				for (Index i{ }; i < size; ++i) data[i] = static_cast<Int64>(vec[i]);
				vm->objectStack.push_link(aobj);
				return IResult<void>(true);
			}
		}
		return SetError(&SetError_UnmatchedCall, vm, type->v_name, args);
	}

	Object* f_full_copy_array(Object* obj) noexcept {
		auto aobj{ obj_cast<ArrayObject>(obj) };
		return obj_allocate(obj->type, arg_cast(aobj->data.data()), arg_cast(aobj->data.size()));
	}

	IResult<void> f_calcassign_array(HVM hvm, LVObject* lv, Object* obj2, AssignType opt) noexcept {
		auto vm{ vm_cast(hvm) };
		auto aobj1{ lv->getAddressObject<ArrayObject>() };
		if (opt == AssignType::ADD_ASSIGN && obj2->type->v_id == TypeId::Array) {
			auto aobj2{ obj_cast<ArrayObject>(obj2) };
			auto size1{ aobj1->data.size() }, size2{ aobj2->data.size() };
			aobj1->data.resize(size1 + size2);
			freestanding::copy_n(aobj1->data.data() + size1, aobj2->data.data(), size2);
			return IResult<void>(true);
		}
		else return SetError(&SetError_IncompatibleCalcAssign, vm, aobj1->type, obj2->type, opt);
	}

	IResult<void> f_assign_lv_data_array(HVM hvm, LVObject* lv, Object* obj) noexcept {
		auto vm{ vm_cast(hvm) };
		if (obj->type->v_id == TypeId::Int) {
			lv->getParent<ArrayObject>()->data[lv->getData<0ULL, Index>()] = obj_cast<IntObject>(obj)->value;
			return IResult<void>(true);
		}
		return SetError(&SetError_IncompatibleAssign, vm, vm->getType(TypeId::Int), obj->type);
	}

	IResult<void> f_calcassign_lv_data_array(HVM hvm, LVObject* lv, Object* obj, AssignType opt) noexcept {
		auto vm{ vm_cast(hvm) };
		if (obj->type->v_id == TypeId::Int) {
			auto value{ obj_cast<IntObject>(obj)->value };
			auto& ref{ lv->getParent<ArrayObject>()->data[lv->getData<0ULL, Index>()] };
			switch (opt) {
			case AssignType::ADD_ASSIGN: ref += value; break;
			case AssignType::SUBTRACT_ASSIGN: ref -= value; break;
			case AssignType::MULTIPLE_ASSIGN: ref *= value; break;
			case AssignType::DIVIDE_ASSIGN: ref = value == 0LL ? 9223372036854775807LL : ref / value; break;
			case AssignType::MOD_ASSIGN: ref = value == 0LL ? 0LL : ref % value; break;
			}
			return IResult<void>(true);
		}
		return SetError(&SetError_IncompatibleCalcAssign, vm, vm->getType(TypeId::Int), obj->type, opt);
	}

	IResult<void> f_bopt_calc_array(HVM hvm, Object* obj1, Object* obj2, BOPTType opt) noexcept {
		auto vm{ vm_cast(hvm) };
		if (opt == BOPTType::ADD && obj2->type->v_id == TypeId::Array) {
			auto aobj{ obj_allocate<ArrayObject>(obj1->type) };
			auto& data1{ obj_cast<ArrayObject>(obj1)->data };
			auto& data2{ obj_cast<ArrayObject>(obj2)->data };
			auto& data{ aobj->data };
			aobj->data.resize(data1.size() + data2.size());
			freestanding::copy_n(data.data(), data1.data(), data1.size());
			freestanding::copy_n(data.data() + data1.size(), data2.data(), data2.size());
			vm->objectStack.push_link(aobj);
			return IResult<void>(true);
		}
		return SetError(&SetError_UnsupportedBOPT, vm, obj1->type, obj2->type, opt);
	}

	IResult<bool> f_equal_array(HVM, Object* obj1, Object* obj2) noexcept {
		if (obj2->type->v_id != TypeId::Array) return IResult<bool>(false);
		auto& data1{ obj_cast<ArrayObject>(obj1)->data };
		auto& data2{ obj_cast<ArrayObject>(obj2)->data };
		return IResult<bool>(data1.size() == data2.size() &&
			freestanding::compare(data1.data(), data2.data(), data1.size() * sizeof(Int64)) == 0);
	}

	constexpr auto ARRAY_ITER_ELEM{ 0ULL };

	IResult<void> f_iter_get_array(HVM hvm, Object* obj) noexcept {
		auto vm{ vm_cast(hvm) };
		auto iter{ obj_allocate<IteratorObject>(vm->getType(TypeId::Iterator)) };
		iter->ref = obj;
		iter->ref->link();
		iter->setData<ARRAY_ITER_ELEM>(0ULL);
		vm->objectStack.push_link(iter);
		return IResult<void>(true);
	}

	IResult<void> f_iter_save_array(HVM hvm, IteratorObject* iter, Size argc) noexcept {
		auto vm{ vm_cast(hvm) };
		auto aobj{ iter->getReference<ArrayObject>() };
		if (argc == 1ULL || argc == 2ULL) {
			auto pos{ iter->getData<0ULL, Index>() };
			auto intType{ vm->getType(TypeId::Int) };
			vm->objectStack.push_link(obj_allocate(intType, arg_cast(aobj->data[pos])));
			if (argc == 2ULL) vm->objectStack.push_link(obj_allocate(intType,
				arg_cast(static_cast<Int64>(pos + 1ULL))));
			return IResult<void>(true);
		}
		return SetError(&SetError_UnmatchedUnpack, vm, 1ULL, argc);
	}

	IResult<void> f_iter_add_array(HVM, IteratorObject* iter) noexcept {
		iter->resetData<0ULL>(iter->getData<0ULL, Index>() + 1ULL);
		return IResult<void>(true);
	}

	IResult<bool> f_iter_check_array(HVM, IteratorObject* iter) noexcept {
		return IResult<bool>(iter->getData<0ULL, Index>() != iter->getReference<ArrayObject>()->data.size());
	}
}

namespace hy {
	TypeObject* AddArrayPrototype(TypeObject* prototype) noexcept {
		auto type{ new TypeObject(prototype) };
		type->v_id = TypeId::Array;
		type->v_name = u"array";

		type->v_static = nullptr;
		type->v_cls = nullptr;
		type->v_cps = nullptr;

		type->a_mutable = true;
		type->a_def = true;
		type->a_unused1 = type->a_unused2 = false;

		type->f_class_create = &f_class_create_array;
		type->f_class_delete = &f_class_delete_array;
		type->f_class_clean = &f_class_clean_array;
		type->f_allocate = &f_allocate_array;
		type->f_deallocate = &f_deallocate_array;
		type->f_implement = nullptr;

		type->f_float = nullptr;
		type->f_bool = &f_bool_array;
		type->f_string = &f_string_array;

		type->f_hash = &f_hash_array;
		type->f_len = &f_len_array;
		type->f_member = &f_member_array;
		type->f_index = &f_index_array;
		type->f_unpack = &f_unpack_array;

		type->f_construct = &f_construct_array;
		type->f_full_copy = &f_full_copy_array;
		type->f_copy = &f_full_copy_array;
		type->f_write = nullptr;
		type->f_scan = nullptr;
		type->f_calcassign = &f_calcassign_array;

		type->f_assign_lv_data = &f_assign_lv_data_array;
		type->f_calcassign_lv_data = &f_calcassign_lv_data_array;
		type->f_free_lv_data = nullptr;

		type->f_sopt_calc = nullptr;
		type->f_bopt_calc = &f_bopt_calc_array;
		type->f_equal = &f_equal_array;
		type->f_compare = nullptr;

		type->f_iter_get = &f_iter_get_array;
		type->f_iter_save = &f_iter_save_array;
		type->f_iter_add = &f_iter_add_array;
		type->f_iter_check = &f_iter_check_array;
		type->f_iter_free = nullptr;

		return type;
	}
}