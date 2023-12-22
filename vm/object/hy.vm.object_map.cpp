#include "../hy.vm.impl.h"

#include <fast_io/fast_io.h>

namespace hy::impl {
	inline bool Map_CanAsKey(TypeObject* type) noexcept {
		return type->f_hash && (!type->a_mutable || type->f_copy);
	}

	inline void Map_Initialize(MapObject* obj) noexcept {
		obj->mCapacity = MapObject::DefaultCapacity;
		obj->mSize = 0ULL;
		obj->mThreshold = static_cast<Size>(MapObject::DefaultCapacity * MapObject::LoadFactor);
		obj->mTable = new MapObject::ItemPointer[obj->mCapacity];
		freestanding::initialize_n(obj->mTable, 0, obj->mCapacity);
	}

	inline void Map_Deallocate(MapObject* obj) noexcept {
		delete[] obj->mTable;
	}

	inline bool Map_Empty(MapObject* obj) noexcept {
		return obj->mSize == 0ULL;
	}

	inline Size Map_Size(MapObject* obj) noexcept {
		return obj->mSize;
	}

	inline void Map_Clear(MapObject* obj) noexcept {
		for (Index i{ }; i < obj->mCapacity; ++i) {
			for (auto item{ obj->mTable[i] }, next{ item }; item; item = next) {
				next = item->next;
				item->key->unlink();
				item->value->unlink();
				delete item;
			}
		}
		freestanding::initialize_n(obj->mTable, 0, obj->mCapacity);
		obj->mSize = 0ULL;
	}

	inline Size Map_Rehash(Size hash) noexcept {
		return hash ^ (hash >> 32ULL);
	}

	inline Index Map_Index(MapObject* obj, Size hash) noexcept {
		return hash & (obj->mCapacity - 1);
	}

	inline void Map_Expand(MapObject* obj) noexcept {
		auto newCapacity{ obj->mCapacity << 1ULL };
		obj->mThreshold = static_cast<Size>(newCapacity * obj->LoadFactor);
		auto newTable{ new MapObject::ItemPointer[newCapacity] };
		freestanding::initialize_n(newTable, 0, newCapacity);
		for (Index i{ }; i < obj->mCapacity; ++i) {
			for (auto item{ obj->mTable[i] }, next{ item }; item; item = next) {
				next = item->next;
				auto newIndex{ item->hash & (newCapacity - 1) };
				item->next = newTable[newIndex];
				newTable[newIndex] = item;
			}
		}
		delete[] obj->mTable;
		obj->mTable = newTable;
		obj->mCapacity = newCapacity;
	}

	IResult<MapObject::ItemPointer> Map_Set(VM* vm, MapObject* obj, Object* key, Object* value) noexcept {
		auto keyType{ key->type };
		// 键类型应能够哈希, 能够复制, 能够判等
		if (Map_CanAsKey(keyType)) {
			auto ir1{ keyType->f_hash(vm, key) };
			if (!ir1) return IResult<MapObject::ItemPointer>();
			auto hash{ Map_Rehash(ir1.data) }; // 键值哈希并扰乱
			auto index{ Map_Index(obj, hash) }; // 桶索引
			for (auto item{ obj->mTable[index] }; item; item = item->next) {
				if (item->hash == hash) { // 同哈希键
					if (auto ir2{ keyType->f_equal(vm, item->key, key) }) {
						if (ir2.data) { // 存在相同键实体则覆盖
							value->link();
							item->value->unlink();
							item->value = value;
							return IResult<MapObject::ItemPointer>(item);
						}
					}
					else return IResult<MapObject::ItemPointer>();
				}
			}
			// 不存在键实体则创建
			if (obj->mSize >= obj->mThreshold) { // 达到阈值且发生哈希冲突时扩容
				Map_Expand(obj); // 扩容
				index = Map_Index(obj, hash); // 重新计算扩容后哈希
			}
			// 数量+1
			++obj->mSize;
			// 不可变对象直接引用, 引用对象拷贝键
			auto actualKey{ keyType->a_mutable ? keyType->f_copy(key) : key };
			// 键值链接
			actualKey->link();
			value->link();
			// 新元素实体
			auto newEntry{ new MapObject::Item{ actualKey, hash, value, obj->mTable[index] } };
			obj->mTable[index] = newEntry;
			return IResult<MapObject::ItemPointer>(newEntry);
		}
		return SetError<MapObject::ItemPointer>(&SetError_UnsupportedKeyType,vm, keyType);
	}

	IResult<MapObject::ItemPointer> Map_Get(VM* vm, MapObject* obj, Object* key) noexcept {
		auto keyType{ key->type };
		// 键类型应能够哈希, 能够复制, 能够判等
		if (Map_CanAsKey(keyType)) {
			auto ir1{ keyType->f_hash(vm, key) };
			if (!ir1) return IResult<MapObject::ItemPointer>();
			auto hash{ Map_Rehash(ir1.data) }; // 键值哈希并扰乱
			auto index{ Map_Index(obj, hash) }; // 桶索引
			for (auto item{ obj->mTable[index] }; item; item = item->next) {
				if (item->hash == hash) { // 同哈希键
					if (auto ir2{ keyType->f_equal(vm, item->key, key) }) {
						if (ir2.data) return IResult<MapObject::ItemPointer>(item);
					}
					else return IResult<MapObject::ItemPointer>();
				}
			}
			String keyName{ u"<" };
			keyName += keyType->v_name;
			keyName += u"> ";
			if (keyType->v_id == TypeId::Int || keyType->v_id == TypeId::Float
				|| keyType->v_id == TypeId::Complex || keyType->v_id == TypeId::Bool
				|| keyType->v_id == TypeId::String || keyType->v_id == TypeId::Vector
				|| keyType->v_id == TypeId::Matrix || keyType->v_id == TypeId::Range)
				keyType->f_string(vm, key, &keyName); // 简单类型输出它的键值
			SetError_NoKey(vm, keyName);
		}
		else SetError_UnsupportedKeyType(vm, keyType);
		return IResult<MapObject::ItemPointer>();
	}

	MapObject::ItemPointer Map_GetIter(MapObject* obj, Index& pos) noexcept {
		for (Index i{ }; i < obj->mCapacity; ++i) {
			if (obj->mTable[i]) {
				pos = i;
				return obj->mTable[i];
			}
		}
		pos = obj->mCapacity;
		return nullptr;
	}

	MapObject::ItemPointer Map_NextIter(MapObject* obj, MapObject::ItemPointer pointer, Index pos, Index& newPos) noexcept {
		if (pointer->next) {
			newPos = pos;
			return pointer->next;
		}
		for (auto i{ pos + 1ULL }; i < obj->mCapacity; ++i) {
			if (obj->mTable[i]) {
				newPos = i;
				return obj->mTable[i];
			}
		}
		newPos = obj->mCapacity;
		return nullptr;
	}

	LIB_EXPORT void Map_Set(HVM hvm, ObjArgsView args, Object* thisObject) noexcept {
		auto vm{ vm_cast(hvm) };
		Map_Set(vm, obj_cast<MapObject>(thisObject), args[0], args[1]);
		vm->objectStack.push_link(thisObject);
	}

	LIB_EXPORT void Map_Get(HVM hvm, ObjArgsView args, Object* thisObject) noexcept {
		auto vm{ vm_cast(hvm) };
		if (auto ir{ Map_Get(vm, obj_cast<MapObject>(thisObject), args[0]) })
			vm->objectStack.push_link(ir.data->value);
	}

	LIB_EXPORT void Map_Delete(HVM hvm, ObjArgsView args, Object* thisObject) noexcept {
		auto vm{ vm_cast(hvm) };
		auto key{ args[0] };
		if (auto keyType{ key->type }; impl::Map_CanAsKey(keyType)) {
			if (auto ir1{ keyType->f_hash(vm, key) }) {
				auto hash{ impl::Map_Rehash(ir1.data) };
				auto mobj{ obj_cast<MapObject>(thisObject) };
				auto index{ impl::Map_Index(mobj, hash) };
				auto ok{ false };
				MapObject::ItemPointer item{ mobj->mTable[index] }, parent{ };
				for (; item; item = item->next) {
					if (item->hash == hash) {
						if (auto ir2{ keyType->f_equal(vm, item->key, key) }) {
							if (ir2.data) {
								ok = true;
								break;
							}
						}
						else return;
					}
					parent = item;
				}
				if (ok) {
					if (parent) {
						auto tmp{ parent->next };
						parent->next = tmp->next;
						tmp->key->unlink();
						tmp->value->unlink();
						delete tmp;
					}
					else {
						auto tmp{ mobj->mTable[index] };
						mobj->mTable[index] = tmp->next;
						tmp->key->unlink();
						tmp->value->unlink();
						delete tmp;
					}
					--mobj->mSize;
				}
				vm->objectStack.push_link(obj_allocate(vm->getType(TypeId::Bool), arg_cast(ok ? 1LL : 0LL)));
			}
		}
		else SetError_UnsupportedKeyType(vm, keyType);
	}

	LIB_EXPORT void Map_Contains(HVM hvm, ObjArgsView args, Object* thisObject) noexcept {
		auto vm{ vm_cast(hvm) };
		auto key{ args[0] };
		auto keyType{ key->type };
		if (impl::Map_CanAsKey(keyType)) {
			if (auto ir1{ keyType->f_hash(vm, key) }) {
				auto hash{ impl::Map_Rehash(ir1.data) };
				auto mobj{ obj_cast<MapObject>(thisObject) };
				auto index{ impl::Map_Index(mobj, hash) };
				auto v{ false };
				for (auto item{ mobj->mTable[index] }; item; item = item->next) {
					if (item->hash == hash) {
						if (auto ir2{ keyType->f_equal(vm, item->key, key) }) {
							if (ir2.data) {
								v = true;
								break;
							}
						}
						else return;
					}
				}
				vm->objectStack.push_link(obj_allocate(vm->getType(TypeId::Bool), arg_cast(v ? 1ULL : 0LL)));
			}
		}
		else SetError_UnsupportedKeyType(vm, keyType);
	}

	LIB_EXPORT void Map_Clear(HVM hvm, ObjArgsView, Object* thisObject) noexcept {
		auto vm{ vm_cast(hvm) };
		Map_Clear(obj_cast<MapObject>(thisObject));
		vm->objectStack.push_link(thisObject);
	}

	LIB_EXPORT void Map_Keys(HVM hvm, ObjArgsView, Object* thisObject) noexcept {
		auto vm{ vm_cast(hvm) };
		auto obj{ obj_cast<MapObject>(thisObject) };
		auto lobj{ obj_allocate<ListObject>(vm->getType(TypeId::List)) };
		lobj->objects.reserve(obj->mSize);
		for (Index i{ }; i < obj->mCapacity; ++i) {
			for (auto item{ obj->mTable[i] }; item; item = item->next) {
				auto key{ item->key };
				auto actualKey{ key->type->a_mutable ? key->type->f_copy(key) : key };
				actualKey->link();
				lobj->objects.emplace_back(actualKey);
			}
		}
		vm->objectStack.push_link(lobj);
	}

	LIB_EXPORT void Map_Values(HVM hvm, ObjArgsView, Object* thisObject) noexcept {
		auto vm{ vm_cast(hvm) };
		auto obj{ obj_cast<MapObject>(thisObject) };
		auto lobj{ obj_allocate<ListObject>(vm->getType(TypeId::List)) };
		lobj->objects.reserve(obj->mSize);
		for (Index i{ }; i < obj->mCapacity; ++i) {
			for (auto item{ obj->mTable[i] }; item; item = item->next) {
				auto value{ item->value };
				value->link();
				lobj->objects.emplace_back(value);
			}
		}
		vm->objectStack.push_link(lobj);
	}
}

namespace hy {
	struct MapStaticData {
		Vector<MapObject*> pool;
		FunctionTable ft;

		MapStaticData(TypeObject* type) noexcept {
			auto funcType{ type->__getType(TypeId::Function) };

			Object* __any[]{ nullptr, nullptr };
			ObjArgsView empty, any1{ __any, 1ULL }, any2{ __any, 2ULL };

			ft.try_emplace(u"set", MakeNative<true, true>(funcType, u"map::set", &impl::Map_Set, any2));
			ft.try_emplace(u"get", MakeNative<true, true>(funcType, u"map::get", &impl::Map_Get, any1));
			ft.try_emplace(u"delete", MakeNative<true, true>(funcType, u"map::delete", &impl::Map_Delete, any1));
			ft.try_emplace(u"contains", MakeNative<true, true>(funcType, u"map::contains", &impl::Map_Contains, any1));
			ft.try_emplace(u"clear", MakeNative<true, true>(funcType, u"map::clear", &impl::Map_Clear, empty));
			ft.try_emplace(u"keys", MakeNative<true, true>(funcType, u"map::keys", &impl::Map_Keys, empty));
			ft.try_emplace(u"values", MakeNative<true, true>(funcType, u"map::values", &impl::Map_Values, empty));
		}
	};

	void f_class_create_map(TypeObject* type) noexcept {
		type->v_static = new MapStaticData{ type };
	}

	void f_class_delete_map(TypeObject* type) noexcept {
		auto staticData{ static_cast<MapStaticData*>(type->v_static) };
		for (auto obj : staticData->pool) {
			impl::Map_Deallocate(obj);
			delete obj;
		}
		for (auto& [_, fobj] : staticData->ft) delete fobj;
		delete staticData;
	}

	void f_class_clean_map(TypeObject* type) noexcept {
		auto staticData{ static_cast<MapStaticData*>(type->v_static) };
		for (auto obj : staticData->pool) {
			impl::Map_Deallocate(obj);
			delete obj;
		}
		staticData->pool = Vector<MapObject*>();
	}

	Object* f_allocate_map(TypeObject* type, Memory, Memory) noexcept {
		auto& pool{ static_cast<MapStaticData*>(type->v_static)->pool };
		MapObject* obj;
		if (pool.empty()) {
			obj = new MapObject{ type };
			impl::Map_Initialize(obj);
		}
		else {
			obj = pool.back();
			pool.pop_back();
		}
		return obj;
	}

	void f_deallocate_map(Object* obj) noexcept {
		auto& pool{ static_cast<MapStaticData*>(obj->type->v_static)->pool };
		auto mobj{ obj_cast<MapObject>(obj) };
		impl::Map_Clear(mobj);
		pool.emplace_back(mobj);
	}

	bool f_bool_map(Object* obj) noexcept {
		return !impl::Map_Empty(obj_cast<MapObject>(obj));
	}

	IResult<void> f_string_map(HVM hvm, Object* obj, String* str) noexcept {
		auto vm{ vm_cast(hvm) };
		String tmp;
		tmp.push_back(u'{');
		auto mobj{ obj_cast<MapObject>(obj) };
		for (Index i{ }; i < mobj->mCapacity; ++i) {
			for (auto item{ mobj->mTable[i] }, next{ item }; item; item = next) {
				next = item->next;
				if (!item->key->type->f_string(vm, item->key, &tmp)) return IResult<void>();
				tmp.push_back(u':');
				if (!item->value->type->f_string(vm, item->value, &tmp)) return IResult<void>();
				tmp.push_back(u',');
			}
		}
		if (impl::Map_Empty(mobj)) tmp.push_back(u'}');
		else tmp.back() = u'}';
		str->append(tmp);
		return IResult<void>(true);
	}

	IResult<Size> f_len_map(HVM, Object* obj) noexcept {
		return IResult<Size>(impl::Map_Size(obj_cast<MapObject>(obj)));
	}

	IResult<void> f_member_map(HVM hvm, bool isLV, Object* obj, const StringView member) noexcept {
		auto vm{ vm_cast(hvm) };
		if (!isLV) {
			auto& ft{ static_cast<MapStaticData*>(obj->type->v_static)->ft };
			if (auto pFobj{ ft.get(member) }) {
				auto fobj{ obj_allocate(vm->getType(TypeId::MemberFunction), *pFobj, obj) };
				vm->objectStack.push_link(fobj);
				return IResult<void>(true);
			}
			else return SetError(&SetError_UnmatchedMember, vm, obj->type, member);
		}
		else return SetError(&SetError_NotLeftValue, vm, member);
	}

	constexpr auto MAP_LVID_ELEM{ 0ULL };

	IResult<void> f_index_map(HVM hvm, bool isLV, Object* obj, ObjArgsView args) noexcept {
		auto vm{ vm_cast(hvm) };
		auto mobj{ obj_cast<MapObject>(obj) };
		if (args.size() == 1ULL) {
			auto arg{ args[0] };
			if (isLV) {
				auto ret{ obj_allocate<LVObject>(vm->getType(TypeId::LV)) };
				ret->parent = obj;
				ret->parent->link();
				ret->setData<MAP_LVID_ELEM>(arg);
				arg->link();
				vm->objectStack.push_link(ret);
				return IResult<void>(true);
			}
			else {
				if (auto ir{ impl::Map_Get(vm, mobj, arg) }) {
					vm->objectStack.push_link(ir.data->value);
					return IResult<void>(true);
				}
			}
		}
		else SetError_UnmatchedIndex(vm, obj->type->v_name, args);
		return IResult<void>();
	}

	IResult<void> f_construct_map(HVM hvm, TypeObject* type, ObjArgsView args) noexcept {
		auto vm{ vm_cast(hvm) };
		if (auto argc{ args.size() }; argc == 0ULL) {
			vm->objectStack.push_link(obj_allocate(type));
			return IResult<void>(true);
		}
		return SetError(&SetError_UnmatchedCall, vm, type->v_name, args);
	}

	Object* f_full_copy_map(Object* obj) noexcept {
		auto mobj{ obj_cast<MapObject>(obj) };
		auto clone{ new MapObject{ obj->type } };
		clone->mSize = mobj->mSize;
		clone->mCapacity = clone->mSize << 1ULL;
		clone->mThreshold = static_cast<Size>(clone->mCapacity * MapObject::LoadFactor);
		clone->mTable = new MapObject::ItemPointer[clone->mCapacity];
		freestanding::initialize_n(clone->mTable, 0, clone->mCapacity);
		for (Index i{ }; i < mobj->mCapacity; ++i) {
			for (auto item{ mobj->mTable[i] }; item; item = item->next) {
				auto key{ item->key };
				if (key->type->a_mutable) key = key->type->f_copy(key);
				key->link();
				auto value{ item->value };
				if (value->type->f_full_copy) value = value->type->f_full_copy(value);
				value->link();
				auto newIndex{ impl::Map_Index(clone, item->hash) };
				auto newItem{ new MapObject::Item{ key, item->hash, value, clone->mTable[newIndex] } };
				clone->mTable[newIndex] = newItem;
			}
		}
		return clone;
	}

	IResult<void> f_assign_lv_data_map(HVM hvm, LVObject* lv, Object* obj) noexcept {
		if (impl::Map_Set(vm_cast(hvm), lv->getParent<MapObject>(), lv->getData<0ULL, Object*>(), obj))
			return IResult<void>(true);
		return IResult<void>();
	}

	IResult<void> f_calcassign_lv_data_map(HVM hvm, LVObject* lv, Object* obj, AssignType opt) noexcept {
		auto vm{ vm_cast(hvm) };
		auto mobj{ lv->getParent<MapObject>() };
		auto key{ lv->getData<0ULL, Object*>() };
		if (auto ir = impl::Map_Get(vm, mobj, key)) {
			auto pValue{ &ir.data->value };
			auto valueType{ (*pValue)->type };
			if (valueType->f_calcassign) {
				auto valueLV{ obj_allocate<LVObject>(lv->type) };
				valueLV->parent = obj;
				valueLV->parent->link();
				valueLV->lvType = LVType::ADDRESS;
				valueLV->storage.address = pValue;
				valueLV->link();
				valueType->f_calcassign(vm, valueLV, obj, opt);
				valueLV->unlink();
			}
			else SetError_IncompatibleCalcAssign(vm, valueType, obj->type, opt);
			return IResult<void>(vm->ok());
		}
		return IResult<void>();
	}

	void f_free_lvdata_map(LVObject* lv) noexcept {
		lv->getData<0ULL, Object*>()->unlink();
	}

	constexpr auto MAP_ITER_ELEM{ 0ULL };

	IResult<void> f_iter_get_map(HVM hvm, Object* obj) noexcept {
		auto vm{ vm_cast(hvm) };
		auto mobj{ obj_cast<MapObject>(obj) };
		auto iter{ obj_allocate<IteratorObject>(vm->getType(TypeId::Iterator)) };
		iter->ref = obj;
		iter->ref->link();
		Index pos;
		auto pointer{ impl::Map_GetIter(mobj, pos) };
		iter->setData<MAP_ITER_ELEM>(pos, pointer);
		vm->objectStack.push_link(iter);
		return IResult<void>(true);
	}

	IResult<void> f_iter_save_map(HVM hvm, IteratorObject* iter, Size argc) noexcept {
		auto vm{ vm_cast(hvm) };
		if (argc == 2ULL) {
			auto pointer{ iter->getData<1ULL, MapObject::ItemPointer>() };
			auto key{ pointer->key }, value{ pointer->value };
			auto keyType{ key->type };
			auto actualKey{ keyType->a_mutable ? keyType->f_copy(key) : key };
			vm->objectStack.push_link(actualKey);
			vm->objectStack.push_link(value);
			return IResult<void>(true);
		}
		return SetError(&SetError_UnmatchedUnpack, vm, 2ULL, argc);
	}

	IResult<void> f_iter_add_map(HVM, IteratorObject* iter) noexcept {
		auto mobj{ iter->getReference<MapObject>() };
		Index pos{ iter->getData<0ULL, Index>() }, newPos;
		auto pointer{ impl::Map_NextIter(mobj,
			iter->getData<1ULL, MapObject::ItemPointer>(), pos, newPos) };
		iter->resetData<1ULL>(pointer);
		if (pos != newPos) iter->resetData<0ULL>(newPos);
		return IResult<void>(true);
	}

	IResult<bool> f_iter_check_map(HVM, IteratorObject* iter) noexcept {
		return IResult<bool>(iter->getData<0ULL, Index>() != iter->getReference<MapObject>()->mCapacity);
	}
}

namespace hy {
	TypeObject* AddMapPrototype(TypeObject* prototype) noexcept {
		auto type{ new TypeObject(prototype) };
		type->v_id = TypeId::Map;
		type->v_name = u"map";

		type->v_static = nullptr;
		type->v_cls = nullptr;
		type->v_cps = nullptr;

		type->a_mutable = true;
		type->a_def = true;
		type->a_unused1 = type->a_unused2 = false;

		type->f_class_create = &f_class_create_map;
		type->f_class_delete = &f_class_delete_map;
		type->f_class_clean = &f_class_clean_map;
		type->f_allocate = &f_allocate_map;
		type->f_deallocate = &f_deallocate_map;
		type->f_implement = nullptr;

		type->f_float = nullptr;
		type->f_bool = &f_bool_map;
		type->f_string = &f_string_map;

		type->f_hash = nullptr;
		type->f_len = &f_len_map;
		type->f_member = &f_member_map;
		type->f_index = &f_index_map;
		type->f_unpack = nullptr;

		type->f_construct = &f_construct_map;
		type->f_full_copy = &f_full_copy_map;
		type->f_copy = nullptr;
		type->f_write = nullptr;
		type->f_scan = nullptr;
		type->f_calcassign = nullptr;

		type->f_assign_lv_data = &f_assign_lv_data_map;
		type->f_calcassign_lv_data = &f_calcassign_lv_data_map;
		type->f_free_lv_data = &f_free_lvdata_map;

		type->f_sopt_calc = nullptr;
		type->f_bopt_calc = nullptr;
		type->f_equal = &f_equal_ref;
		type->f_compare = nullptr;

		type->f_iter_get = &f_iter_get_map;
		type->f_iter_save = &f_iter_save_map;
		type->f_iter_add = &f_iter_add_map;
		type->f_iter_check = &f_iter_check_map;
		type->f_iter_free = nullptr;

		return type;
	}
}