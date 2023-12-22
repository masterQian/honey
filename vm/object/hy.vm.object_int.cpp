#include "../hy.vm.impl.h"

#include <fast_io/fast_io.h>

namespace hy {
	struct IntStaticData {
		IntObject consts[256];
		Vector<IntObject*> pool;

		IntStaticData(TypeObject* type) noexcept {
			auto startValue{ -128LL };
			for (auto& obj : consts) {
				obj.type = type;
				obj.value = startValue++;
				obj.link();
			}
		}
	};

	void f_class_create_int(TypeObject* type) noexcept {
		type->v_static = new IntStaticData{ type };
	}

	void f_class_delete_int(TypeObject* type) noexcept {
		auto staticData{ static_cast<IntStaticData*>(type->v_static) };
		for (auto obj : staticData->pool) delete obj;
		delete staticData;
	}

	void f_class_clean_int(TypeObject* type) noexcept {
		auto staticData{ static_cast<IntStaticData*>(type->v_static) };
		for (auto obj : staticData->pool) delete obj;
		staticData->pool = Vector<IntObject*>();
	}

	// arg: 整数
	Object* f_allocate_int(TypeObject* type, Memory arg, Memory) noexcept {
		auto staticData{ static_cast<IntStaticData*>(type->v_static) };
		auto v{ arg_recast<Int64>(arg) };
		if (v >= -128LL && v <= 127LL) return &staticData->consts[v + 128LL];
		auto& pool{ staticData->pool };
		IntObject* obj;
		if (pool.empty()) obj = new IntObject{ type };
		else {
			obj = pool.back();
			pool.pop_back();
		}
		obj->value = v;
		return obj;
	}

	void f_deallocate_int(Object* obj) noexcept {
		auto& pool{ static_cast<IntStaticData*>(obj->type->v_static)->pool };
		// 回收的IntObject一定不是来自常量池的, 无需判断
		pool.emplace_back(obj_cast<IntObject>(obj));
	}

	Float64 f_float_int(Object* obj) noexcept {
		return static_cast<Float64>(obj_cast<IntObject>(obj)->value);
	}

	bool f_bool_int(Object* obj) noexcept {
		return static_cast<bool>(obj_cast<IntObject>(obj)->value);
	}

	IResult<void> f_string_int(HVM, Object* obj, String* str) noexcept {
		print(fast_io::u16ostring_ref{ str }, obj_cast<IntObject>(obj)->value);
		return IResult<void>(true);
	}

	IResult<Size> f_hash_int(HVM, Object* obj) noexcept {
		return IResult<Size>(std::hash<Int64>{}(obj_cast<IntObject>(obj)->value));
	}

	IResult<void> f_construct_int(HVM hvm, TypeObject* type, ObjArgsView args) noexcept {
		auto vm{ vm_cast(hvm) };
		if (auto argc{ args.size() }; argc == 0ULL) {
			vm->objectStack.push_link(obj_allocate(type));
			return IResult<void>(true);
		}
		else if (argc == 1ULL) {
			if (auto arg{ args[0] }; arg->type->v_id == TypeId::Int) {
				vm->objectStack.push_link(obj_allocate(type, arg_cast(obj_cast<IntObject>(arg)->value)));
				return IResult<void>(true);
			}
		}
		return SetError(&SetError_UnmatchedCall, vm, type->v_name, args);
	}

	IResult<Size> f_scan_int(HVM hvm, TypeObject* type, const StringView str) noexcept {
		auto vm{ vm_cast(hvm) };
		auto start{ str.data() }, p{ start };
		while (freestanding::cvt::isblank(*p)) ++p;
		if (!freestanding::cvt::check_stol(p)) {
			Str end;
			auto v{ freestanding::cvt::safe_stol(p, &end) };
			vm->objectStack.push_link(obj_allocate(type, arg_cast(v)));
			return IResult<Size>(end - start);
		}
		return SetError<Size>(&SetError_ScanError, vm, type);
	}

	IResult<void> f_calcassign_int(HVM hvm, LVObject* lv, Object* obj2, AssignType opt) noexcept {
		auto vm{ vm_cast(hvm) };
		auto iobj1{ lv->getAddressObject<IntObject>() };
		if (obj2->type->v_id == TypeId::Int) {
			auto v2{ obj_cast<IntObject>(obj2)->value }, v{ iobj1->value };
			switch (opt) {
			case AssignType::ADD_ASSIGN: v += v2; break;
			case AssignType::SUBTRACT_ASSIGN: v -= v2; break;
			case AssignType::MULTIPLE_ASSIGN: v *= v2; break;
			case AssignType::DIVIDE_ASSIGN: v = v2 == 0LL ? 922337203685477580LL : v / v2; break;
			case AssignType::MOD_ASSIGN: v = v2 == 0LL ? 0LL : v % v2; break;
			}
			auto iobj{ obj_allocate(iobj1->type, arg_cast(v)) };
			iobj->link();
			lv->setAddress(iobj);
			iobj1->unlink();
			return IResult<void>(true);
		}
		return SetError(&SetError_IncompatibleCalcAssign, vm, iobj1->type, obj2->type, opt);
	}

	IResult<void> f_sopt_calc_int(HVM hvm, Object* obj, SOPTType) noexcept {
		auto vm{ vm_cast(hvm) };
		auto iobj{ obj_cast<IntObject>(obj) };
		vm->objectStack.push_link(obj_allocate(obj->type, arg_cast(-iobj->value)));
		return IResult<void>(true);
	}

	IResult<void> f_bopt_calc_int(HVM hvm, Object* obj1, Object* obj2, BOPTType opt) noexcept {
		auto vm{ vm_cast(hvm) };
		auto& ost{ vm->objectStack };
		auto v1{ obj_cast<IntObject>(obj1)->value };
		auto floatType{ vm->getType(TypeId::Float) };
		switch (opt) {
		case BOPTType::MOD: {
			if (obj2->type->v_id == TypeId::Int) {
				auto v2{ obj_cast<IntObject>(obj2)->value };
				ost.push_link(obj_allocate(obj1->type, arg_cast(v2 == 0LL ? 0LL : v1 % v2)));
				return IResult<void>(true);
			}
			return SetError(&SetError_UnsupportedBOPT, vm, obj1->type, obj2->type, opt);
		}
		case BOPTType::POWER: {
			Object* ret{ };
			switch (obj2->type->v_id) {
			case TypeId::Int: ret = obj_allocate(floatType, arg_cast(::pow(v1, obj_cast<IntObject>(obj2)->value))); break;
			case TypeId::Float: ret = obj_allocate(floatType, arg_cast(::pow(v1, obj_cast<FloatObject>(obj2)->value))); break;
			case TypeId::Complex: {
				auto cobj2{ obj_cast<ComplexObject>(obj2) };
				auto u{ cobj2->im * ::log(v1) };
				auto r{ ::pow(v1, cobj2->re) };
				ret = obj_allocate<ComplexObject>(obj2->type, arg_cast(r * ::cos(u)), arg_cast(r * ::sin(u)));
				break;
			}
			default: return SetError(&SetError_UnsupportedBOPT, vm, obj1->type, obj2->type, opt);
			}
			ost.push_link(ret);
			return IResult<void>(true);
		}
		default: {
			switch (obj2->type->v_id) {
			case TypeId::Int: {
				auto v2{ obj_cast<IntObject>(obj2)->value }, v{ v1 };
				switch (opt) {
				case BOPTType::ADD: v += v2; break;
				case BOPTType::SUBTRACT: v -= v2; break;
				case BOPTType::MULTIPLE: v *= v2; break;
				case BOPTType::DIVIDE: v = v2 == 0LL ? 9223372036854775807LL : v / v2; break;
				}
				ost.push_link(obj_allocate(obj1->type, arg_cast(v)));
				break;
			}
			case TypeId::Float: {
				auto v2{ obj_cast<FloatObject>(obj2)->value }, v{ static_cast<Float64>(v1) };
				switch (opt) {
				case BOPTType::ADD: v += v2; break;
				case BOPTType::SUBTRACT: v -= v2; break;
				case BOPTType::MULTIPLE: v *= v2; break;
				case BOPTType::DIVIDE: v /= v2; break;
				}
				ost.push_link(obj_allocate(floatType, arg_cast(v)));
				break;
			}
			case TypeId::Complex: {
				auto re{ obj_cast<ComplexObject>(obj2)->re };
				auto im{ obj_cast<ComplexObject>(obj2)->im };
				auto ret{ obj_allocate<ComplexObject>(obj2->type) };
				switch (opt) {
				case BOPTType::ADD: ret->set(v1 + re, im); break;
				case BOPTType::SUBTRACT: ret->set(v1 - re, -im); break;
				case BOPTType::MULTIPLE: ret->set(v1 * re, v1 * im); break;
				case BOPTType::DIVIDE: {
					auto u{ re * re + im * im };
					ret->set(v1 * re / u, -v1 * im / u);
					break;
				}
				}
				ost.push_link(ret);
				break;
			}
			default: return SetError(&SetError_UnsupportedBOPT, vm, obj1->type, obj2->type, opt);
			}
			return IResult<void>(true);
		}
		}
	}

	IResult<bool> f_equal_int(HVM, Object* obj1, Object* obj2) noexcept {
		auto v1{ obj_cast<IntObject>(obj1)->value };
		switch (obj2->type->v_id) {
		case TypeId::Int: return IResult<bool>(v1 == obj_cast<IntObject>(obj2)->value);
		case TypeId::Float: return IResult<bool>(v1 == obj_cast<FloatObject>(obj2)->value);
		default: return IResult<bool>(false);
		}
	}

	IResult<bool> f_compare_int(HVM hvm, Object* obj1, Object* obj2, BOPTType opt) noexcept {
		auto v1{ obj_cast<IntObject>(obj1)->value };
		auto ret{ false };
		switch (obj2->type->v_id) {
		case TypeId::Int: {
			auto v2{ obj_cast<IntObject>(obj2)->value };
			switch (opt) {
			case BOPTType::GT: ret = v1 > v2; break;
			case BOPTType::GE: ret = v1 >= v2; break;
			case BOPTType::LT: ret = v1 < v2; break;
			case BOPTType::LE: ret = v1 <= v2; break;
			}
			break;
		}
		case TypeId::Float: {
			auto v2{ obj_cast<FloatObject>(obj2)->value };
			switch (opt) {
			case BOPTType::GT: ret = v1 > v2; break;
			case BOPTType::GE: ret = v1 >= v2; break;
			case BOPTType::LT: ret = v1 < v2; break;
			case BOPTType::LE: ret = v1 <= v2; break;
			}
			break;
		}
		default: return SetError<bool>(&SetError_UnsupportedBOPT, vm_cast(hvm), obj1->type, obj2->type, opt);
		}
		return IResult<bool>(ret);
	}
}

namespace hy {
	TypeObject* AddIntPrototype(TypeObject* prototype) noexcept {
		auto type{ new TypeObject(prototype) };
		type->v_id = TypeId::Int;
		type->v_name = u"int";
		
		type->v_static = nullptr;
		type->v_cls = nullptr;
		type->v_cps = nullptr;

		type->a_mutable = false;
		type->a_def = true;
		type->a_unused1 = type->a_unused2 = false;

		type->f_class_create = &f_class_create_int;
		type->f_class_delete = &f_class_delete_int;
		type->f_class_clean = &f_class_clean_int;
		type->f_allocate = &f_allocate_int;
		type->f_deallocate = &f_deallocate_int;
		type->f_implement = nullptr;

		type->f_float = &f_float_int;
		type->f_bool = &f_bool_int;
		type->f_string = &f_string_int;

		type->f_hash = &f_hash_int;
		type->f_len = nullptr;
		type->f_member = nullptr;
		type->f_index = nullptr;
		type->f_unpack = nullptr;

		type->f_construct = &f_construct_int;
		type->f_full_copy = nullptr;
		type->f_copy = nullptr;
		type->f_write = nullptr;
		type->f_scan = &f_scan_int;
		type->f_calcassign = &f_calcassign_int;

		type->f_assign_lv_data = nullptr;
		type->f_calcassign_lv_data = nullptr;
		type->f_free_lv_data = nullptr;

		type->f_sopt_calc = &f_sopt_calc_int;
		type->f_bopt_calc = &f_bopt_calc_int;
		type->f_equal = &f_equal_int;
		type->f_compare = &f_compare_int;

		type->f_iter_get = nullptr;
		type->f_iter_save = nullptr;
		type->f_iter_add = nullptr;
		type->f_iter_check = nullptr;
		type->f_iter_free = nullptr;

		return type;
	}
}