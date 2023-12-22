#include "../hy.vm.impl.h"

namespace hy::impl {
	LIB_EXPORT void List_Push(HVM hvm, ObjArgsView args, Object* thisObject) noexcept {
		auto vm{ vm_cast(hvm) };
		auto lobj{ obj_cast<ListObject>(thisObject) };
		args[0]->link();
		lobj->objects.emplace_back(args[0]);
		vm->objectStack.push_link(lobj);
	}

	LIB_EXPORT void List_Insert(HVM hvm, ObjArgsView args, Object* thisObject) noexcept {
		auto vm{ vm_cast(hvm) };
		auto lobj{ obj_cast<ListObject>(thisObject) };
		auto index{ obj_cast<IntObject>(args[0])->value };
		auto len{ static_cast<Int64>(lobj->objects.size() + 1) };
		if (index > 0 && index <= len) {
			args[1]->link();
			lobj->objects.insert(lobj->objects.cbegin() + index - 1, args[1]);
			vm->objectStack.push_link(lobj);
		}
		else SetError_IndexOutOfRange(vm, index, len);
	}

	LIB_EXPORT void List_Pop(HVM hvm, ObjArgsView, Object* thisObject) noexcept {
		auto vm{ vm_cast(hvm) };
		auto lobj{ obj_cast<ListObject>(thisObject) };
		if (lobj->objects.empty()) SetError_EmptyContainer(vm, u"list::pop", lobj->type);
		else {
			auto obj{ lobj->objects.back() };
			lobj->objects.pop_back();
			vm->objectStack.push_normal(obj);
		}
	}

	LIB_EXPORT void List_Delete(HVM hvm, ObjArgsView args, Object* thisObject) noexcept {
		auto vm{ vm_cast(hvm) };
		auto lobj{ obj_cast<ListObject>(thisObject) };
		auto index{ obj_cast<IntObject>(args[0])->value };
		auto len{ static_cast<Int64>(lobj->objects.size()) };
		if (index > 0 && index <= len) {
			auto obj{ lobj->objects[index - 1] };
			lobj->objects.erase(lobj->objects.cbegin() + index - 1);
			vm->objectStack.push_normal(obj);
		}
		else SetError_IndexOutOfRange(vm, index, len);
	}

	LIB_EXPORT void List_Clear(HVM hvm, ObjArgsView, Object* thisObject) noexcept {
		auto vm{ vm_cast(hvm) };
		auto lobj{ obj_cast<ListObject>(thisObject) };
		for (auto obj : lobj->objects) obj->unlink();
		lobj->objects.clear();
		vm->objectStack.push_link(thisObject);
	}

	LIB_EXPORT void List_Reverse(HVM hvm, ObjArgsView, Object* thisObject) noexcept {
		auto vm{ vm_cast(hvm) };
		auto lobj{ obj_cast<ListObject>(thisObject) };
		std::reverse(lobj->objects.begin(), lobj->objects.end());
		vm->objectStack.push_link(thisObject);
	}
}

namespace hy {
	struct ListStaticData {
		Vector<ListObject*> pool;
		FunctionTable ft;

		ListStaticData(TypeObject* type) noexcept {
			auto funcType{ type->__getType(TypeId::Function) };
			auto intType{ type->__getType(TypeId::Int) };

			Object* __any[] { intType, nullptr };
			ObjArgsView empty, any1{ __any + 1, 1ULL }, it_any1{ __any, 2ULL }, it{ __any, 1ULL };

			ft.try_emplace(u"push", MakeNative<true, true>(funcType, u"list::push", &impl::List_Push, any1));
			ft.try_emplace(u"insert", MakeNative<true, true>(funcType, u"list::insert", &impl::List_Insert, it_any1));
			ft.try_emplace(u"pop", MakeNative<true, true>(funcType, u"list::pop", &impl::List_Pop, empty));
			ft.try_emplace(u"delete", MakeNative<true, true>(funcType, u"list::delete", &impl::List_Delete, it));
			ft.try_emplace(u"clear", MakeNative<true, true>(funcType, u"list::clear", &impl::List_Clear, empty));
			ft.try_emplace(u"reverse", MakeNative<true, true>(funcType, u"list::reverse", &impl::List_Reverse, empty));
		}
	};

	void f_class_create_list(TypeObject* type) noexcept {
		type->v_static = new ListStaticData(type);
	}

	void f_class_delete_list(TypeObject* type) noexcept {
		auto staticData{ static_cast<ListStaticData*>(type->v_static) };
		for (auto obj : staticData->pool) delete obj;
		// ft中的FunctionObject均为native无需unlink释放
		for (auto& [_, fobj] : staticData->ft) delete fobj;
		delete staticData;
	}

	void f_class_clean_list(TypeObject* type) noexcept {
		auto staticData{ static_cast<ListStaticData*>(type->v_static) };
		for (auto obj : staticData->pool) delete obj;
		staticData->pool = Vector<ListObject*>();
	}

	Object* f_allocate_list(TypeObject* type, Memory, Memory) noexcept {
		auto& pool{ static_cast<ListStaticData*>(type->v_static)->pool };
		ListObject* obj;
		if (pool.empty()) obj = new ListObject(type);
		else {
			obj = pool.back();
			pool.pop_back();
			obj->objects.clear();
		}
		return obj;
	}

	void f_deallocate_list(Object* obj) noexcept {
		auto& pool{ static_cast<ListStaticData*>(obj->type->v_static)->pool };
		auto lobj{ obj_cast<ListObject>(obj) };
		for (auto item : lobj->objects) item->unlink();
		pool.emplace_back(lobj);
	}

	bool f_bool_list(Object* obj) noexcept {
		return !obj_cast<ListObject>(obj)->objects.empty();
	}

	IResult<void> f_string_list(HVM hvm, Object* obj, String* str) noexcept {
		auto vm{ vm_cast(hvm) };
		String tmp;
		tmp.push_back(u'{');
		auto lobj{ obj_cast<ListObject>(obj) };
		for (auto item : lobj->objects) {
			if (item == obj) tmp += u"..."; // 防止自引用
			else if (!item->type->f_string(vm, item, &tmp)) return IResult<void>();
			tmp.push_back(u',');
		}
		if (lobj->objects.empty()) tmp.push_back(u'}');
		else tmp.back() = u'}';
		str->append(tmp);
		return IResult<void>(true);
	}

	IResult<Size> f_len_list(HVM, Object* obj) noexcept {
		return IResult<Size>(obj_cast<ListObject>(obj)->objects.size());
	}

	IResult<void> f_member_list(HVM hvm, bool isLV, Object* obj, const StringView member) noexcept {
		auto vm{ vm_cast(hvm) };
		if (!isLV) {
			auto& ft{ static_cast<ListStaticData*>(obj->type->v_static)->ft };
			if (auto pFobj{ ft.get(member) }) {
				auto fobj{ obj_allocate(vm->getType(TypeId::MemberFunction), *pFobj, obj) };
				vm->objectStack.push_link(fobj);
				return IResult<void>(true);
			}
			else return SetError(&SetError_UnmatchedMember, vm, obj->type, member);
		}
		else return SetError(&SetError_NotLeftValue, vm, member);
	}

	constexpr auto LIST_LVID_SLICE{ 0ULL };

	IResult<void> f_index_list(HVM hvm, bool isLV, Object* obj, ObjArgsView args) noexcept {
		auto vm{ vm_cast(hvm) };
		auto lobj{ obj_cast<ListObject>(obj) };
		if (auto argc{ args.size() }; argc == 1ULL) {
			if (auto arg{ args[0] }; arg->type->v_id == TypeId::Int) {
				auto length{ static_cast<Int64>(lobj->objects.size()) };
				auto index{ obj_cast<IntObject>(arg)->value };
				if (index > 0LL && index <= length) {
					auto actualIndex{ static_cast<Index>(index - 1) };
					if (isLV) {
						auto lv{ obj_allocate<LVObject>(vm->getType(TypeId::LV)) };
						lv->parent = obj;
						lv->parent->link();
						lv->lvType = LVType::ADDRESS;
						lv->storage.address = &lobj->objects[actualIndex];
						vm->objectStack.push_link(lv);
					}
					else vm->objectStack.push_link(lobj->objects[actualIndex]);
					return IResult<void>(true);
				}
				return SetError(&SetError_IndexOutOfRange, vm, index, length);
			}
		}
		else if (argc == 2ULL) {
			if (auto arg1{ args[0] }, arg2{ args[1] };
				arg1->type->v_id == TypeId::Int && arg2->type->v_id == TypeId::Int) {
				auto left{ obj_cast<IntObject>(arg1)->value };
				auto right{ obj_cast<IntObject>(arg2)->value };
				auto length{ static_cast<Int64>(lobj->objects.size()) };
				if (left < 0LL || left > length)
					return SetError(&SetError_IndexOutOfRange, vm, left, length);
				if (right < 0LL || right > length)
					return SetError(&SetError_IndexOutOfRange, vm, right, length);
				Index begin{ }, end{ };
				if (length) {
					if (!left) left = 1LL;
					if (!right) right = length;
					if (left > right) return SetError(&SetError_IndexOutOfRange, vm, left, right);
					begin = static_cast<Index>(left - 1LL);
					end = static_cast<Index>(right);
				}
				else begin = end = 0ULL;
				if (isLV) {
					auto lv{ obj_allocate<LVObject>(vm->getType(TypeId::LV)) };
					lv->parent = obj;
					lv->parent->link();
					lv->setData<LIST_LVID_SLICE>(begin, end);
					vm->objectStack.push_link(lv);
				}
				else {
					auto newList{ obj_allocate<ListObject>(lobj->type) };
					newList->objects.reserve(end - begin);
					newList->objects.assign(lobj->objects.cbegin() + begin, lobj->objects.cbegin() + end);
					for (auto item : newList->objects) item->link();
					vm->objectStack.push_link(newList);
				}
				return IResult<void>(true);
			}
		}
		SetError_UnmatchedIndex(vm, obj->type->v_name, args);
		return IResult<void>();
	}

	IResult<void> f_unpack_list(HVM hvm, Object* obj, Size argc) noexcept {
		auto vm{ vm_cast(hvm) };
		auto lobj{ obj_cast<ListObject>(obj) };
		if (auto size{ lobj->objects.size() }; argc <= size) {
			for (Size i{ }; i < argc; ++i) vm->objectStack.push_link(lobj->objects[i]);
			return IResult<void>(true);
		}
		else return SetError(&SetError_UnmatchedUnpack, vm, size, argc);
	}

	IResult<void> f_construct_list(HVM hvm, TypeObject* type, ObjArgsView args) noexcept {
		auto vm{ vm_cast(hvm) };
		if (auto argc{ args.size() }; argc == 0ULL) {
			vm->objectStack.push_link(obj_allocate(type));
			return IResult<void>(true);
		}
		else if (argc == 1ULL) {
			auto arg{ args[0] };
			ListObject* lobj{ };
			if (arg->type->v_id == TypeId::Vector) {
				lobj = obj_allocate<ListObject>(type);
				auto& data{ obj_cast<VectorObject>(arg)->data };
				if (!data.empty()) {
					auto floatType{ vm->getType(TypeId::Float) };
					lobj->objects.reserve(data.size());
					Object* item{ };
					for (auto v : data) {
						item = obj_allocate(floatType, arg_cast(v));
						item->link();
						lobj->objects.emplace_back(item);
					}
				}
			}
			else if (arg->type->v_id == TypeId::Range) {
				auto intType{ vm->getType(TypeId::Int) };
				lobj = obj_allocate<ListObject>(type);
				auto robj{ obj_cast<RangeObject>(arg) };
				auto start{ robj->start }, step{ robj->step }, end{ robj->end };
				auto size{ static_cast<Size>((end - start) / step) + 1ULL };
				lobj->objects.reserve(size);
				Object* item{ };
				if (step > 0) {
					for (; start <= end; start += step) {
						item = obj_allocate(intType, arg_cast(start));
						item->link();
						lobj->objects.emplace_back(item);
					}
				}
				else {
					for (; start >= end; start += step) {
						item = obj_allocate(intType, arg_cast(start));
						item->link();
						lobj->objects.emplace_back(item);
					}
				}
			}
			else if (arg->type->v_id == TypeId::Array) {
				lobj = obj_allocate<ListObject>(type);
				auto& data{ obj_cast<ArrayObject>(arg)->data };
				if (!data.empty()) {
					auto intType{ vm->getType(TypeId::Int) };
					lobj->objects.reserve(data.size());
					Object* item{ };
					for (auto v : data) {
						item = obj_allocate(intType, arg_cast(v));
						item->link();
						lobj->objects.emplace_back(item);
					}
				}
			}
			else return SetError(&SetError_UnmatchedCall, vm, type->v_name, args);
			vm->objectStack.push_link(lobj);
			return IResult<void>(true);
		}
		return SetError(&SetError_UnmatchedCall, vm, type->v_name, args);
	}

	Object* f_full_copy_list(Object* obj) noexcept {
		auto lobj{ obj_cast<ListObject>(obj) };
		auto clone{ obj_allocate<ListObject>(obj->type) };
		clone->objects = lobj->objects;
		for (auto& item : clone->objects) {
			if (item == obj) item = clone; // 防止自引用
			else if (item->type->f_full_copy) item = item->type->f_full_copy(item);
			item->link();
		}
		return clone;
	}

	IResult<void> f_write_list(HVM, Object* obj, ObjArgsView args) noexcept {
		auto lobj{ obj_cast<ListObject>(obj) };
		lobj->objects.reserve(lobj->objects.size() + args.size());
		lobj->objects.insert(lobj->objects.end(), args.cbegin(), args.cend());
		for (auto arg : args) arg->link();
		return IResult<void>(true);
	}

	IResult<void> f_calcassign_list(HVM hvm, LVObject* lv, Object* obj2, AssignType opt) noexcept {
		auto vm{ vm_cast(hvm) };
		auto lobj1{ lv->getAddressObject<ListObject>() };
		if (opt == AssignType::ADD_ASSIGN && obj2->type->v_id == TypeId::List) {
			auto lobj2{ obj_cast<ListObject>(obj2) };
			for (auto item : lobj2->objects) item->link();
			lobj1->objects.reserve(lobj1->objects.size() + lobj2->objects.size());
			lobj1->objects.insert(lobj1->objects.end(), lobj2->objects.cbegin(), lobj2->objects.cend());
			return IResult<void>(true);
		}
		else return SetError(&SetError_IncompatibleCalcAssign, vm, lobj1->type, obj2->type, opt);
	}

	IResult<void> f_assign_lv_data_list(HVM hvm, LVObject* lv, Object* obj) noexcept {
		auto vm{ vm_cast(hvm) };
		if (obj->type->v_id == TypeId::List) {
			auto lobj1{ lv->getParent<ListObject>() };
			auto lobj2{ obj_cast<ListObject>(obj) };
			auto& data1{ lobj1->objects };
			auto& data2{ lobj2->objects };
			auto begin{ lv->getData<0ULL, Index>() };
			auto end{ lv->getData<1ULL, Index>() };
			if (lobj1 == lobj2) { // 同列表
				auto rawSize{ data1.size() };
				data1.resize(rawSize * 2 + begin - end);
				for (auto i{ 0ULL }; i < begin; ++i) data1[i]->link();
				for (auto i{ end }; i < rawSize; ++i) data1[i]->link();
				auto pb{ data1.data() };
				freestanding::copy_n(pb + begin + rawSize, pb + end, rawSize - end);
				freestanding::copy_n(pb + begin, pb, rawSize);
			}
			else {
				for (auto item : data2) item->link();
				for (auto i{ begin }; i < end; ++i) data1[i]->unlink();
				data1.erase(data1.cbegin() + begin, data1.cbegin() + end);
				data1.insert(data1.cbegin() + begin, data2.cbegin(), data2.cend());
			}
			return IResult<void>(true);
		}
		return SetError(&SetError_IncompatibleAssign, vm, vm->getType(TypeId::List), obj->type);
	}

	IResult<void> f_bopt_calc_list(HVM hvm, Object* obj1, Object* obj2, BOPTType opt) noexcept {
		auto vm{ vm_cast(hvm) };
		if (opt == BOPTType::ADD && obj2->type->v_id == TypeId::List) {
			auto lobj1{ obj_cast<ListObject>(obj1) };
			auto lobj2{ obj_cast<ListObject>(obj2) };
			auto lobj{ obj_allocate<ListObject>(obj1->type) };
			auto count1{ lobj1->objects.size() };
			auto count2{ lobj2->objects.size() };
			auto count{ count1 + count2 };
			lobj->objects.resize(count);
			auto data1{ lobj1->objects.data() };
			auto data2{ lobj2->objects.data() };
			auto data{ lobj->objects.data() };
			freestanding::copy_n(data, data1, count1);
			freestanding::copy_n(data + count1, data2, count2);
			for (auto item : lobj->objects) item->link();
			vm->objectStack.push_link(lobj);
			return IResult<void>(true);
		}
		return SetError(&SetError_UnsupportedBOPT, vm, obj1->type, obj2->type, opt);
	}

	IResult<bool> f_equal_list(HVM hvm, Object* obj1, Object* obj2) noexcept {
		if (obj2->type->v_id != TypeId::List) return IResult<bool>(false);
		auto& data1{ obj_cast<ListObject>(obj1)->objects };
		auto& data2{ obj_cast<ListObject>(obj2)->objects };
		if (data1.size() != data2.size()) return IResult<bool>(false);
		for (Index i{ }, l = data1.size(); i < l; ++i) {
			if (auto ir{ data1[i]->type->f_equal(hvm, data1[i], data2[i]) }) {
				if (!ir.data) return ir;
			}
			else return ir;
		}
		return IResult<bool>(true);
	}

	constexpr auto LIST_ITER_ELEM{ 0ULL };

	IResult<void> f_iter_get_list(HVM hvm, Object* obj) noexcept {
		auto vm{ vm_cast(hvm) };
		auto iter{ obj_allocate<IteratorObject>(vm->getType(TypeId::Iterator)) };
		iter->ref = obj;
		iter->ref->link();
		iter->setData<LIST_ITER_ELEM>(0ULL);
		vm->objectStack.push_link(iter);
		return IResult<void>(true);
	}

	IResult<void> f_iter_save_list(HVM hvm, IteratorObject* iter, Size argc) noexcept {
		auto vm{ vm_cast(hvm) };
		auto lobj{ iter->getReference<ListObject>() };
		if (argc == 1ULL || argc == 2ULL) {
			auto pos{ iter->getData<0ULL, Index>() };
			vm->objectStack.push_link(lobj->objects[pos]);
			if (argc == 2ULL) vm->objectStack.push_link(obj_allocate(vm->getType(TypeId::Int),
				arg_cast(static_cast<Int64>(pos + 1ULL))));
			return IResult<void>(true);
		}
		return SetError(&SetError_UnmatchedUnpack, vm, 1ULL, argc);
	}

	IResult<void> f_iter_add_list(HVM, IteratorObject* iter) noexcept {
		iter->resetData<0ULL>(iter->getData<0ULL, Index>() + 1ULL);
		return IResult<void>(true);
	}

	IResult<bool> f_iter_check_list(HVM, IteratorObject* iter) noexcept {
		return IResult<bool>(iter->getData<0ULL, Index>() != iter->getReference<ListObject>()->objects.size());
	}
}

namespace hy {
	TypeObject* AddListPrototype(TypeObject* prototype) noexcept {
		auto type{ new TypeObject(prototype) };
		type->v_id = TypeId::List;
		type->v_name = u"list";

		type->v_static = nullptr;
		type->v_cls = nullptr;
		type->v_cps = nullptr;

		type->a_mutable = true;
		type->a_def = true;
		type->a_unused1 = type->a_unused2 = false;

		type->f_class_create = &f_class_create_list;
		type->f_class_delete = &f_class_delete_list;
		type->f_class_clean = &f_class_clean_list;
		type->f_allocate = &f_allocate_list;
		type->f_deallocate = &f_deallocate_list;
		type->f_implement = nullptr;

		type->f_float = nullptr;
		type->f_bool = &f_bool_list;
		type->f_string = &f_string_list;

		type->f_hash = nullptr;
		type->f_len = &f_len_list;
		type->f_member = &f_member_list;
		type->f_index = &f_index_list;
		type->f_unpack = &f_unpack_list;

		type->f_construct = &f_construct_list;
		type->f_full_copy = &f_full_copy_list;
		type->f_copy = nullptr;
		type->f_write = &f_write_list;
		type->f_scan = nullptr;
		type->f_calcassign = &f_calcassign_list;

		type->f_assign_lv_data = &f_assign_lv_data_list;
		type->f_calcassign_lv_data = nullptr;
		type->f_free_lv_data = nullptr;

		type->f_sopt_calc = nullptr;
		type->f_bopt_calc = &f_bopt_calc_list;
		type->f_equal = &f_equal_list;
		type->f_compare = nullptr;

		type->f_iter_get = &f_iter_get_list;
		type->f_iter_save = &f_iter_save_list;
		type->f_iter_add = &f_iter_add_list;
		type->f_iter_check = &f_iter_check_list;
		type->f_iter_free = nullptr;

		return type;
	}
}