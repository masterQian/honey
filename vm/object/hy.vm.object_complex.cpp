#include "../hy.vm.impl.h"

#include <fast_io/fast_io.h>

namespace hy {
	struct ComplexStaticData {
		Vector<ComplexObject*> pool;
	};

	void f_class_create_complex(TypeObject* type) noexcept {
		type->v_static = new ComplexStaticData;
	}

	void f_class_delete_complex(TypeObject* type) noexcept {
		auto staticData{ static_cast<ComplexStaticData*>(type->v_static) };
		for (auto obj : staticData->pool) delete obj;
		delete staticData;
	}

	void f_class_clean_complex(TypeObject* type) noexcept {
		auto staticData{ static_cast<ComplexStaticData*>(type->v_static) };
		for (auto obj : staticData->pool) delete obj;
		staticData->pool = Vector<ComplexObject*>();
	}

	// arg1: Float64 实部
	// arg2: Float64 虚部
	Object* f_allocate_complex(TypeObject* type, Memory arg1, Memory arg2) noexcept {
		auto& pool{ static_cast<ComplexStaticData*>(type->v_static)->pool };
		ComplexObject* obj;
		if (pool.empty()) obj = new ComplexObject(type);
		else {
			obj = pool.back();
			pool.pop_back();
		}
		obj->set(arg_recast<Float64>(arg1), arg_recast<Float64>(arg2));
		return obj;
	}

	void f_deallocate_complex(Object* obj) noexcept {
		auto& pool{ static_cast<ComplexStaticData*>(obj->type->v_static)->pool };
		pool.emplace_back(obj_cast<ComplexObject>(obj));
	}

	IResult<void> f_string_complex(HVM, Object* obj, String* str) noexcept {
		fast_io::u16ostring_ref strRef{ str };
		auto cobj{ obj_cast<ComplexObject>(obj) };
		if (cobj->re != 0.0) {
			print(strRef, cobj->re);
			if (cobj->im > 0.0) str->push_back(u'+');
		}
		else if (cobj->im == 0.0) {
			str->push_back(u'0');
			return IResult<void>(true);
		}
		if (cobj->im == 1.0) str->push_back(u'i');
		else if (cobj->im == -1.0) str->append(u"-i");
		else if (cobj->im != 0.0) print(strRef, cobj->im, fast_io::mnp::chvw(u'i'));
		return IResult<void>(true);
	}

	IResult<Size> f_hash_complex(HVM, Object* obj) noexcept {
		return IResult<Size>(std::hash<Float64>{}(obj_cast<ComplexObject>(obj)->re)
			^ std::hash<Float64>{}(obj_cast<ComplexObject>(obj)->im));
	}

	IResult<void> f_member_complex(HVM hvm, bool isLV, Object* obj, const StringView member) noexcept {
		auto vm{ vm_cast(hvm) };
		auto cobj{ obj_cast<ComplexObject>(obj) };
		if (!isLV && (member == u"re" || member == u"im")) {
			auto ret{ obj_allocate(vm->getType(TypeId::Float),
				arg_cast(member == u"re" ? cobj->re : cobj->im)) };
			vm->objectStack.push_link(ret);
			return IResult<void>(true);
		}
		return SetError(&SetError_UnmatchedMember, vm, obj->type, member);
	}

	IResult<void> f_unpack_complex(HVM hvm, Object* obj, Size argc) noexcept {
		auto vm{ vm_cast(hvm) };
		auto cobj{ obj_cast<ComplexObject>(obj) };
		if (argc == 2ULL) {
			auto type{ vm->getType(TypeId::Float) };
			vm->objectStack.push_link(obj_allocate(type, arg_cast(cobj->re)));
			vm->objectStack.push_link(obj_allocate(type, arg_cast(cobj->im)));
			return IResult<void>(true);
		}
		else return SetError(&SetError_UnmatchedUnpack, vm, 2ULL, argc);
	}

	IResult<void> f_construct_complex(HVM hvm, TypeObject* type, ObjArgsView args) noexcept {
		auto vm{ vm_cast(hvm) };
		auto& ost{ vm->objectStack };
		if (auto argc{ args.size() }; argc == 0ULL) {
			ost.push_link(obj_allocate(type));
			return IResult<void>(true);
		}
		else if (argc == 1ULL) {
			if (auto arg{ args[0] }; arg->type->v_id == TypeId::Complex) {
				ost.push_link(obj_allocate(type, arg_cast(obj_cast<ComplexObject>(arg)->re),
					arg_cast(obj_cast<ComplexObject>(arg)->im)));
				return IResult<void>(true);
			}
			else if (arg->type->f_float) {
				ost.push_link(obj_allocate(type, arg_cast(arg->type->f_float(arg))));
				return IResult<void>(true);
			}
		}
		else if (argc == 2ULL) {
			if (auto arg1{ args[0] }, arg2{ args[1] }; arg1->type->f_float && arg2->type->f_float) {
				ost.push_link(obj_allocate(type, arg_cast(arg1->type->f_float(arg1)),
					arg_cast(arg2->type->f_float(arg2))));
				return IResult<void>(true);
			}
		}
		return SetError(&SetError_UnmatchedCall, vm, type->v_name, args);
	}

	IResult<void> f_calcassign_complex(HVM hvm, LVObject* lv, Object* obj2, AssignType opt) noexcept {
		auto vm{ vm_cast(hvm) };
		auto cobj1{ lv->getAddressObject<ComplexObject>() };
		if (opt == AssignType::MOD_ASSIGN || (obj2->type->v_id != TypeId::Complex && !obj2->type->f_float))
			return SetError(&SetError_IncompatibleCalcAssign, vm, cobj1->type, obj2->type, opt);
		Float64 re1{ cobj1->re }, im1{ cobj1->im }, re{ }, im{ };
		if (obj2->type->v_id == TypeId::Complex) {
			auto re2{ obj_cast<ComplexObject>(obj2)->re };
			auto im2{ obj_cast<ComplexObject>(obj2)->im };
			switch (opt) {
			case AssignType::ADD_ASSIGN: re = re1 + re2; im = im1 + im2; break;
			case AssignType::SUBTRACT_ASSIGN: re = re1 - re2; im = im1 - im2; break;
			case AssignType::MULTIPLE_ASSIGN: re = re1 * re2 - im1 * im2; im = re1 * im2 + im1 * re2; break;
			case AssignType::DIVIDE_ASSIGN: {
				auto u{ re2 * re2 + im2 * im2 };
				re = (re1 * re2 + im1 * im2) / u;
				im = (im1 * re2 - re1 * im2) / u;
				break;
			}
			}
		}
		else {
			auto value{ obj2->type->f_float(obj2) };
			switch (opt) {
			case AssignType::ADD_ASSIGN: re = re1 + value; im = im1; break;
			case AssignType::SUBTRACT_ASSIGN: re = re1 - value; im = im1; break;
			case AssignType::MULTIPLE_ASSIGN: re = re1 * value; im = im1 * value; break;
			case AssignType::DIVIDE_ASSIGN: re = re1 / value; im = im1 / value; break;
			}
		}
		auto ret{ obj_allocate(cobj1->type, arg_cast(re), arg_cast(im)) };
		ret->link();
		lv->setAddress(ret);
		cobj1->unlink();
		return IResult<void>(true);
	}

	IResult<void> f_sopt_calc_complex(HVM hvm, Object* obj, SOPTType) noexcept {
		auto vm{ vm_cast(hvm) };
		auto cobj{ obj_cast<ComplexObject>(obj) };
		vm->objectStack.push_link(obj_allocate(obj->type, arg_cast(-cobj->re), arg_cast(-cobj->im)));
		return IResult<void>(true);
	}

	IResult<void> f_bopt_calc_complex(HVM hvm, Object* obj1, Object* obj2, BOPTType opt) noexcept {
		auto vm{ vm_cast(hvm) };
		if (opt == BOPTType::MOD)
			return SetError(&SetError_UnsupportedBOPT, vm, obj1->type, obj2->type, opt);
		auto re1{ obj_cast<ComplexObject>(obj1)->re };
		auto im1{ obj_cast<ComplexObject>(obj1)->im };
		if (obj2->type->v_id == TypeId::Complex) {
			auto re2{ obj_cast<ComplexObject>(obj2)->re };
			auto im2{ obj_cast<ComplexObject>(obj2)->im };
			auto ret{ obj_allocate<ComplexObject>(obj1->type) };
			switch (opt) {
			case BOPTType::ADD: ret->set(re1 + re2, im1 + im2); break;
			case BOPTType::SUBTRACT: ret->set(re1 - re2, im1 - im2); break;
			case BOPTType::MULTIPLE: ret->set(re1 * re2 - im1 * im2, re1 * im2 + im1 * re2); break;
			case BOPTType::DIVIDE: {
				auto u{ re2 * re2 + im2 * im2 };
				ret->set((re1 * re2 + im1 * im2) / u, (im1 * re2 - re1 * im2) / u);
				break;
			}
			case BOPTType::POWER: {
				if (re1 == 0.0 && im1 == 0.0) ret->set(0.0, 0.0);
				else if (re2 == 0.0 && im2 == 0.0) ret->set(1.0, 0.0);
				else {
					auto r{ ::sqrt(re1 * re1 + im1 * im1) };
					auto theta{ ::atan2(im1, re1) };
					auto rr{ ::pow(r, re2) * ::exp(-im2 * theta) };
					auto theta1{ re2 * theta };
					auto theta2{ im2 * ::log(r) };
					auto c1{ ::cos(theta1) }, c2{ ::cos(theta2) };
					auto s1{ ::sin(theta1) }, s2{ ::sin(theta2) };
					ret->set(rr * (c1 * c2 - s1 * s2), rr * (c1 * s2 + s1 * c2));
				}
				break;
			}
			}
			vm->objectStack.push_link(ret);
			return IResult<void>(true);
		}
		if (obj2->type->f_float) {
			auto value{ obj2->type->f_float(obj2) };
			auto ret{ obj_allocate<ComplexObject>(obj1->type) };
			switch (opt) {
			case BOPTType::ADD: ret->set(re1 + value, im1); break;
			case BOPTType::SUBTRACT: ret->set(re1 - value, im1); break;
			case BOPTType::MULTIPLE: ret->set(re1 * value, im1 * value); break;
			case BOPTType::DIVIDE: ret->set(re1 / value, im1 / value); break;
			case BOPTType::POWER: {
				if (re1 == 0.0 && im1 == 0.0) ret->set(0.0, 0.0);
				else {
					auto r{ ::pow(re1 * re1 + im1 * im1, value / 2) };
					auto thetaV{ ::atan2(im1, re1) * value };
					ret->set(r * ::cos(thetaV), r * ::sin(thetaV));
				}
				break;
			}
			}
			vm->objectStack.push_link(ret);
			return IResult<void>(true);
		}
		return SetError(&SetError_UnsupportedBOPT, vm, obj1->type, obj2->type, opt);
	}

	IResult<bool> f_equal_complex(HVM, Object* obj1, Object* obj2) noexcept {
		auto cobj1{ obj_cast<ComplexObject>(obj1) };
		if (obj2->type->v_id == TypeId::Complex) return IResult<bool>(cobj1->re == obj_cast<ComplexObject>(obj2)->re
			&& cobj1->im == obj_cast<ComplexObject>(obj2)->im);
		else if (obj2->type->f_float) return IResult<bool>(cobj1->re == obj2->type->f_float(obj2) && cobj1->im == 0.0);
		return IResult<bool>(false);
	}
}

namespace hy {
	TypeObject* AddComplexPrototype(TypeObject* prototype) noexcept {
		auto type{ new TypeObject(prototype) };
		type->v_id = TypeId::Complex;
		type->v_name = u"complex";
		
		type->v_static = nullptr;
		type->v_cls = nullptr;
		type->v_cps = nullptr;

		type->a_mutable = false;
		type->a_def = true;
		type->a_unused1 = type->a_unused2 = false;

		type->f_class_create = &f_class_create_complex;
		type->f_class_delete = &f_class_delete_complex;
		type->f_class_clean = &f_class_clean_complex;
		type->f_allocate = &f_allocate_complex;
		type->f_deallocate = &f_deallocate_complex;
		type->f_implement = nullptr;

		type->f_float = nullptr;
		type->f_bool = nullptr;
		type->f_string = &f_string_complex;

		type->f_hash = &f_hash_complex;
		type->f_len = nullptr;
		type->f_member = &f_member_complex;
		type->f_index = nullptr;
		type->f_unpack = &f_unpack_complex;

		type->f_construct = &f_construct_complex;
		type->f_full_copy = nullptr;
		type->f_copy = nullptr;
		type->f_write = nullptr;
		type->f_scan = nullptr;
		type->f_calcassign = &f_calcassign_complex;
		
		type->f_assign_lv_data = nullptr;
		type->f_calcassign_lv_data = nullptr;
		type->f_free_lv_data = nullptr;

		type->f_sopt_calc = &f_sopt_calc_complex;
		type->f_bopt_calc = &f_bopt_calc_complex;
		type->f_equal = &f_equal_complex;
		type->f_compare = nullptr;

		type->f_iter_get = nullptr;
		type->f_iter_save = nullptr;
		type->f_iter_add = nullptr;
		type->f_iter_check = nullptr;
		type->f_iter_free = nullptr;

		return type;
	}
}