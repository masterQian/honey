#include "../hy.vm.impl.h"

#include <fast_io/fast_io.h>

namespace hy {
	struct RangeStaticData {
		Vector<RangeObject*> pool;
	};

	void f_class_create_range(TypeObject* type) noexcept {
		type->v_static = new RangeStaticData;
	}

	void f_class_delete_range(TypeObject* type) noexcept {
		auto staticData{ static_cast<RangeStaticData*>(type->v_static) };
		for (auto obj : staticData->pool) delete obj;
		delete staticData;
	}

	void f_class_clean_range(TypeObject* type) noexcept {
		auto staticData{ static_cast<RangeStaticData*>(type->v_static) };
		for (auto obj : staticData->pool) delete obj;
		staticData->pool = Vector<RangeObject*>();
	}

	// arg1: Int64[3] 数据
	Object* f_allocate_range(TypeObject* type, Memory arg, Memory) noexcept {
		auto& pool{ static_cast<RangeStaticData*>(type->v_static)->pool };
		RangeObject* obj;
		if (pool.empty()) obj = new RangeObject(type);
		else {
			obj = pool.back();
			pool.pop_back();
		}
		if (arg) {
			auto pData{ arg_recast<Int64*>(arg) };
			obj->start = pData[0];
			obj->step = pData[1];
			obj->end = pData[2];
		}
		else {
			obj->start = 0;
			obj->step = 1;
			obj->end = 0;
		}
		return obj;
	}

	void f_deallocate_range(Object* obj) noexcept {
		auto& pool{ static_cast<RangeStaticData*>(obj->type->v_static)->pool };
		pool.emplace_back(obj_cast<RangeObject>(obj));
	}

	IResult<void> f_string_range(HVM, Object* obj, String* str) noexcept {
		auto robj{ obj_cast<RangeObject>(obj) };
		print(fast_io::u16ostring_ref{ str }, robj->start, u"~", robj->step, u"~", robj->end);
		return IResult<void>(true);
	}

	IResult<Size> f_hash_range(HVM, Object* obj) noexcept {
		std::hash<Int64> hasher;
		auto robj{ obj_cast<RangeObject>(obj) };
		return IResult<Size>(hasher(robj->start) ^ hasher(robj->step) ^ hasher(robj->end));
	}

	IResult<Size> f_len_range(HVM, Object* obj) noexcept {
		auto robj{ obj_cast<RangeObject>(obj) };
		return IResult<Size>(static_cast<Size>((robj->end - robj->start) / robj->step) + 1ULL);
	}

	IResult<void> f_member_range(HVM hvm, bool isLV, Object* obj, const StringView member) noexcept {
		auto vm{ vm_cast(hvm) };
		auto robj{ obj_cast<RangeObject>(obj) };
		if (!isLV) {
			if (member == u"start" || member == u"step" || member == u"end") {
				Int64 vInt{ };
				if (member == u"start") vInt = robj->start;
				else if (member == u"step") vInt = robj->step;
				else vInt = robj->end;
				vm->objectStack.push_link(obj_allocate(vm->getType(TypeId::Int), arg_cast(vInt)));
				return IResult<void>(true);
			}
			else return SetError(&SetError_UnmatchedMember, vm, obj->type, member);
		}
		else return SetError(&SetError_NotLeftValue, vm, member);
	}

	IResult<void> f_index_range(HVM hvm, bool isLV, Object* obj, ObjArgsView args) noexcept {
		auto vm{ vm_cast(hvm) };
		auto robj{ obj_cast<RangeObject>(obj) };
		if (auto argc{ args.size() }; argc == 1ULL) {
			if (auto arg{ args[0] }; arg->type->v_id == TypeId::Int) {
				auto length{ static_cast<Int64>((robj->end - robj->start) / robj->step) + 1 };
				auto index{ obj_cast<IntObject>(arg)->value };
				if (index > 0LL && index <= length) {
					if (!isLV) {
						vm->objectStack.push_link(obj_allocate(vm->getType(TypeId::Int),
							arg_cast(robj->start + (index - 1) * robj->step)));
						return IResult<void>(true);
					}
					else SetError_NotLeftValue(vm, u"");
				}
				else SetError_IndexOutOfRange(vm, index, length);
			}
			else SetError_UnmatchedIndex(vm, obj->type->v_name, args);
		}
		else SetError_UnmatchedIndex(vm, obj->type->v_name, args);
		return IResult<void>();
	}

	IResult<void> f_unpack_range(HVM hvm, Object* obj, Size argc) noexcept {
		auto vm{ vm_cast(hvm) };
		auto robj{ obj_cast<RangeObject>(obj) };
		if (argc == 3ULL) {
			auto type{ vm->getType(TypeId::Int) };
			auto& ost{ vm->objectStack };
			ost.push_link(obj_allocate(type, arg_cast(robj->start)));
			ost.push_link(obj_allocate(type, arg_cast(robj->step)));
			ost.push_link(obj_allocate(type, arg_cast(robj->end)));
			return IResult<void>(true);
		}
		else return SetError(&SetError_UnmatchedUnpack, vm, 3ULL, argc);
	}

	IResult<void> f_construct_range(HVM hvm, TypeObject* type, ObjArgsView args) noexcept {
		auto vm{ vm_cast(hvm) };
		Int64 data[] { 0, 1, 0 };
		if (auto argc{ args.size() }; argc == 2ULL || argc == 3ULL) {
			auto arg1{ args[0] }, arg2{ args[1] };
			if (arg1->type->v_id == TypeId::Int && arg2->type->v_id == TypeId::Int) {
				data[0] = obj_cast<IntObject>(arg1)->value;
				data[2] = obj_cast<IntObject>(arg2)->value;
				if (argc == 3ULL) {
					auto arg3{ args[2] };
					if (arg3->type->v_id == TypeId::Int) {
						data[1] = data[2];
						data[2] = obj_cast<IntObject>(arg3)->value;
						if (RangeObject::check(data[0], data[1], data[2])) {
							vm->objectStack.push_link(obj_allocate(type, arg_cast(data)));
							return IResult<void>(true);
						}
						else SetError_IllegalRange(vm, data[0], data[1], data[2]);
					}
					else SetError_UnmatchedCall(vm, type->v_name, args);
				}
				else {
					if (RangeObject::check(data[0], data[1], data[2])) {
						vm->objectStack.push_link(obj_allocate(type, arg_cast(data)));
						return IResult<void>(true);
					}
					else SetError_IllegalRange(vm, data[0], data[1], data[2]);
				}
			}
			else SetError_UnmatchedCall(vm, type->v_name, args);
		}
		return IResult<void>();
	}

	constexpr auto RANGE_ITER_ELEM{ 0ULL };

	IResult<void> f_iter_get_range(HVM hvm, Object* obj) noexcept {
		auto vm{ vm_cast(hvm) };
		auto robj{ obj_cast<RangeObject>(obj) };
		auto iter{ obj_allocate<IteratorObject>(vm->getType(TypeId::Iterator)) };
		iter->ref = robj;
		iter->ref->link();
		iter->setData<RANGE_ITER_ELEM>(robj->start);
		vm->objectStack.push_link(iter);
		return IResult<void>(true);
	}

	IResult<void> f_iter_save_range(HVM hvm, IteratorObject* iter, Size argc) noexcept {
		auto vm{ vm_cast(hvm) };
		if (argc == 1ULL) {
			vm->objectStack.push_link(obj_allocate(vm->getType(TypeId::Int), iter->getData<0ULL, Memory>()));
			return IResult<void>(true);
		}
		return SetError(&SetError_UnmatchedUnpack, vm, 1ULL, argc);
	}

	IResult<void> f_iter_add_range(HVM, IteratorObject* iter) noexcept {
		auto robj{ iter->getReference<RangeObject>() };
		iter->resetData<0ULL, Int64>((iter->getData<0ULL, Int64>() + robj->step));
		return IResult<void>(true);
	}

	IResult<bool> f_iter_check_range(HVM, IteratorObject* iter) noexcept {
		auto robj{ iter->getReference<RangeObject>() };
		auto v{ iter->getData<0ULL, Int64>() };
		if (robj->step > 0) return IResult<bool>(v <= robj->end);
		else return IResult<bool>(v >= robj->end);
	}
}

namespace hy {
	TypeObject* AddRangePrototype(TypeObject* prototype) noexcept {
		auto type{ new TypeObject(prototype) };
		type->v_id = TypeId::Range;
		type->v_name = u"range";

		type->v_static = nullptr;
		type->v_cls = nullptr;
		type->v_cps = nullptr;

		type->a_mutable = false;
		type->a_def = true;
		type->a_unused1 = type->a_unused2 = false;

		type->f_class_create = &f_class_create_range;
		type->f_class_delete = &f_class_delete_range;
		type->f_class_clean = &f_class_clean_range;
		type->f_allocate = &f_allocate_range;
		type->f_deallocate = &f_deallocate_range;
		type->f_implement = nullptr;

		type->f_float = nullptr;
		type->f_bool = nullptr;
		type->f_string = &f_string_range;

		type->f_hash = &f_hash_range;
		type->f_len = &f_len_range;
		type->f_member = &f_member_range;
		type->f_index = &f_index_range;
		type->f_unpack = &f_unpack_range;

		type->f_construct = &f_construct_range;
		type->f_full_copy = nullptr;
		type->f_copy = nullptr;
		type->f_write = nullptr;
		type->f_scan = nullptr;
		type->f_calcassign = nullptr;

		type->f_assign_lv_data = nullptr;
		type->f_calcassign_lv_data = nullptr;
		type->f_free_lv_data = nullptr;

		type->f_bopt_calc = nullptr;
		type->f_sopt_calc = nullptr;
		type->f_equal = &f_equal_ref;
		type->f_compare = nullptr;

		type->f_iter_get = &f_iter_get_range;
		type->f_iter_save = &f_iter_save_range;
		type->f_iter_add = &f_iter_add_range;
		type->f_iter_check = &f_iter_check_range;
		type->f_iter_free = nullptr;

		return type;
	}
}