#include "hy.vm.impl.h"

namespace hy {
	using ImplementFunc = bool(*)(TypeObject*, TypeObject*) noexcept;

	bool f_implement_real(TypeObject*, TypeObject* type) noexcept {
		return type->v_id == TypeId::Int || type->v_id == TypeId::Float;
	}

	bool f_implement_numeric(TypeObject*, TypeObject* type) noexcept {
		return type->v_id == TypeId::Int || type->v_id == TypeId::Float || type->v_id == TypeId::Complex;
	}

	bool f_implement_interface(TypeObject* cpt, TypeObject* type) noexcept {
		auto ret{ false };
		auto& tcs = *cpt->v_cps;
		for (Index i{ }, l{ tcs.size() }; i < l; ++i) {
			auto& ci{ tcs[i] };
			switch (ci.sct) {
			case SubConceptElementType::NOT: ret = !ret; break;
			case SubConceptElementType::JUMP_TRUE:
			case SubConceptElementType::JUMP_FALSE: {
				// 决定是否跳转(同或关系)
				// JUMP_TRUE | ret	| isJump
				//     0     |  0   |   1
				//     0     |  1   |   0
				//     1     |  0   |   0
				//     1     |  1   |   1
				if ((ci.sct == SubConceptElementType::JUMP_TRUE) == ret) i += ci.v.offset - 1ULL;
				break;
			}
			case SubConceptElementType::SET: ret = ci.v.type->canImplement(type); break;
			}
		}
		return ret;
	}
}

namespace hy {
	TypeObject* GenerateConcept(TypeObject* prototype, TypeObject* nullType,
		TypeConceptStruct* conceptStruct, const StringView name, ImplementFunc func) noexcept {
		auto type{ new TypeObject(prototype) };
		type->v_name = name;
		type->makeUserClassHash();

		type->v_static = nullptr;
		type->v_cls = nullptr;
		type->v_cps = conceptStruct;

		type->a_mutable = false;
		type->a_def = false;
		type->a_unused1 = type->a_unused2 = false;

		type->f_class_create = nullptr;
		type->f_class_delete = nullptr;
		type->f_class_clean = nullptr;
		type->f_allocate = nullType->f_allocate;
		type->f_deallocate = nullType->f_deallocate;
		type->f_implement = func;

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

	TypeObject* AddInterfacePrototype(VM* vm, const StringView name, TypeConceptStruct* conceptStruct) noexcept {
		return GenerateConcept(vm->getType(TypeId::Type), vm->getType(TypeId::Null), conceptStruct, name, &f_implement_interface);
	}

	void AddBuiltinConcept(VM* vm) noexcept {
		auto prototype{ vm->getType(TypeId::Type) };
		auto nullType{ vm->getType(TypeId::Null) };

		auto& dom{ vm->moduleTree.builtin->dom };

		struct BuiltinConcept {
			StringView name;
			bool (*implFunc)(TypeObject*, TypeObject*) noexcept;
		}builtinConcepts[] {
			{ u"real", &f_implement_real },
			{ u"numeric", &f_implement_numeric },
		};

		for (auto& builtinConcept : builtinConcepts) {
			auto type{ GenerateConcept(prototype, nullType, new TypeConceptStruct,
				builtinConcept.name, builtinConcept.implFunc) };
			dom.symbols.setSymbol(type->v_name, type);
		}
	}
}