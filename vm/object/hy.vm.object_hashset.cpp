#include "../hy.vm.impl.h"

#include <fast_io/fast_io.h>

namespace hy::impl {
	inline bool HashSet_CanAsKey(TypeObject* type) noexcept {
		return type->f_hash && (!type->a_mutable || type->f_copy);
	}

	inline void HashSet_Initialize(HashSetObject* obj) noexcept {
		obj->mCapacity = HashSetObject::DefaultCapacity;
		obj->mSize = 0ULL;
		obj->mThreshold = static_cast<Size>(HashSetObject::DefaultCapacity * HashSetObject::LoadFactor);
		obj->mTable = new HashSetObject::ItemPointer[obj->mCapacity];
		freestanding::initialize_n(obj->mTable, 0, obj->mCapacity);
	}

	inline void HashSet_Deallocate(HashSetObject* obj) noexcept {
		delete[] obj->mTable;
	}

	inline bool HashSet_Empty(HashSetObject* obj) noexcept {
		return obj->mSize == 0ULL;
	}

	inline Size HashSet_Size(HashSetObject* obj) noexcept {
		return obj->mSize;
	}

	inline void HashSet_Clear(HashSetObject* obj) noexcept {
		for (Index i{ }; i < obj->mCapacity; ++i) {
			for (auto item{ obj->mTable[i] }, next{ item }; item; item = next) {
				next = item->next;
				item->key->unlink();
				delete item;
			}
		}
		freestanding::initialize_n(obj->mTable, 0, obj->mCapacity);
		obj->mSize = 0ULL;
	}

	inline Size HashSet_Rehash(Size hash) noexcept {
		return hash ^ (hash >> 32ULL);
	}

	inline Index HashSet_Index(HashSetObject* obj, Size hash) noexcept {
		return hash & (obj->mCapacity - 1);
	}

	inline void HashSet_Expand(HashSetObject* obj) noexcept {
		auto newCapacity{ obj->mCapacity << 1ULL };
		obj->mThreshold = static_cast<Size>(newCapacity * obj->LoadFactor);
		auto newTable{ new HashSetObject::ItemPointer[newCapacity] };
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

	IResult<HashSetObject::ItemPointer> HashSet_Set(VM* vm, HashSetObject* obj, Object* key) noexcept {
		auto keyType{ key->type };
		// 键类型应能够哈希, 能够复制, 能够判等
		if (HashSet_CanAsKey(keyType)) {
			auto ir1{ keyType->f_hash(vm, key) };
			if (!ir1) return IResult<HashSetObject::ItemPointer>();
			auto hash{ HashSet_Rehash(ir1.data) }; // 键值哈希并扰乱
			auto index{ HashSet_Index(obj, hash) }; // 桶索引
			for (auto item{ obj->mTable[index] }; item; item = item->next) {
				if (item->hash == hash) { // 同哈希键
					if (auto ir2{ keyType->f_equal(vm, item->key, key) }) {
						if (ir2.data) return IResult<HashSetObject::ItemPointer>(item); // 存在相同键实体则忽略
					}
					else return IResult<HashSetObject::ItemPointer>();
				}
			}
			// 不存在键实体则创建
			if (obj->mSize >= obj->mThreshold) { // 达到阈值且发生哈希冲突时扩容
				HashSet_Expand(obj); // 扩容
				index = HashSet_Index(obj, hash); // 重新计算扩容后哈希
			}
			// 数量+1
			++obj->mSize;
			// 不可变对象直接引用, 引用对象拷贝键
			auto actualKey{ keyType->a_mutable ? keyType->f_copy(key) : key };
			// 键值链接
			actualKey->link();
			// 新元素实体
			auto newEntry{ new HashSetObject::Item{ actualKey, hash, obj->mTable[index] } };
			obj->mTable[index] = newEntry;
			return IResult<HashSetObject::ItemPointer>(newEntry);
		}
		return SetError<HashSetObject::ItemPointer>(&SetError_UnsupportedKeyType, vm, keyType);
	}

	inline HashSetObject::ItemPointer HashSet_GetIter(HashSetObject* obj, Index& pos) noexcept {
		for (Index i{ }; i < obj->mCapacity; ++i) {
			if (obj->mTable[i]) {
				pos = i;
				return obj->mTable[i];
			}
		}
		pos = obj->mCapacity;
		return nullptr;
	}

	inline HashSetObject::ItemPointer HashSet_NextIter(HashSetObject* obj, HashSetObject::ItemPointer pointer, Index pos, Index& newPos) noexcept {
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

	LIB_EXPORT void HashSet_Push(HVM hvm, ObjArgsView args, Object* thisObject) noexcept {
		auto vm{ vm_cast(hvm) };
		if(HashSet_Set(vm, obj_cast<HashSetObject>(thisObject), args[0]))
			vm->objectStack.push_link(thisObject);
	}

	LIB_EXPORT void HashSet_Clear(HVM hvm, ObjArgsView, Object* thisObject) noexcept {
		auto vm{ vm_cast(hvm) };
		HashSet_Clear(obj_cast<HashSetObject>(thisObject));
		vm->objectStack.push_link(thisObject);
	}

	LIB_EXPORT void HashSet_Delete(HVM hvm, ObjArgsView args, Object* thisObject) noexcept {
		auto vm{ vm_cast(hvm) };
		auto key{ args[0] };
		if (auto keyType{ key->type }; impl::HashSet_CanAsKey(keyType)) {
			if (auto ir1{ keyType->f_hash(vm, key) }) {
				auto hash{ impl::HashSet_Rehash(ir1.data) };
				auto hsobj{ obj_cast<HashSetObject>(thisObject) };
				auto index{ impl::HashSet_Index(hsobj, hash) };
				auto ok{ false };
				HashSetObject::ItemPointer item{ hsobj->mTable[index] }, parent{ };
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
						delete tmp;
					}
					else {
						auto tmp{ hsobj->mTable[index] };
						hsobj->mTable[index] = tmp->next;
						tmp->key->unlink();
						delete tmp;
					}
					--hsobj->mSize;
				}
				vm->objectStack.push_link(obj_allocate(vm->getType(TypeId::Bool), arg_cast(ok ? 1LL : 0LL)));
			}
		}
		else SetError_UnsupportedKeyType(vm, keyType);
	}
}

namespace hy {
	struct HashSetStaticData {
		Vector<HashSetObject*> pool;
		FunctionTable ft;

		HashSetStaticData(TypeObject* type) noexcept {
			auto funcType{ type->__getType(TypeId::Function) };

			Object* __any[] { nullptr };
			ObjArgsView empty, any1{ __any, 1ULL };

			ft.try_emplace(u"push", MakeNative<true, true>(funcType, u"hashset::push", &impl::HashSet_Push, any1));
			ft.try_emplace(u"clear", MakeNative<true, true>(funcType, u"hashset::clear", &impl::HashSet_Clear, empty));
			ft.try_emplace(u"delete", MakeNative<true, true>(funcType, u"hashset::delete", &impl::HashSet_Delete, any1));
		}
	};

	void f_class_create_hashset(TypeObject* type) noexcept {
		type->v_static = new HashSetStaticData{ type };
	}

	void f_class_delete_hashset(TypeObject* type) noexcept {
		auto staticData{ static_cast<HashSetStaticData*>(type->v_static) };
		for (auto obj : staticData->pool) {
			impl::HashSet_Deallocate(obj);
			delete obj;
		}
		for (auto& [_, fobj] : staticData->ft) delete fobj;
		delete staticData;
	}

	void f_class_clean_hashset(TypeObject* type) noexcept {
		auto staticData{ static_cast<HashSetStaticData*>(type->v_static) };
		for (auto obj : staticData->pool) {
			impl::HashSet_Deallocate(obj);
			delete obj;
		}
		staticData->pool = Vector<HashSetObject*>();
	}

	Object* f_allocate_hashset(TypeObject* type, Memory, Memory) noexcept {
		auto& pool{ static_cast<HashSetStaticData*>(type->v_static)->pool };
		HashSetObject* obj;
		if (pool.empty()) {
			obj = new HashSetObject{ type };
			impl::HashSet_Initialize(obj);
		}
		else {
			obj = pool.back();
			pool.pop_back();
		}
		return obj;
	}

	void f_deallocate_hashset(Object* obj) noexcept {
		auto& pool{ static_cast<HashSetStaticData*>(obj->type->v_static)->pool };
		auto hsobj{ obj_cast<HashSetObject>(obj) };
		impl::HashSet_Clear(hsobj);
		pool.emplace_back(hsobj);
	}

	bool f_bool_hashset(Object* obj) noexcept {
		return !impl::HashSet_Empty(obj_cast<HashSetObject>(obj));
	}

	IResult<void> f_string_hashset(HVM hvm, Object* obj, String* str) noexcept {
		auto vm{ vm_cast(hvm) };
		String tmp;
		tmp.push_back(u'{');
		auto hsobj{ obj_cast<HashSetObject>(obj) };
		for (Index i{ }; i < hsobj->mCapacity; ++i) {
			for (auto item{ hsobj->mTable[i] }, next{ item }; item; item = next) {
				next = item->next;
				if (!item->key->type->f_string(vm, item->key, &tmp)) return IResult<void>();
				tmp.push_back(u',');
			}
		}
		if (impl::HashSet_Empty(hsobj)) tmp.push_back(u'}');
		else tmp.back() = u'}';
		str->append(tmp);
		return IResult<void>(true);
	}

	IResult<Size> f_len_hashset(HVM, Object* obj) noexcept {
		return IResult<Size>(impl::HashSet_Size(obj_cast<HashSetObject>(obj)));
	}

	IResult<void> f_member_hashset(HVM hvm, bool isLV, Object* obj, const StringView member) noexcept {
		auto vm{ vm_cast(hvm) };
		if (!isLV) {
			auto& ft{ static_cast<HashSetStaticData*>(obj->type->v_static)->ft };
			if (auto pFobj{ ft.get(member) }) {
				auto fobj{ obj_allocate(vm->getType(TypeId::MemberFunction), *pFobj, obj) };
				vm->objectStack.push_link(fobj);
				return IResult<void>(true);
			}
			else return SetError(&SetError_UnmatchedMember, vm, obj->type, member);
		}
		else return SetError(&SetError_NotLeftValue, vm, member);
	}

	IResult<void> f_index_hashset(HVM hvm, bool isLV, Object* obj, ObjArgsView args) noexcept {
		auto vm{ vm_cast(hvm) };
		if (!isLV && args.size() == 1ULL) {
			auto key{ args[0] };
			auto keyType{ key->type };
			// 键类型应能够哈希, 能够复制, 能够判等
			if (impl::HashSet_CanAsKey(keyType)) {
				if (auto ir1{ keyType->f_hash(vm, key) }) {
					auto hash{ impl::HashSet_Rehash(ir1.data) }; // 键值哈希并扰乱
					auto hsobj{ obj_cast<HashSetObject>(obj) };
					auto index{ impl::HashSet_Index(hsobj, hash) }; // 桶索引
					auto v{ false };
					for (auto item{ hsobj->mTable[index] }; item; item = item->next) {
						if (item->hash == hash) { // 同哈希键
							if (auto ir2{ keyType->f_equal(vm, item->key, key) }) {
								if (ir2.data) {
									v = true;
									break;
								}
							}
							else return IResult<void>();
						}
					}
					vm->objectStack.push_link(obj_allocate(vm->getType(TypeId::Bool), arg_cast(v ? 1ULL : 0LL)));
					return IResult<void>(true);
				}
			}
			else SetError_UnsupportedKeyType(vm, keyType);
		}
		else SetError_UnmatchedIndex(vm, obj->type->v_name, args);
		return IResult<void>();
	}

	IResult<void> f_construct_hashset(HVM hvm, TypeObject* type, ObjArgsView args) noexcept {
		auto vm{ vm_cast(hvm) };
		if (auto argc{ args.size() }; argc == 0ULL) {
			vm->objectStack.push_link(obj_allocate(type));
			return IResult<void>(true);
		}
		return SetError(&SetError_UnmatchedCall, vm, type->v_name, args);
	}

	Object* f_full_copy_hashset(Object* obj) noexcept {
		auto hsobj{ obj_cast<HashSetObject>(obj) };
		auto clone{ new HashSetObject(obj->type) };
		clone->mSize = hsobj->mSize;
		clone->mCapacity = clone->mSize << 1ULL;
		clone->mThreshold = static_cast<Size>(clone->mCapacity * HashSetObject::LoadFactor);
		clone->mTable = new HashSetObject::ItemPointer[clone->mCapacity];
		freestanding::initialize_n(clone->mTable, 0, clone->mCapacity);
		for (Index i{ }; i < hsobj->mCapacity; ++i) {
			for (auto item{ hsobj->mTable[i] }; item; item = item->next) {
				auto key{ item->key };
				if (key->type->a_mutable) key = key->type->f_copy(key);
				key->link();
				auto newIndex{ impl::HashSet_Index(clone, item->hash) };
				auto newItem{ new HashSetObject::Item{ key, item->hash, clone->mTable[newIndex] } };
				clone->mTable[newIndex] = newItem;
			}
		}
		return clone;
	}

	constexpr auto HASHSET_ITER_ELEM{ 0ULL };

	IResult<void> f_iter_get_hashset(HVM hvm, Object* obj) noexcept {
		auto vm{ vm_cast(hvm) };
		auto hsobj{ obj_cast<HashSetObject>(obj) };
		auto iter{ obj_allocate<IteratorObject>(vm->getType(TypeId::Iterator)) };
		iter->ref = obj;
		iter->ref->link();
		Index pos;
		auto pointer{ impl::HashSet_GetIter(hsobj, pos) };
		iter->setData<HASHSET_ITER_ELEM>(pos, pointer);
		vm->objectStack.push_link(iter);
		return IResult<void>(true);
	}

	IResult<void> f_iter_save_hashset(HVM hvm, IteratorObject* iter, Size argc) noexcept {
		auto vm{ vm_cast(hvm) };
		if (argc == 1ULL) {
			auto key{ iter->getData<1ULL, HashSetObject::ItemPointer>()->key };
			auto keyType{ key->type };
			auto actualKey{ keyType->a_mutable ? keyType->f_copy(key) : key };
			vm->objectStack.push_link(actualKey);
			return IResult<void>(true);
		}
		return SetError(&SetError_UnmatchedUnpack, vm, 1ULL, argc);
	}

	IResult<void> f_iter_add_hashset(HVM, IteratorObject* iter) noexcept {
		auto hsobj{ iter->getReference<HashSetObject>() };
		auto pos{ iter->getData<0ULL, Index>() }, newPos{ 0ULL };
		auto pointer{ impl::HashSet_NextIter(hsobj,
			iter->getData<1ULL, HashSetObject::ItemPointer>(), pos, newPos) };
		iter->resetData<1ULL>(pointer);
		if (pos != newPos) iter->resetData<0ULL>(newPos);
		return IResult<void>(true);
	}

	IResult<bool> f_iter_check_hashset(HVM, IteratorObject* iter) noexcept {
		return IResult<bool>(iter->getData<0ULL, Index>() != iter->getReference<HashSetObject>()->mCapacity);
	}
}

namespace hy {
	TypeObject* AddHashSetPrototype(TypeObject* prototype) noexcept {
		auto type{ new TypeObject(prototype) };
		type->v_id = TypeId::HashSet;
		type->v_name = u"hashset";

		type->v_static = nullptr;
		type->v_cls = nullptr;
		type->v_cps = nullptr;

		type->a_mutable = true;
		type->a_def = true;
		type->a_unused1 = type->a_unused2 = false;

		type->f_class_create = &f_class_create_hashset;
		type->f_class_delete = &f_class_delete_hashset;
		type->f_class_clean = &f_class_clean_hashset;
		type->f_allocate = &f_allocate_hashset;
		type->f_deallocate = &f_deallocate_hashset;
		type->f_implement = nullptr;

		type->f_float = nullptr;
		type->f_bool = &f_bool_hashset;
		type->f_string = &f_string_hashset;

		type->f_hash = nullptr;
		type->f_len = &f_len_hashset;
		type->f_member = &f_member_hashset;
		type->f_index = &f_index_hashset;
		type->f_unpack = nullptr;

		type->f_construct = &f_construct_hashset;
		type->f_full_copy = &f_full_copy_hashset;
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

		type->f_iter_get = &f_iter_get_hashset;
		type->f_iter_save = &f_iter_save_hashset;
		type->f_iter_add = &f_iter_add_hashset;
		type->f_iter_check = &f_iter_check_hashset;
		type->f_iter_free = nullptr;

		return type;
	}
}