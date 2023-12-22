#include "../hy.vm.impl.h"

#include <fast_io/fast_io.h>

namespace hy {
	struct FloatStaticData {
		Vector<FloatObject*> pool;
	};

	void f_class_create_float(TypeObject* type) noexcept {
		type->v_static = new FloatStaticData;
	}

	void f_class_delete_float(TypeObject* type) noexcept {
		auto staticData{ static_cast<FloatStaticData*>(type->v_static) };
		for (auto obj : staticData->pool) delete obj;
		delete staticData;
	}

	void f_class_clean_float(TypeObject* type) noexcept {
		auto staticData{ static_cast<FloatStaticData*>(type->v_static) };
		for (auto obj : staticData->pool) delete obj;
		staticData->pool = Vector<FloatObject*>();
	}

	// arg: 浮点数
	Object* f_allocate_float(TypeObject* type, Memory arg, Memory) noexcept {
		auto& pool{ static_cast<FloatStaticData*>(type->v_static)->pool };
		FloatObject* obj;
		if (pool.empty()) obj = new FloatObject(type);
		else {
			obj = pool.back();
			pool.pop_back();
		}
		obj->value = arg_recast<Float64>(arg);
		return obj;
	}

	void f_deallocate_float(Object* obj) noexcept {
		auto& pool{ static_cast<FloatStaticData*>(obj->type->v_static)->pool };
		pool.emplace_back(obj_cast<FloatObject>(obj));
	}

	Float64 f_float_float(Object* obj) noexcept {
		return obj_cast<FloatObject>(obj)->value;
	}

	bool f_bool_float(Object* obj) noexcept {
		return static_cast<bool>(obj_cast<FloatObject>(obj)->value);
	}

	IResult<void> f_string_float(HVM, Object* obj, String* str) noexcept {
		print(fast_io::u16ostring_ref{ str }, obj_cast<FloatObject>(obj)->value);
		return IResult<void>(true);
	}

	IResult<Size> f_hash_float(HVM, Object* obj) noexcept {
		return IResult<Size>(std::hash<Float64>{}(obj_cast<FloatObject>(obj)->value));
	}

	IResult<void> f_construct_float(HVM hvm, TypeObject* type, ObjArgsView args) noexcept {
		auto vm{ vm_cast(hvm) };
		if (auto argc{ args.size() }; argc == 0ULL) {
			vm->objectStack.push_link(obj_allocate(type));
			return IResult<void>(true);
		}
		else if (argc == 1ULL) {
			if (auto arg{ args[0] }; arg->type->f_float) {
				vm->objectStack.push_link(obj_allocate(type, arg_cast(arg->type->f_float(arg))));
				return IResult<void>(true);
			}
		}
		return SetError(&SetError_UnmatchedCall, vm, type->v_name, args);
	}

	IResult<Size> f_scan_float(HVM hvm, TypeObject* type, const StringView str) noexcept {
		auto vm{ vm_cast(hvm) };
		auto start{ str.data() }, p{ start };
		while (freestanding::cvt::isblank(*p)) ++p;
		Str end;
		auto v{ freestanding::cvt::stod(p, &end) };
		if(p == end) return SetError<Size>(&SetError_ScanError, vm, type);
		vm->objectStack.push_link(obj_allocate(type, arg_cast(v)));
		return IResult<Size>(end - start);
	}

	IResult<void> f_calcassign_float(HVM hvm, LVObject* lv, Object* obj2, AssignType opt) noexcept {
		auto vm{ vm_cast(hvm) };
		auto fobj1{ lv->getAddressObject<FloatObject>() };
		if (opt != AssignType::MOD_ASSIGN && obj2->type->f_float) {
			auto v2{ obj2->type->f_float(obj2) }, v { fobj1->value };
			switch (opt) {
			case AssignType::ADD_ASSIGN: v += v2; break;
			case AssignType::SUBTRACT_ASSIGN: v -= v2; break;
			case AssignType::MULTIPLE_ASSIGN: v *= v2; break;
			case AssignType::DIVIDE_ASSIGN: v /= v2; break;
			}
			auto fobj{ obj_allocate(fobj1->type, arg_cast(v)) };
			fobj->link();
			lv->setAddress(fobj);
			fobj1->unlink();
			return IResult<void>(true);
		}
		return SetError(&SetError_IncompatibleCalcAssign, vm, fobj1->type, obj2->type, opt);
	}

	IResult<void> f_sopt_calc_float(HVM hvm, Object* obj, SOPTType) noexcept {
		auto vm{ vm_cast(hvm) };
		auto fobj{ obj_cast<FloatObject>(obj) };
		vm->objectStack.push_link(obj_allocate(obj->type, arg_cast(-fobj->value)));
		return IResult<void>(true);
	}

	IResult<void> f_bopt_calc_float(HVM hvm, Object* obj1, Object* obj2, BOPTType opt) noexcept {
		auto vm{ vm_cast(hvm) };
		if (opt == BOPTType::MOD)
			return SetError(&SetError_UnsupportedBOPT, vm, obj1->type, obj2->type, opt);
		auto v1{ obj_cast<FloatObject>(obj1)->value };
		switch (obj2->type->v_id) {
		case TypeId::Complex: {
			auto re{ obj_cast<ComplexObject>(obj2)->re }, im{ obj_cast<ComplexObject>(obj2)->im };
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
			case BOPTType::POWER: {
				auto u{ im * ::log(v1) }, r{ ::pow(v1, re) };
				ret->set(r * ::cos(u), r * ::sin(u));
				break;
			}
			}
			vm->objectStack.push_link(ret);
			return IResult<void>(true);
		}
		default: {
			if (obj2->type->f_float) {
				auto v2{ obj2->type->f_float(obj2) }, v{ v1 };
				switch (opt) {
				case BOPTType::ADD: v += v2; break;
				case BOPTType::SUBTRACT: v -= v2; break;
				case BOPTType::MULTIPLE: v *= v2; break;
				case BOPTType::DIVIDE: v /= v2; break;
				case BOPTType::POWER: v = ::pow(v, v2); break;
				}
				vm->objectStack.push_link(obj_allocate(obj1->type, arg_cast(v)));
				return IResult<void>(true);
			}
			return SetError(&SetError_UnsupportedBOPT, vm, obj1->type, obj2->type, opt);
		}
		}
	}

	IResult<bool> f_equal_float(HVM, Object* obj1, Object* obj2) noexcept {
		auto v1{ obj_cast<FloatObject>(obj1)->value };
		if (obj2->type->f_float) return IResult<bool>(v1 == obj2->type->f_float(obj2));
		return IResult<bool>(false);
	}

	IResult<bool> f_compare_float(HVM hvm, Object* obj1, Object* obj2, BOPTType opt) noexcept {
		if (obj2->type->f_float) {
			auto v1{ obj_cast<FloatObject>(obj1)->value }, v2{ obj2->type->f_float(obj2) };
			auto ret{ false };
			switch (opt) {
			case BOPTType::GT: ret = v1 > v2; break;
			case BOPTType::GE: ret = v1 >= v2; break;
			case BOPTType::LT: ret = v1 < v2; break;
			case BOPTType::LE: ret = v1 <= v2; break;
			}
			return IResult<bool>(ret);
		}
		return SetError<bool>(&SetError_UnsupportedBOPT, vm_cast(hvm), obj1->type, obj2->type, opt);
	}
}

namespace hy {
	TypeObject* AddFloatPrototype(TypeObject* prototype) noexcept {
		auto type{ new TypeObject(prototype) };
		type->v_id = TypeId::Float;
		type->v_name = u"float";
		
		type->v_static = nullptr;
		type->v_cls = nullptr;
		type->v_cps = nullptr;

		type->a_mutable = false;
		type->a_def = true;
		type->a_unused1 = type->a_unused2 = false;

		type->f_class_create = &f_class_create_float;
		type->f_class_delete = &f_class_delete_float;
		type->f_class_clean = &f_class_clean_float;
		type->f_allocate = &f_allocate_float;
		type->f_deallocate = &f_deallocate_float;
		type->f_implement = nullptr;

		type->f_float = &f_float_float;
		type->f_bool = &f_bool_float;
		type->f_string = &f_string_float;

		type->f_hash = &f_hash_float;
		type->f_len = nullptr;
		type->f_member = nullptr;
		type->f_index = nullptr;
		type->f_unpack = nullptr;

		type->f_construct = &f_construct_float;
		type->f_full_copy = nullptr;
		type->f_copy = nullptr;
		type->f_write = nullptr;
		type->f_scan = &f_scan_float;
		type->f_calcassign = &f_calcassign_float;

		type->f_assign_lv_data = nullptr;
		type->f_calcassign_lv_data = nullptr;
		type->f_free_lv_data = nullptr;

		type->f_sopt_calc = &f_sopt_calc_float;
		type->f_bopt_calc = &f_bopt_calc_float;
		type->f_equal = &f_equal_float;
		type->f_compare = &f_compare_float;

		type->f_iter_get = nullptr;
		type->f_iter_save = nullptr;
		type->f_iter_add = nullptr;
		type->f_iter_check = nullptr;
		type->f_iter_free = nullptr;

		return type;
	}
}