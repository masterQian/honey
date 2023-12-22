#include "../hy.vm.impl.h"

#include <fast_io/fast_io.h>

namespace hy::impl {
	void Bin_ShowBytes(BinObject* bobj, String* str) noexcept {
		fast_io::u16ostring_ref strRef{ str };
		str->push_back(u'{');
		for (auto i : bobj->data)
			print(strRef, fast_io::mnp::hexupper(i), fast_io::mnp::chvw(u','));
		if (bobj->data.empty()) str->push_back(u'}');
		else str->back() = u'}';
	}

	LIB_EXPORT void Bin_Clear(HVM hvm, ObjArgsView, Object* thisObject) noexcept {
		auto vm{ vm_cast(hvm) };
		auto bobj{ obj_cast<BinObject>(thisObject) };
		bobj->data.clear();
		vm->objectStack.push_link(bobj);
	}

	LIB_EXPORT void Bin_Reverse(HVM hvm, ObjArgsView, Object* thisObject) noexcept {
		auto vm{ vm_cast(hvm) };
		auto bobj{ obj_cast<BinObject>(thisObject) };
		std::reverse(bobj->data.begin(), bobj->data.end());
		vm->objectStack.push_link(bobj);
	}
}

namespace hy {
	struct BinStaticData {
		Vector<BinObject*> pool;
		FunctionTable ft;

		BinStaticData(TypeObject* type) noexcept {
			auto funcType{ type->__getType(TypeId::Function) };

			ObjArgsView empty;

			ft.try_emplace(u"clear", MakeNative<true, true>(funcType, u"bin::clear", &impl::Bin_Clear, empty));
			ft.try_emplace(u"reverse", MakeNative<true, true>(funcType, u"bin::reverse", &impl::Bin_Reverse, empty));
		}
	};

	void f_class_create_bin(TypeObject* type) noexcept {
		type->v_static = new BinStaticData(type);
	}

	void f_class_delete_bin(TypeObject* type) noexcept {
		auto staticData{ static_cast<BinStaticData*>(type->v_static) };
		for (auto obj : staticData->pool) delete obj;
		for (auto& [_, fobj] : staticData->ft) delete fobj;
		delete staticData;
	}

	void f_class_clean_bin(TypeObject* type) noexcept {
		auto staticData{ static_cast<BinStaticData*>(type->v_static) };
		for (auto obj : staticData->pool) delete obj;
		staticData->pool = Vector<BinObject*>();
	}

	// arg1: Byte* 字节流地址
	// arg2: Size 字节流长度
	Object* f_allocate_bin(TypeObject* type, Memory arg1, Memory arg2) noexcept {
		auto& pool{ static_cast<BinStaticData*>(type->v_static)->pool };
		BinObject* obj;
		if (pool.empty()) obj = new BinObject(type);
		else {
			obj = pool.back();
			pool.pop_back();
		}
		obj->data.assign(arg_recast<Byte*>(arg1), arg_recast<Size>(arg2));
		return obj;
	}

	void f_deallocate_bin(Object* obj) noexcept {
		auto& pool{ static_cast<BinStaticData*>(obj->type->v_static)->pool };
		pool.emplace_back(obj_cast<BinObject>(obj));
	}

	bool f_bool_bin(Object* obj) noexcept {
		return !obj_cast<BinObject>(obj)->data.empty();
	}

	IResult<void> f_string_bin(HVM, Object* obj, String* str) noexcept {
		constexpr auto MAX_BIN_SIZE{ 1024ULL };

		auto bobj{ obj_cast<BinObject>(obj) };
		if (auto size{ bobj->data.size() }; size > MAX_BIN_SIZE) {
			fast_io::u16ostring_ref strRef{ str };
			print(strRef, u"bin[ ", size, u" ]");
		}
		else impl::Bin_ShowBytes(bobj, str);
		return IResult<void>(true);
	}

	IResult<Size> f_hash_bin(HVM, Object* obj) noexcept {
		auto& data{ obj_cast<BinObject>(obj)->data };
		return IResult<Size>(freestanding::cvt::hash_bytes<Size>(data.data(), data.data() + data.size()));
	}

	IResult<Size> f_len_bin(HVM, Object* obj) noexcept {
		return IResult<Size>(obj_cast<BinObject>(obj)->data.size());
	}

	IResult<void> f_member_bin(HVM hvm, bool isLV, Object* obj, const StringView member) noexcept {
		auto vm{ vm_cast(hvm) };
		auto bobj{ obj_cast<BinObject>(obj) };
		if (!isLV) {
			auto strType{ vm->getType(TypeId::String) };
			if (member == u"bytes") {
				auto sobj{ obj_allocate<StringObject>(strType) };
				impl::Bin_ShowBytes(bobj, &sobj->value);
				vm->objectStack.push_link(sobj);
				return IResult<void>(true);
			}
			else if (member == u"utf8") {
				auto sobj{ obj_allocate<StringObject>(strType) };
				platform::Platform_UTF8ToString(&bobj->data, &sobj->value);
				vm->objectStack.push_link(sobj);
				return IResult<void>(true);

			}
			else if (member == u"gb2312") {
				auto sobj{ obj_allocate<StringObject>(strType) };
				platform::Platform_GB2312ToString(&bobj->data, &sobj->value);
				vm->objectStack.push_link(sobj);
				return IResult<void>(true);
			}
			else {
				auto& ft{ static_cast<BinStaticData*>(obj->type->v_static)->ft };
				if (auto pFobj{ ft.get(member) }) {
					auto fobj{ obj_allocate(vm->getType(TypeId::MemberFunction), *pFobj, obj) };
					vm->objectStack.push_link(fobj);
					return IResult<void>(true);
				}
			}
		}
		return SetError(&SetError_UnmatchedMember, vm, obj->type, member);
	}

	constexpr auto BIN_LVID_BYTE{ 0ULL };

	IResult<void> f_index_bin(HVM hvm, bool isLV, Object* obj, ObjArgsView args) noexcept {
		auto vm{ vm_cast(hvm) };
		auto bobj{ obj_cast<BinObject>(obj) };
		if (auto argc{ args.size() }; argc == 1ULL) {
			if (auto arg{ args[0] }; arg->type->v_id == TypeId::Int) {
				auto length{ static_cast<Int64>(bobj->data.size()) };
				auto index{ obj_cast<IntObject>(arg)->value };
				if (index > 0LL && index <= length) {
					auto actualIndex{ static_cast<Index>(index - 1) };
					if (isLV) {
						auto ret{ obj_allocate<LVObject>(vm->getType(TypeId::LV)) };
						ret->parent = obj;
						ret->parent->link();
						ret->setData<BIN_LVID_BYTE>(actualIndex);
						vm->objectStack.push_link(ret);
						return IResult<void>(true);
					}
					else {
						vm->objectStack.push_link(obj_allocate(vm->getType(TypeId::Int),
							arg_cast(static_cast<Int64>(bobj->data[actualIndex]))));
						return IResult<void>(true);
					}
				}
				else SetError_IndexOutOfRange(vm, index, length);
			}
			else SetError_UnmatchedIndex(vm, obj->type->v_name, args);
		}
		else SetError_UnmatchedIndex(vm, obj->type->v_name, args);
		return IResult<void>();
	}

	IResult<void> f_construct_bin(HVM hvm, TypeObject* type, ObjArgsView args) noexcept {
		auto vm{ vm_cast(hvm) };
		if (auto argc{ args.size() }; argc == 0ULL) {
			vm->objectStack.push_link(obj_allocate(type));
			return IResult<void>(true);
		}
		return SetError(&SetError_UnmatchedCall, vm, type->v_name, args);
	}

	Object* f_full_copy_bin(Object* obj) noexcept {
		auto& bin{ obj_cast<BinObject>(obj)->data };
		return obj_allocate(obj->type, arg_cast(bin.data()), arg_cast(bin.size()));
	}

	IResult<void> f_calcassign_bin(HVM hvm, LVObject* lv, Object* obj2, AssignType opt) noexcept {
		auto bobj1{ lv->getAddressObject<BinObject>() };
		if (opt == AssignType::ADD_ASSIGN && obj2->type->v_id == TypeId::Bin) {
			bobj1->data += obj_cast<BinObject>(obj2)->data;
			return IResult<void>(true);
		}
		return SetError(&SetError_IncompatibleCalcAssign, vm_cast(hvm), bobj1->type, obj2->type, opt);
	}

	IResult<void> f_assign_lv_data_bin(HVM hvm, LVObject* lv, Object* obj) noexcept {
		auto vm{ vm_cast(hvm) };
		auto& bin{ lv->getParent<BinObject>()->data };
		if (obj->type->v_id == TypeId::Int) {
			bin[lv->getData<0ULL, Index>()] = static_cast<Byte>(obj_cast<IntObject>(obj)->value);
			return IResult<void>(true);
		}
		return SetError(&SetError_IncompatibleAssign, vm, vm->getType(TypeId::Int), obj->type);
	}

	IResult<void> f_calcassign_lv_data_bin(HVM hvm, LVObject* lv, Object* obj, AssignType opt) noexcept {
		auto vm{ vm_cast(hvm) };
		auto& bin{ lv->getParent<BinObject>()->data };
		if (obj->type->v_id == TypeId::Int) {
			bin[lv->getData<0ULL, Index>()] += static_cast<Byte>(obj_cast<IntObject>(obj)->value);
			return IResult<void>(true);
		}
		return SetError(&SetError_IncompatibleCalcAssign, vm, vm->getType(TypeId::Int), obj->type, opt);
	}

	IResult<void> f_bopt_calc_bin(HVM hvm, Object* obj1, Object* obj2, BOPTType opt) noexcept {
		auto vm{ vm_cast(hvm) };
		if (opt == BOPTType::ADD && obj2->type->v_id == TypeId::Bin) {
			auto ret{ obj_allocate<BinObject>(obj1->type) };
			auto& bin1{ obj_cast<BinObject>(obj1)->data };
			auto& bin2{ obj_cast<BinObject>(obj2)->data };
			ret->data.resize(bin1.size() + bin2.size());
			freestanding::copy_n(ret->data.data(), bin1.data(), bin1.size());
			freestanding::copy_n(ret->data.data() + bin1.size(), bin2.data(), bin2.size());
			vm->objectStack.push_link(ret);
			return IResult<void>(true);
		}
		return SetError(&SetError_UnsupportedBOPT, vm, obj1->type, obj2->type, opt);
	}

	IResult<bool> f_equal_bin(HVM, Object* obj1, Object* obj2) noexcept {
		if (obj2->type->v_id == TypeId::Bin) {
			auto& bin1{ obj_cast<BinObject>(obj1)->data };
			auto& bin2{ obj_cast<BinObject>(obj2)->data };
			return IResult<bool>(bin1.size() == bin2.size() &&
				freestanding::compare(bin1.data(), bin2.data(), bin1.size()) == 0);
		}
		return IResult<bool>(false);
	}
}

namespace hy {
	TypeObject* AddBinPrototype(TypeObject* prototype) noexcept {
		auto type{ new TypeObject(prototype) };
		type->v_id = TypeId::Bin;
		type->v_name = u"bin";

		type->v_static = nullptr;
		type->v_cls = nullptr;
		type->v_cps = nullptr;

		type->a_mutable = true;
		type->a_def = true;
		type->a_unused1 = type->a_unused2 = false;

		type->f_class_create = &f_class_create_bin;
		type->f_class_delete = &f_class_delete_bin;
		type->f_class_clean = &f_class_clean_bin;
		type->f_allocate = &f_allocate_bin;
		type->f_deallocate = &f_deallocate_bin;
		type->f_implement = nullptr;

		type->f_float = nullptr;
		type->f_bool = &f_bool_bin;
		type->f_string = &f_string_bin;

		type->f_hash = &f_hash_bin;
		type->f_len = &f_len_bin;
		type->f_member = &f_member_bin;
		type->f_index = &f_index_bin;
		type->f_unpack = nullptr;

		type->f_construct = &f_construct_bin;
		type->f_full_copy = &f_full_copy_bin;
		type->f_copy = &f_full_copy_bin;
		type->f_write = nullptr;
		type->f_scan = nullptr;
		type->f_calcassign = &f_calcassign_bin;

		type->f_assign_lv_data = &f_assign_lv_data_bin;
		type->f_calcassign_lv_data = &f_calcassign_lv_data_bin;
		type->f_free_lv_data = nullptr;

		type->f_sopt_calc = nullptr;
		type->f_bopt_calc = &f_bopt_calc_bin;
		type->f_equal = &f_equal_bin;
		type->f_compare = nullptr;

		type->f_iter_get = nullptr;
		type->f_iter_save = nullptr;
		type->f_iter_add = nullptr;
		type->f_iter_check = nullptr;
		type->f_iter_free = nullptr;

		return type;
	}
}