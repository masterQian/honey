#include "../hy.vm.impl.h"

namespace hy {
	struct ObjectStaticData {
		Vector<ObjectObject*> pool;
	};
}

namespace hy::impl {
	inline ObjectObject* Object_Allocate(TypeObject* type) noexcept {
		auto& pool{ static_cast<ObjectStaticData*>(type->v_static)->pool };
		ObjectObject* oobj;
		if (pool.empty()) {
			oobj = new ObjectObject(type);
			oobj->membersData.resize(type->v_cls->members.size());
		}
		else {
			oobj = pool.back();
			pool.pop_back();
		}
		return oobj;
	}

	inline void Object_Initialize(ObjectObject* oobj) noexcept {
		for (auto& [memberName, member] : oobj->type->v_cls->members) {
			auto memberObject{ obj_allocate(member.type) };
			memberObject->link();
			oobj->membersData[member.index] = memberObject;
		}
	}

	inline FunctionObject* Object_GetMemberFunction(TypeObject* type, const StringView name, TypeObject* ret) noexcept {
		if (auto pFobj{ type->v_cls->funcs.get(name) }) {
			auto fobj{ *pFobj };
			if (!ret || ret == fobj->data.normal.ret) return fobj;
		}
		return nullptr;
	}

	inline Index Object_GetMemberIndex(TypeObject* type, const StringView name) noexcept {
		if (auto member{ type->v_cls->members.get(name) }) return member->index;
		return INone64;
	}

	inline bool Object_CanCopy(TypeObject* type) noexcept {
		for (auto& [_, member] : type->v_cls->members) {
			if (member.type->a_mutable && !member.type->f_copy) return false;
		}
		return true;
	}
}

namespace hy {
	void f_class_create_object(TypeObject* type) noexcept {
		type->v_static = new ObjectStaticData;
	}

	void f_class_delete_object(TypeObject* type) noexcept {
		auto staticData{ static_cast<ObjectStaticData*>(type->v_static) };
		for (auto obj : staticData->pool) delete obj;
		delete staticData;
	}

	void f_class_clean_object(TypeObject* type) noexcept {
		auto staticData{ static_cast<ObjectStaticData*>(type->v_static) };
		for (auto obj : staticData->pool) delete obj;
		staticData->pool = Vector<ObjectObject*>();
	}

	Object* f_allocate_object(TypeObject* type, Memory, Memory) noexcept {
		auto oobj{ impl::Object_Allocate(type) };
		impl::Object_Initialize(oobj);
		return oobj;
	}

	void f_deallocate_object(Object* obj) noexcept {
		auto& pool{ static_cast<ObjectStaticData*>(obj->type->v_static)->pool };
		auto oobj{ static_cast<ObjectObject*>(obj) };
		for (auto member : oobj->membersData) member->unlink();
		pool.emplace_back(oobj);
	}

	IResult<void> f_string_object(HVM hvm, Object* obj, String* str) noexcept {
		auto vm{ vm_cast(hvm) };
		auto cstCount{ vm->callStack.size() };
		auto type{ obj->type };
		auto fobj{ impl::Object_GetMemberFunction(type, UserClassFunctionString, vm->getType(TypeId::String)) };
		if (!CallFunction(vm, fobj, ObjArgsView{ }, obj)) return IResult<void>();
		if (!RunCallStack(vm, cstCount)) return IResult<void>();
		auto ret{ obj_cast<StringObject>(vm->objectStack.pop_normal()) };
		str->append(ret->value);
		ret->unlink();
		return IResult<void>(true);
	}

	IResult<void> f_string_object_default(HVM hvm, Object* obj, String* str) noexcept {
		auto vm{ vm_cast(hvm) };
		String tmp; 
		tmp.push_back(u'{');
		auto oobj{ obj_cast<ObjectObject>(obj) };
		for (auto& [key, member] : obj->type->v_cls->members) {
			tmp += key;
			tmp.push_back(u':');
			auto item{ oobj->membersData[member.index] };
			if (!item->type->f_string(vm, item, &tmp)) return IResult<void>();
			tmp.push_back(u',');
		}
		if (tmp.size() == 1ULL) tmp.push_back(u'}');
		else tmp.back() = u'}';
		str->append(tmp);
		return IResult<void>(true);
	}

	IResult<Size> f_hash_object(HVM hvm, Object* obj) noexcept {
		auto vm{ vm_cast(hvm) };
		auto cstCount{ vm->callStack.size() };
		auto type{ obj->type };
		auto fobj{ impl::Object_GetMemberFunction(type, UserClassFunctionHash, vm->getType(TypeId::Int)) };
		if (!CallFunction(vm, fobj, ObjArgsView{ }, obj)) return IResult<Size>();
		if (!RunCallStack(vm, cstCount)) return IResult<Size>();
		auto ret{ obj_cast<IntObject>(vm->objectStack.pop_normal()) };
		auto hash{ static_cast<Size>(ret->value) };
		ret->unlink();
		return IResult<Size>(hash);
	}

	IResult<Size> f_len_object(HVM hvm, Object* obj) noexcept {
		auto vm{ vm_cast(hvm) };
		auto cstCount{ vm->callStack.size() };
		auto type{ obj->type };
		auto fobj{ impl::Object_GetMemberFunction(type, UserClassFunctionLen, vm->getType(TypeId::Int)) };
		if (!CallFunction(vm, fobj, ObjArgsView{ }, obj)) return IResult<Size>();
		if (!RunCallStack(vm, cstCount)) return IResult<Size>();
		auto ret{ obj_cast<IntObject>(vm->objectStack.pop_normal()) };
		auto len{ static_cast<Size>(ret->value) };
		ret->unlink();
		return IResult<Size>(len);
	}

	IResult<void> f_member_object(HVM hvm, bool isLV, Object* obj, const StringView member) noexcept {
		auto vm{ vm_cast(hvm) };
		auto& ost{ vm->objectStack };
		auto type{ obj->type };
		auto oobj{ obj_cast<ObjectObject>(obj) };
		// 查找成员变量
		if (auto index{ impl::Object_GetMemberIndex(type, member) }; index != INone64) {
			if (isLV) {
				auto lv{ obj_allocate<LVObject>(vm->getType(TypeId::LV)) };
				lv->parent = obj;
				lv->parent->link();
				lv->lvType = LVType::ADDRESS;
				lv->storage.address = &oobj->membersData[index];
				ost.push_link(lv);
			}
			else ost.push_link(oobj->membersData[index]);
			return IResult<void>(true);
		}
		// 查找成员函数
		if (auto fobj{ impl::Object_GetMemberFunction(type, member, nullptr) }) {
			if (!isLV) {
				auto mfobj{ obj_allocate(vm->getType(TypeId::MemberFunction), fobj, obj) };
				ost.push_link(mfobj);
				return IResult<void>(true);
			}
			else return SetError(&SetError_NotLeftValue, vm, member);
		}
		else return SetError(&SetError_UnmatchedMember, vm, type, member);
	}

	IResult<void> f_index_object(HVM hvm, bool isLV, Object* obj, ObjArgsView args) noexcept {
		auto vm{ vm_cast(hvm) };
		if (isLV) return SetError(&SetError_NotLeftValue, vm, u"");
		auto fobj{ impl::Object_GetMemberFunction(obj->type, UserClassFunctionIndex, nullptr) };
		return CallFunction(vm, fobj, args, obj);
	}

	IResult<void> f_unpack_object(HVM hvm, Object* obj, Size argc) noexcept {
		auto vm{ vm_cast(hvm) };
		auto cstCount{ vm->callStack.size() };
		auto type{ obj->type };
		auto fobj{ impl::Object_GetMemberFunction(type, UserClassFunctionUnpack, vm->getType(TypeId::List)) };
		if (!CallFunction(vm, fobj, ObjArgsView{ }, obj)) return IResult<void>();
		if (!RunCallStack(vm, cstCount)) return IResult<void>();
		auto ret{ obj_cast<ListObject>(vm->objectStack.pop_normal()) };
		auto ir{ ret->type->f_unpack(vm, ret, argc) };
		ret->unlink();
		return ir;
	}

	IResult<void> f_construct_object(HVM hvm, TypeObject* type, ObjArgsView args) noexcept {
		auto vm{ vm_cast(hvm) };
		if (auto fobj{ impl::Object_GetMemberFunction(type, UserClassFunctionConstruct, vm->getType(TypeId::Null)) }) {
			auto obj{ obj_allocate(type) };
			vm->objectStack.push_link(obj);
			auto cstCount{ vm->callStack.size() };
			if (!CallFunction(vm, fobj, args, obj)) return IResult<void>();
			if (!RunCallStack(vm, cstCount)) return IResult<void>();
			vm->objectStack.pop_unlink(); // 弹出__construct__返回的null
		}
		else if (args.empty()) vm->objectStack.push_link(obj_allocate(type));
		else return SetError(&SetError_UnmatchedCall, vm, type->v_name, args);
		return IResult<void>(true);
	}

	Object* f_full_copy_object(Object* obj) noexcept {
		auto oobj{ obj_cast<ObjectObject>(obj) };
		auto clone{ impl::Object_Allocate(oobj->type) };
		auto& objData{ oobj->membersData };
		auto& cloneData{ clone->membersData };
		for (auto i{ 0ULL }, count{ objData.size() }; i < count; ++i) {
			auto item{ objData[i] };
			if (item->type->f_full_copy) item = item->type->f_full_copy(item);
			item->link();
			cloneData[i] = item;
		}
		return clone;
	}

	Object* f_copy_object(Object* obj) noexcept {
		auto oobj{ obj_cast<ObjectObject>(obj) };
		auto clone{ impl::Object_Allocate(oobj->type) };
		auto& objData{ oobj->membersData };
		auto& cloneData{ clone->membersData };
		for (auto i{ 0ULL }, count{ objData.size() }; i < count; ++i) {
			auto item{ objData[i] };
			if (item->type->a_mutable) item = item->type->f_copy(item);
			item->link();
			cloneData[i] = item;
		}
		return clone;
	}

	IResult<void> f_write_object(HVM hvm, Object* obj, ObjArgsView args) noexcept {
		auto vm{ vm_cast(hvm) };
		auto cstCount{ vm->callStack.size() };
		auto  type{ obj->type };
		auto fobj{ impl::Object_GetMemberFunction(type, UserClassFunctionWrite, vm->getType(TypeId::Null)) };
		if (!CallFunction(vm, fobj, args, obj)) return IResult<void>();
		if (!RunCallStack(vm, cstCount)) return IResult<void>();
		vm->objectStack.pop_unlink(); // 弹出__write__返回的null
		return IResult<void>(true);
	}

	IResult<void> f_calcassign_object(HVM hvm, LVObject* lv, Object* obj2, AssignType opt) noexcept {
		auto vm{ vm_cast(hvm) };
		auto oobj1{ lv->getAddressObject<ObjectObject>() };
		auto type{ oobj1->type };
		auto funcName{ UserClassFunctionAddAssign };
		switch (opt) {
		case AssignType::ADD_ASSIGN: funcName = UserClassFunctionAddAssign; break;
		case AssignType::SUBTRACT_ASSIGN: funcName = UserClassFunctionSubAssign; break;
		case AssignType::MULTIPLE_ASSIGN: funcName = UserClassFunctionMulAssign; break;
		case AssignType::DIVIDE_ASSIGN: funcName = UserClassFunctionDivAssign; break;
		case AssignType::MOD_ASSIGN: funcName = UserClassFunctionModAssign; break;
		}
		if (auto fobj{ impl::Object_GetMemberFunction(type, funcName, vm->getType(TypeId::Null)) }) {
			auto cstCount{ vm->callStack.size() };
			Object* args[] { obj2 };
			if (!CallFunction(vm, fobj, ObjArgsView{ args, 1ULL }, oobj1)) return IResult<void>();
			if (!RunCallStack(vm, cstCount)) return IResult<void>();
			vm->objectStack.pop_unlink(); // 弹出__calcassign__返回的null
			return IResult<void>(true);
		}
		return SetError(&SetError_IncompatibleCalcAssign, vm, oobj1->type, obj2->type, opt);
	}

	IResult<void> f_sopt_calc_object(HVM hvm, Object* obj, SOPTType opt) noexcept {
		auto vm{ vm_cast(hvm) };
		auto type{ obj->type };
		auto funcName{ UserClassFunctionNeg };
		switch (opt) {
		case SOPTType::NEGATIVE: funcName = UserClassFunctionNeg; break;
		}
		if (auto fobj{ impl::Object_GetMemberFunction(type, funcName, nullptr) })
			return CallFunction(vm, fobj, ObjArgsView{ }, obj);
		return SetError(&SetError_UnsupportedSOPT, vm, obj->type, opt);
	}

	IResult<void> f_bopt_calc_object(HVM hvm, Object* obj1, Object* obj2, BOPTType opt) noexcept {
		auto vm{ vm_cast(hvm) };
		auto type{ obj1->type };
		auto funcName{ UserClassFunctionAdd };
		switch (opt) {
		case BOPTType::ADD: funcName = UserClassFunctionAdd; break;
		case BOPTType::SUBTRACT: funcName = UserClassFunctionSub; break;
		case BOPTType::MULTIPLE: funcName = UserClassFunctionMul; break;
		case BOPTType::DIVIDE: funcName = UserClassFunctionDiv; break;
		case BOPTType::MOD: funcName = UserClassFunctionMod; break;
		case BOPTType::POWER: funcName = UserClassFunctionPow; break;
		}
		if (auto fobj{ impl::Object_GetMemberFunction(type, funcName, nullptr) }) {
			Object* args[] { obj2 };
			return CallFunction(vm, fobj, ObjArgsView{ args, 1ULL }, obj1);
		}
		return SetError(&SetError_UnsupportedBOPT, vm, obj1->type, obj2->type, opt);
	}

	IResult<bool> f_equal_object(HVM hvm, Object* obj1, Object* obj2) noexcept {
		auto vm{ vm_cast(hvm) };
		auto cstCount{ vm->callStack.size() };
		auto type{ obj1->type };
		auto fobj{ impl::Object_GetMemberFunction(type, UserClassFunctionEqual, vm->getType(TypeId::Bool)) };
		Object* args[] { obj2 };
		if (!CallFunction(vm, fobj, ObjArgsView{ args, 1ULL }, obj1)) return IResult<bool>();
		if (!RunCallStack(vm, cstCount)) return IResult<bool>();
		auto ret{ obj_cast<BoolObject>(vm->objectStack.pop_normal()) };
		auto isEqual{ ret->value };
		ret->unlink();
		return IResult<bool>(isEqual);
	}

	IResult<bool> f_compare_object(HVM hvm, Object* obj1, Object* obj2, BOPTType opt) noexcept {
		auto vm{ vm_cast(hvm) };
		auto type{ obj1->type };
		auto funcName{ UserClassFunctionGT };
		switch (opt) {
		case BOPTType::GT: funcName = UserClassFunctionGT; break;
		case BOPTType::GE: funcName = UserClassFunctionGE; break;
		case BOPTType::LT: funcName = UserClassFunctionLT; break;
		case BOPTType::LE: funcName = UserClassFunctionLE; break;
		}
		if (auto fobj{ impl::Object_GetMemberFunction(type, funcName, vm->getType(TypeId::Bool)) }) {
			auto cstCount{ vm->callStack.size() };
			Object* args[] { obj2 };
			if (!CallFunction(vm, fobj, ObjArgsView{ args, 1ULL }, obj1)) return IResult<bool>();
			if (!RunCallStack(vm, cstCount)) return IResult<bool>();
			auto ret{ obj_cast<BoolObject>(vm->objectStack.pop_normal()) };
			auto cmp{ ret->value };
			ret->unlink();
			return IResult<bool>(cmp);
		}
		return SetError<bool>(&SetError_UnsupportedBOPT, vm, obj1->type, obj2->type, opt);
	}

	constexpr auto OBJECT_ITER_DEFAULT{ 0ULL };

	IResult<void> f_iter_get_object(HVM hvm, Object* obj) noexcept {
		auto vm{ vm_cast(hvm) };
		auto type{ obj->type };
		auto fobj{ impl::Object_GetMemberFunction(type, UserClassFunctionIterGet, nullptr) };
		auto cstCount{ vm->callStack.size() };
		if (!CallFunction(vm, fobj, ObjArgsView{ }, obj)) return IResult<void>();
		if (!RunCallStack(vm, cstCount)) return IResult<void>();
		auto  iterObj{ vm->objectStack.pop_normal() };
		auto iter{ obj_allocate<IteratorObject>(vm->getType(TypeId::Iterator)) };
		iter->ref = obj;
		iter->ref->link();
		iter->setData<OBJECT_ITER_DEFAULT>(iterObj);
		vm->objectStack.push_link(iter);
		return IResult<void>(true);
	}

	IResult<void> f_iter_save_object(HVM hvm, IteratorObject* iter, Size argc) noexcept {
		auto vm{ vm_cast(hvm) };
		auto obj{ iter->getReference<Object>() };
		auto type{ obj->type };
		auto fobj{ impl::Object_GetMemberFunction(type, UserClassFunctionIterSave, vm->getType(TypeId::List)) };
		auto cstCount{ vm->callStack.size() };
		Object* args[] { iter->getData<0ULL, Object*>() };
		if (!CallFunction(vm, fobj, ObjArgsView{ args, 1ULL }, obj)) return IResult<void>();
		if (!RunCallStack(vm, cstCount)) return IResult<void>();
		auto ret{ obj_cast<ListObject>(vm->objectStack.pop_normal()) };
		auto count{ ret->objects.size() };
		if (argc == count) ret->type->f_unpack(vm, ret, argc);
		else SetError_UnmatchedUnpack(vm, count, argc);
		ret->unlink();
		return IResult<void>(vm->ok());
	}

	IResult<void> f_iter_add_object(HVM hvm, IteratorObject* iter) noexcept {
		auto vm{ vm_cast(hvm) };
		auto obj{ iter->getReference<Object>() };
		auto type{ obj->type };
		auto iterObj{ iter->getData<0ULL, Object*>() };
		auto fobj{ impl::Object_GetMemberFunction(type, UserClassFunctionIterAdd, iterObj->type) };
		auto cstCount{ vm->callStack.size() };
		Object* args[] { iterObj };
		if (!CallFunction(vm, fobj, ObjArgsView{ args, 1ULL }, obj)) return IResult<void>();
		if (!RunCallStack(vm, cstCount)) return IResult<void>();
		iter->resetData<0ULL>(vm->objectStack.pop_normal());
		iterObj->unlink();
		return IResult<void>(true);
	}

	IResult<bool> f_iter_check_object(HVM hvm, IteratorObject* iter) noexcept {
		auto vm{ vm_cast(hvm) };
		auto obj{ iter->getReference<Object>() };
		auto type{ obj->type };
		auto iterObj{ iter->getData<0ULL, Object*>() };
		auto fobj{ impl::Object_GetMemberFunction(type, UserClassFunctionIterCheck, vm->getType(TypeId::Bool)) };
		auto cstCount{ vm->callStack.size() };
		Object* args[] { iterObj };
		if (!CallFunction(vm, fobj, ObjArgsView{ args, 1ULL }, obj)) return IResult<bool>();
		if (!RunCallStack(vm, cstCount)) return IResult<bool>();
		auto bobj{ obj_cast<BoolObject>(vm->objectStack.pop_normal()) };
		auto checkResult{ bobj->value };
		bobj->unlink();
		return IResult<bool>(checkResult);
	}

	void f_iter_free_object(IteratorObject* iter) noexcept {
		iter->getData<0ULL, Object*>()->unlink();
	}
}

namespace hy {
	TypeObject* AddObjectPrototype(VM* vm, const StringView name) noexcept {
		auto type{ new TypeObject(vm->getType(TypeId::Type)) };
		
		type->v_name = name;
		type->makeUserClassHash();

		type->v_static = nullptr;
		type->v_cls = nullptr;
		type->v_cps = nullptr;

		type->a_mutable = true;
		type->a_def = true;
		type->a_unused1 = type->a_unused2 = false;

		type->f_class_create = &f_class_create_object;
		type->f_class_delete = &f_class_delete_object;
		type->f_class_clean = &f_class_clean_object;
		type->f_allocate = &f_allocate_object;
		type->f_deallocate = &f_deallocate_object;
		type->f_implement = nullptr;

		type->f_class_create(type);

		return type;
	}

	void SetObjectPrototype(VM* vm, TypeObject* type, TypeClassStruct* classStruct) noexcept {
		type->v_cls = classStruct;

		type->f_float = nullptr;
		type->f_bool = nullptr;

		// 检查__string__合法性
		if (impl::Object_GetMemberFunction(type, UserClassFunctionString, vm->getType(TypeId::String)))
			type->f_string = &f_string_object;
		else type->f_string = &f_string_object_default;

		// 检查__hash__合法性
		if (impl::Object_GetMemberFunction(type, UserClassFunctionHash, vm->getType(TypeId::Int)))
			type->f_hash = &f_hash_object;
		else type->f_hash = nullptr;

		// 检查__len__合法性
		if (impl::Object_GetMemberFunction(type, UserClassFunctionLen, vm->getType(TypeId::Int)))
			type->f_len = &f_len_object;
		else type->f_len = nullptr;

		type->f_member = &f_member_object;

		// 检查__index__合法性
		if (impl::Object_GetMemberFunction(type, UserClassFunctionIndex, nullptr))
			type->f_index = &f_index_object;
		else type->f_index = nullptr;

		// 检查__unpack__合法性
		if (impl::Object_GetMemberFunction(type, UserClassFunctionUnpack, vm->getType(TypeId::List)))
			type->f_unpack = &f_unpack_object;
		else type->f_unpack = nullptr;

		type->f_construct = &f_construct_object;
		type->f_full_copy = &f_full_copy_object;

		// 检查copy合理性
		if (impl::Object_CanCopy(type)) type->f_copy = &f_copy_object;
		else type->f_copy = nullptr;

		// 检查__write__合法性
		if (impl::Object_GetMemberFunction(type, UserClassFunctionWrite, vm->getType(TypeId::Null)))
			type->f_write = &f_write_object;
		else type->f_write = nullptr;

		type->f_scan = nullptr;

		type->f_calcassign = &f_calcassign_object;

		type->f_assign_lv_data = nullptr;
		type->f_calcassign_lv_data = nullptr;
		type->f_free_lv_data = nullptr;

		type->f_sopt_calc = &f_sopt_calc_object;
		type->f_bopt_calc = &f_bopt_calc_object;

		// 检查__equal__合法性
		if (impl::Object_GetMemberFunction(type, UserClassFunctionEqual, vm->getType(TypeId::Bool)))
			type->f_equal = &f_equal_object;
		else type->f_equal = &f_equal_ref;

		type->f_compare = &f_compare_object;

		auto hasIterFunction{ false };
		if (auto fobj{ impl::Object_GetMemberFunction(type, UserClassFunctionIterGet, nullptr) }) {
			auto iterType{ fobj->data.normal.ret };
			if (impl::Object_GetMemberFunction(type, UserClassFunctionIterSave, vm->getType(TypeId::List))
				&& impl::Object_GetMemberFunction(type, UserClassFunctionIterAdd, iterType)
				&& impl::Object_GetMemberFunction(type, UserClassFunctionIterCheck, vm->getType(TypeId::Bool))) {
				hasIterFunction = true;
			}
		}
		if (hasIterFunction) {
			type->f_iter_get = &f_iter_get_object;
			type->f_iter_save = &f_iter_save_object;
			type->f_iter_add = &f_iter_add_object;
			type->f_iter_check = &f_iter_check_object;
			type->f_iter_free = &f_iter_free_object;
		}
		else {
			type->f_iter_get = nullptr;
			type->f_iter_save = nullptr;
			type->f_iter_add = nullptr;
			type->f_iter_check = nullptr;
			type->f_iter_free = nullptr;
		}
	}
}