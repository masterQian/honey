#include "../hy.vm.impl.h"

#include <fast_io/fast_io.h>

namespace hy::impl {
	IResult<void> String_Concat(VM* vm, String* str, ObjArgsView args, bool newLine) noexcept {
		for (auto arg : args) {
			if (!arg->type->f_string(vm, arg, str)) return IResult<void>();
		}
		if (newLine) str->push_back(u'\n');
		return IResult<void>(true);
	}

	LIB_EXPORT void String_Substr(HVM hvm, ObjArgsView args, Object* thisObject) noexcept {
		auto vm{ vm_cast(hvm) };
		auto sobj{ obj_cast<StringObject>(thisObject) };
		auto len{ static_cast<Int64>(sobj->value.size()) };
		if (auto argc{ args.size() }; argc == 1ULL) {
			if (args[0]->type->v_id == TypeId::Int) {
				auto start{ obj_cast<IntObject>(args[0])->value };
				if (start <= 0LL || start > len) start = 1LL;
				auto ret{ obj_allocate<StringObject>(thisObject->type) };
				ret->value = sobj->value.substr(start - 1LL);
				vm->objectStack.push_link(ret);
				return;
			}
		}
		else if (argc == 2ULL) {
			if (args[0]->type->v_id == TypeId::Int && args[1]->type->v_id == TypeId::Int) {
				auto start{ obj_cast<IntObject>(args[0])->value };
				auto count{ obj_cast<IntObject>(args[1])->value };
				if (start <= 0LL || count < 0LL || start + count > len + 1LL) {
					start = 1LL;
					count = 0LL;
				}
				auto ret{ obj_allocate<StringObject>(thisObject->type) };
				ret->value = sobj->value.substr(start - 1LL, count);
				vm->objectStack.push_link(ret);
				return;
			}
		}
		SetError_UnmatchedCall(vm, u"string::substr", args);
	}

	LIB_EXPORT void String_Left(HVM hvm, ObjArgsView args, Object* thisObject) noexcept {
		auto vm{ vm_cast(hvm) };
		auto& str1{ obj_cast<StringObject>(thisObject)->value };
		auto& str2{ obj_cast<StringObject>(args[0])->value };
		auto ret{ obj_allocate<StringObject>(thisObject->type) };
		ret->value = str1.substr(0, str1.find(str2));
		vm->objectStack.push_link(ret);
	}

	LIB_EXPORT void String_Right(HVM hvm, ObjArgsView args, Object* thisObject) noexcept {
		auto vm{ vm_cast(hvm) };
		auto& str1{ obj_cast<StringObject>(thisObject)->value };
		auto& str2{ obj_cast<StringObject>(args[0])->value };
		auto ret{ obj_allocate<StringObject>(thisObject->type) };
		ret->value = str1.substr(str1.find(str2) + str2.size());
		vm->objectStack.push_link(ret);
	}

	LIB_EXPORT void String_Middle(HVM hvm, ObjArgsView args, Object* thisObject) noexcept {
		auto vm{ vm_cast(hvm) };
		auto& str1{ obj_cast<StringObject>(thisObject)->value };
		auto& str2{ obj_cast<StringObject>(args[0])->value };
		auto& str3{ obj_cast<StringObject>(args[1])->value };
		auto ret{ obj_allocate<StringObject>(thisObject->type) };
		auto l{ str1.find(str2) }, k{ str2.size() };
		if (l != String::npos) {
			auto r{ str1.find(str3, l + k) };
			if (r != String::npos) ret->value = str1.substr(l + k, r - l - k);
		}
		vm->objectStack.push_link(ret);
	}

	LIB_EXPORT void String_Replace(HVM hvm, ObjArgsView args, Object* thisObject) noexcept {
		auto vm{ vm_cast(hvm) };
		auto& str1{ obj_cast<StringObject>(thisObject)->value };
		auto& str2{ obj_cast<StringObject>(args[0])->value };
		auto& str3{ obj_cast<StringObject>(args[1])->value };
		auto ret{ obj_allocate<StringObject>(thisObject->type, arg_cast(str1.data()), arg_cast(str1.size())) };
		for (Index pos{ }; pos != String::npos; pos += str3.size()) {
			if ((pos = ret->value.find(str2, pos)) != String::npos) ret->value.replace(pos, str2.size(), str3);
			else break;
		}
		vm->objectStack.push_link(ret);
	}

	LIB_EXPORT void String_Split(HVM hvm, ObjArgsView args, Object* thisObject) noexcept {
		auto vm{ vm_cast(hvm) };
		auto& str1{ obj_cast<StringObject>(thisObject)->value };
		auto& str2{ obj_cast<StringObject>(args[0])->value };
		auto lastPos{ str1.find_first_not_of(str2, 0ULL) };
		auto pos{ str1.find_first_of(str2, lastPos) };
		auto lobj{ obj_allocate<ListObject>(vm->getType(TypeId::List)) };
		for (; pos != String::npos || lastPos != String::npos;) {
			auto sobj{ obj_allocate<StringObject>(thisObject->type) };
			sobj->value = str1.substr(lastPos, pos - lastPos);
			sobj->link();
			lobj->objects.emplace_back(sobj);
			lastPos = str1.find_first_not_of(str2, pos);
			pos = str1.find_first_of(str2, lastPos);
		}
		vm->objectStack.push_link(lobj);
	}

	LIB_EXPORT void String_Reverse(HVM hvm, ObjArgsView, Object* thisObject) noexcept {
		auto vm{ vm_cast(hvm) };
		auto sobj{ obj_cast<StringObject>(thisObject) };
		auto ret{ obj_allocate<StringObject>(thisObject->type) };
		ret->value.assign(sobj->value.rbegin(), sobj->value.rend());
		vm->objectStack.push_link(ret);
	}

	LIB_EXPORT void String_Strip(HVM hvm, ObjArgsView, Object* thisObject) noexcept {
		auto vm{ vm_cast(hvm) };
		auto& str1{ obj_cast<StringObject>(thisObject)->value };
		auto ret{ obj_allocate<StringObject>(thisObject->type) };
		if (auto len{ str1.size() }) {
			Index left{ }, right{ len - 1ULL };
			while (left < len && freestanding::cvt::isblank(str1[left])) ++left;
			while (right >= left && freestanding::cvt::isblank(str1[right])) --right;
			if (left != len) ret->value = str1.substr(left, right - left + 1ULL);
		}
		vm->objectStack.push_link(ret);
	}

	LIB_EXPORT void String_StartsWith(HVM hvm, ObjArgsView args, Object* thisObject) noexcept {
		auto vm{ vm_cast(hvm) };
		auto& str1{ obj_cast<StringObject>(thisObject)->value };
		auto str2{ obj_cast<StringObject>(args[0])->value };
		vm->objectStack.push_link(obj_allocate(vm->getType(TypeId::Bool),
			arg_cast(static_cast<Int64>(str1.starts_with(str2)))));
	}

	LIB_EXPORT void String_EndsWith(HVM hvm, ObjArgsView args, Object* thisObject) noexcept {
		auto vm{ vm_cast(hvm) };
		auto& str1{ obj_cast<StringObject>(thisObject)->value };
		auto& str2{ obj_cast<StringObject>(args[0])->value };
		vm->objectStack.push_link(obj_allocate(vm->getType(TypeId::Bool),
			arg_cast(static_cast<Int64>(str1.ends_with(str2)))));
	}

	LIB_EXPORT void String_Find(HVM hvm, ObjArgsView args, Object* thisObject) noexcept {
		auto vm{ vm_cast(hvm) };
		auto& str1{ obj_cast<StringObject>(thisObject)->value };
		auto argc{ args.size() };
		auto ok{ false };
		auto start{ 0LL };
		StringView sv;
		if (argc >= 1ULL) {
			if (args[0]->type->v_id == TypeId::String) {
				sv = obj_cast<StringObject>(args[0])->value;
				if (argc == 1ULL) ok = true;
				else if (argc == 2ULL) {
					if (args[1]->type->v_id == TypeId::Int) {
						start = obj_cast<IntObject>(args[1])->value - 1LL;
						if (start < 0LL) start = 0LL;
						ok = true;
					}
				}
			}
		}
		if (ok) {
			auto index{ str1.find(sv, static_cast<Index>(start)) };
			vm->objectStack.push_link(obj_allocate(vm->getType(TypeId::Int),
				arg_cast(index == String::npos ? 0ULL : index + 1ULL)));
		}
		else SetError_UnmatchedCall(vm, u"string::find", args);
	}

	LIB_EXPORT void String_RFind(HVM hvm, ObjArgsView args, Object* thisObject) noexcept {
		auto vm{ vm_cast(hvm) };
		auto& str1{ obj_cast<StringObject>(thisObject)->value };
		auto argc{ args.size() }, len{ str1.size() };
		auto ok{ false };
		auto start{ static_cast<Int64>(len) - 1LL };
		StringView sv;
		if (argc >= 1ULL) {
			if (args[0]->type->v_id == TypeId::String) {
				sv = obj_cast<StringObject>(args[0])->value;
				if (argc == 1ULL) ok = true;
				else if (argc == 2ULL) {
					if (args[1]->type->v_id == TypeId::Int) {
						start = obj_cast<IntObject>(args[1])->value - 1LL;
						if (start < 0LL) start = len - 1LL;
						ok = true;
					}
				}
			}
		}
		if (ok) {
			auto index{ str1.rfind(sv, static_cast<Index>(start)) };
			vm->objectStack.push_link(obj_allocate(vm->getType(TypeId::Int),
				arg_cast(index == String::npos ? 0ULL : index + 1ULL)));
		}
		else SetError_UnmatchedCall(vm, u"string::rfind", args);
	}

	LIB_EXPORT void String_Contains(HVM hvm, ObjArgsView args, Object* thisObject) noexcept {
		auto vm{ vm_cast(hvm) };
		auto& str1{ obj_cast<StringObject>(thisObject)->value };
		auto& str2{ obj_cast<StringObject>(args[0])->value };
		vm->objectStack.push_link(obj_allocate(vm->getType(TypeId::Bool),
			arg_cast(static_cast<Int64>(str1.find(str2) != String::npos))));
	}
	
	LIB_EXPORT void String_Lower(HVM hvm, ObjArgsView, Object* thisObject) noexcept {
		auto vm{ vm_cast(hvm) };
		auto& str{ obj_cast<StringObject>(thisObject)->value };
		auto sobj{ obj_allocate<StringObject>(thisObject->type,
			arg_cast(str.data()), arg_cast(str.size())) };
		for (auto& c : sobj->value) {
			if (c >= u'A' && c <= u'Z') c += 32;
		}
		vm->objectStack.push_link(sobj);
	}

	LIB_EXPORT void String_Upper(HVM hvm, ObjArgsView, Object* thisObject) noexcept {
		auto vm{ vm_cast(hvm) };
		auto& str{ obj_cast<StringObject>(thisObject)->value };
		auto sobj{ obj_allocate<StringObject>(thisObject->type,
			arg_cast(str.data()), arg_cast(str.size())) };
		for (auto& c : sobj->value) {
			if (c >= u'a' && c <= u'z') c -= 32;
		}
		vm->objectStack.push_link(sobj);
	}

	LIB_EXPORT void String_Count(HVM hvm, ObjArgsView args, Object* thisObject) noexcept {
		auto vm{ vm_cast(hvm) };
		auto& str1{ obj_cast<StringObject>(thisObject)->value };
		auto& str2{ obj_cast<StringObject>(args[0])->value };
		auto pos{ 0ULL }, count{ 0ULL };
		while ((pos = str1.find(str2, pos)) != String::npos) {
			++count;
			pos += str2.size();
		}
		vm->objectStack.push_link(obj_allocate(vm->getType(TypeId::Int), arg_cast(count)));
	}
}

namespace hy {
	struct StringStaticData {
		Vector<StringObject*> pool;
		FunctionTable ft;

		StringStaticData(TypeObject* type) noexcept {
			auto funcType{ type->__getType(TypeId::Function) };

			Object* __any[] { type, type };
			ObjArgsView empty, st1{ __any, 1ULL }, st2{ __any, 2ULL };

			ft.try_emplace(u"substr", MakeNative<false, true>(funcType, u"string::substr", &impl::String_Substr, empty));
			ft.try_emplace(u"left", MakeNative<true, true>(funcType, u"string::left", &impl::String_Left, st1));
			ft.try_emplace(u"right", MakeNative<true, true>(funcType, u"string::right", &impl::String_Right, st1));
			ft.try_emplace(u"middle", MakeNative<true, true>(funcType, u"string::middle", &impl::String_Middle, st2));
			ft.try_emplace(u"replace", MakeNative<true, true>(funcType, u"string::replace", &impl::String_Replace, st2));
			ft.try_emplace(u"split", MakeNative<true, true>(funcType, u"string::split", &impl::String_Split, st1));
			ft.try_emplace(u"reverse", MakeNative<true, true>(funcType, u"string::reverse", &impl::String_Reverse, empty));
			ft.try_emplace(u"strip", MakeNative<true, true>(funcType, u"string::strip", &impl::String_Strip, empty));
			ft.try_emplace(u"startsWith", MakeNative<true, true>(funcType, u"string::startsWith", &impl::String_StartsWith, st1));
			ft.try_emplace(u"endsWith", MakeNative<true, true>(funcType, u"string::endsWith", &impl::String_EndsWith, st1));
			ft.try_emplace(u"find", MakeNative<false, true>(funcType, u"string::find", &impl::String_Find, empty));
			ft.try_emplace(u"rfind", MakeNative<false, true>(funcType, u"string::rfind", &impl::String_RFind, empty));
			ft.try_emplace(u"contains", MakeNative<true, true>(funcType, u"string::contains", &impl::String_Contains, st1));
			ft.try_emplace(u"lower", MakeNative<true, true>(funcType, u"string::lower", &impl::String_Lower, empty));
			ft.try_emplace(u"upper", MakeNative<true, true>(funcType, u"string::upper", &impl::String_Upper, empty));
			ft.try_emplace(u"count", MakeNative<true, true>(funcType, u"string::count", &impl::String_Count, st1));
		}
	};

	void f_class_create_string(TypeObject* type) noexcept {
		type->v_static = new StringStaticData{ type };
	}

	void f_class_delete_string(TypeObject* type) noexcept {
		auto staticData{ static_cast<StringStaticData*>(type->v_static) };
		for (auto obj : staticData->pool) delete obj;
		for (auto& [_, fobj] : staticData->ft) delete fobj;
		delete staticData;
	}

	void f_class_clean_string(TypeObject* type) noexcept {
		auto staticData{ static_cast<StringStaticData*>(type->v_static) };
		for (auto obj : staticData->pool) delete obj;
		staticData->pool = Vector<StringObject*>();
	}

	// arg1: Char* 字符串地址(可空)
	// arg2: Size 字符串长度
	Object* f_allocate_string(TypeObject* type, Memory arg1, Memory arg2) noexcept {
		auto& pool{ static_cast<StringStaticData*>(type->v_static)->pool };
		StringObject* obj;
		if (pool.empty()) obj = new StringObject{ type };
		else {
			obj = pool.back();
			pool.pop_back();
		}
		obj->value.clear();
		if (arg2) {
			if (arg1) obj->value.assign(arg_recast<Str>(arg1), arg_recast<Size>(arg2));
			else obj->value.resize(arg_recast<Size>(arg2));
		}
		return obj;
	}

	void f_deallocate_string(Object* obj) noexcept {
		auto& pool{ static_cast<StringStaticData*>(obj->type->v_static)->pool };
		pool.emplace_back(obj_cast<StringObject>(obj));
	}

	bool f_bool_string(Object* obj) noexcept {
		return !obj_cast<StringObject>(obj)->value.empty();
	}

	IResult<void> f_string_string(HVM, Object* obj, String* str) noexcept {
		str->append(obj_cast<StringObject>(obj)->value);
		return IResult<void>(true);
	}

	IResult<Size> f_hash_string(HVM, Object* obj) noexcept {
		return IResult<Size>(std::hash<String>{}(obj_cast<StringObject>(obj)->value));
	}

	IResult<Size> f_len_string(HVM, Object* obj) noexcept {
		return IResult<Size>(obj_cast<StringObject>(obj)->value.length());
	}

	IResult<void> f_member_string(HVM hvm, bool isLV, Object* obj, const StringView member) noexcept {
		auto vm{ vm_cast(hvm) };
		auto sobj{ obj_cast<StringObject>(obj) };
		if (!isLV) {
			auto binType{ vm->getType(TypeId::Bin) };
			if (member == u"utf8") {
				auto bobj{ obj_allocate<BinObject>(binType) };
				platform::Platform_StringToUTF8(&sobj->value, &bobj->data);
				vm->objectStack.push_link(bobj);
				return IResult<void>(true);
			}
			else if (member == u"gb2312") {
				auto bobj{ obj_allocate<BinObject>(binType) };
				platform::Platform_StringToGB2312(&sobj->value, &bobj->data);
				vm->objectStack.push_link(bobj);
				return IResult<void>(true);
			}
			else {
				auto& ft{ static_cast<StringStaticData*>(obj->type->v_static)->ft };
				if (auto pFobj{ ft.get(member) }) {
					auto fobj{ obj_allocate(vm->getType(TypeId::MemberFunction), *pFobj, obj) };
					vm->objectStack.push_link(fobj);
					return IResult<void>(true);
				}
			}
		}
		return SetError(&SetError_UnmatchedMember, vm, obj->type, member);
	}

	constexpr auto STRING_LVID_CHAR{ 0ULL };

	IResult<void> f_index_string(HVM hvm, bool isLV, Object* obj, ObjArgsView args) noexcept {
		auto vm{ vm_cast(hvm) };
		auto sobj{ obj_cast<StringObject>(obj) };
		if (auto argc{ args.size() }; argc == 1ULL) {
			if (auto arg{ args[0] }; arg->type->v_id == TypeId::Int) {
				auto length{ static_cast<Int64>(sobj->value.length()) };
				auto index{ obj_cast<IntObject>(arg)->value };
				if (index > 0LL && index <= length) {
					auto actualIndex{ static_cast<Index>(index - 1) };
					if (isLV) {
						auto ret{ obj_allocate<LVObject>(vm->getType(TypeId::LV)) };
						ret->parent = obj;
						ret->parent->link();
						ret->setData<STRING_LVID_CHAR>(actualIndex);
						vm->objectStack.push_link(ret);
						return IResult<void>(true);
					}
					else {
						vm->objectStack.push_link(obj_allocate(vm->getType(TypeId::Int),
							arg_cast(static_cast<Int64>(sobj->value[actualIndex]))));
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

	IResult<void> f_construct_string(HVM hvm, TypeObject* type, ObjArgsView args) noexcept {
		auto vm{ vm_cast(hvm) };
		if (auto argc{ args.size() }; argc == 0ULL) {
			vm->objectStack.push_link(obj_allocate(type));
			return IResult<void>(true);
		}
		else if (argc == 1ULL) {
			if (auto arg{ args[0] }; arg->type->v_id == TypeId::String) {
				auto sobj{ obj_cast<StringObject>(arg) };
				vm->objectStack.push_link(obj_allocate(type, arg_cast(sobj->value.data()), arg_cast(sobj->value.size())));
				return IResult<void>(true);
			}
		}
		return SetError(&SetError_UnmatchedCall, vm, type->v_name, args);
	}

	Object* f_full_copy_string(Object* obj) noexcept {
		auto& str{ obj_cast<StringObject>(obj)->value };
		return obj_allocate(obj->type, arg_cast(str.data()), arg_cast(str.size()));
	}

	IResult<void> f_write_string(HVM hvm, Object* obj, ObjArgsView args) noexcept {
		return impl::String_Concat(vm_cast(hvm), &obj_cast<StringObject>(obj)->value, args, false);
	}

	IResult<Size> f_scan_string(HVM hvm, TypeObject* type, const StringView str) noexcept {
		auto vm{ vm_cast(hvm) };
		vm->objectStack.push_link(obj_allocate(type, arg_cast(str.data()), arg_cast(str.size())));
		return IResult<Size>(str.size());
	}

	IResult<void> f_calcassign_string(HVM hvm, LVObject* lv, Object* obj2, AssignType opt) noexcept {
		auto sobj1{ lv->getAddressObject<StringObject>() };
		if (opt == AssignType::ADD_ASSIGN && obj2->type->v_id == TypeId::String) {
			sobj1->value += obj_cast<StringObject>(obj2)->value;
			return IResult<void>(true);
		}
		return SetError(&SetError_IncompatibleCalcAssign, vm_cast(hvm), sobj1->type, obj2->type, opt);
	}

	IResult<void> f_assign_lv_data_string(HVM hvm, LVObject* lv, Object* obj) noexcept {
		auto vm{ vm_cast(hvm) };
		auto& str{ lv->getParent<StringObject>()->value };
		if (obj->type->v_id == TypeId::Int) {
			str[lv->getData<0ULL, Index>()] = static_cast<Char>(obj_cast<IntObject>(obj)->value);
			return IResult<void>(true);
		}
		return SetError(&SetError_IncompatibleAssign, vm, vm->getType(TypeId::Int), obj->type);
	}

	IResult<void> f_calcassign_lv_data_string(HVM hvm, LVObject* lv, Object* obj, AssignType opt) noexcept {
		auto vm{ vm_cast(hvm) };
		auto& str{ lv->getParent<StringObject>()->value };
		if (obj->type->v_id == TypeId::Int) {
			str[lv->getData<0ULL, Index>()] += static_cast<Char>(obj_cast<IntObject>(obj)->value);
			return IResult<void>(true);
		}
		return SetError(&SetError_IncompatibleCalcAssign, vm, vm->getType(TypeId::Int), obj->type, opt);
	}

	IResult<void> f_bopt_calc_string(HVM hvm, Object* obj1, Object* obj2, BOPTType opt) noexcept {
		auto vm{ vm_cast(hvm) };
		if (opt == BOPTType::ADD && obj2->type->v_id == TypeId::String) {
			auto ret{ obj_allocate<StringObject>(obj1->type) };
			print(fast_io::u16ostring_ref{ &ret->value }, obj_cast<StringObject>(obj1)->value,
				obj_cast<StringObject>(obj2)->value);
			vm->objectStack.push_link(ret);
			return IResult<void>(true);
		}
		return SetError(&SetError_UnsupportedBOPT, vm, obj1->type, obj2->type, opt);
	}

	IResult<bool> f_equal_string(HVM, Object* obj1, Object* obj2) noexcept {
		if (obj2->type->v_id == TypeId::String)
			return IResult<bool>(obj_cast<StringObject>(obj1)->value == obj_cast<StringObject>(obj2)->value);
		return IResult<bool>(false);
	}

	IResult<bool> f_compare_string(HVM hvm, Object* obj1, Object* obj2, BOPTType opt) noexcept {
		if (obj2->type->v_id == TypeId::String) {
			auto sobj1{ obj_cast<StringObject>(obj1) };
			auto sobj2{ obj_cast<StringObject>(obj2) };
			auto ret{ false };
			switch (opt) {
			case BOPTType::GT: ret = sobj1->value > sobj2->value; break;
			case BOPTType::GE: ret = sobj1->value >= sobj2->value; break;
			case BOPTType::LT: ret = sobj1->value < sobj2->value; break;
			case BOPTType::LE: ret = sobj1->value <= sobj2->value; break;
			}
			return IResult<bool>(ret);
		}
		return SetError<bool>(&SetError_UnsupportedBOPT, vm_cast(hvm), obj1->type, obj2->type, opt);
	}
}

namespace hy {
	TypeObject* AddStringPrototype(TypeObject* prototype) noexcept {
		auto type{ new TypeObject(prototype) };
		type->v_id = TypeId::String;
		type->v_name = u"string";
		
		type->v_static = nullptr;
		type->v_cls = nullptr;
		type->v_cps = nullptr;

		type->a_mutable = true;
		type->a_def = true;
		type->a_unused1 = type->a_unused2 = false;

		type->f_class_create = &f_class_create_string;
		type->f_class_delete = &f_class_delete_string;
		type->f_class_clean = &f_class_clean_string;
		type->f_allocate = &f_allocate_string;
		type->f_deallocate = &f_deallocate_string;
		type->f_implement = nullptr;

		type->f_float = nullptr;
		type->f_bool = &f_bool_string;
		type->f_string = &f_string_string;

		type->f_hash = &f_hash_string;
		type->f_len = &f_len_string;
		type->f_member = &f_member_string;
		type->f_index = &f_index_string;
		type->f_unpack = nullptr;

		type->f_construct = &f_construct_string;
		type->f_full_copy = &f_full_copy_string;
		type->f_copy = &f_full_copy_string;
		type->f_write = &f_write_string;
		type->f_scan = &f_scan_string;
		type->f_calcassign = &f_calcassign_string;

		type->f_assign_lv_data = &f_assign_lv_data_string;
		type->f_calcassign_lv_data = &f_calcassign_lv_data_string;
		type->f_free_lv_data = nullptr;

		type->f_sopt_calc = nullptr;
		type->f_bopt_calc = &f_bopt_calc_string;
		type->f_equal = &f_equal_string;
		type->f_compare = &f_compare_string;

		type->f_iter_get = nullptr;
		type->f_iter_save = nullptr;
		type->f_iter_add = nullptr;
		type->f_iter_check = nullptr;
		type->f_iter_free = nullptr;

		return type;
	}
}