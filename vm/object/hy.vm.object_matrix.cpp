#include "../hy.vm.impl.h"

#include <fast_io/fast_io.h>

namespace hy {
	struct MatrixStaticData {
		Vector<MatrixObject*> pool;
	};

	void f_class_create_matrix(TypeObject* type) noexcept {
		type->v_static = new MatrixStaticData;
	}

	void f_class_delete_matrix(TypeObject* type) noexcept {
		auto staticData{ static_cast<MatrixStaticData*>(type->v_static) };
		for (auto obj : staticData->pool) delete obj;
		delete staticData;
	}

	void f_class_clean_matrix(TypeObject* type) noexcept {
		auto staticData{ static_cast<MatrixStaticData*>(type->v_static) };
		for (auto obj : staticData->pool) delete obj;
		staticData->pool = Vector<MatrixObject*>();
	}

	// arg1: Float64* 矩阵数据(可空)
	// arg2: Size[2] 行, 列
	Object* f_allocate_matrix(TypeObject* type, Memory arg1, Memory arg2) noexcept {
		auto& pool{ static_cast<MatrixStaticData*>(type->v_static)->pool };
		auto data{ arg_recast<Float64*>(arg1) };
		auto pSize{ arg_recast<Size*>(arg2) };
		auto row{ pSize ? pSize[0] : 0ULL }, col{ pSize ? pSize[1] : 0ULL }, size{ row * col };
		if (size == 0ULL) row = col = 0ULL;
		MatrixObject* obj;
		if (pool.empty()) {
			obj = new MatrixObject(type);
			if (size) obj->data.resize(size);
		}
		else {
			obj = pool.back();
			pool.pop_back();
			if (size) {
				if (obj->data.mSize >= size) obj->data.mSize = size;
				else {
					obj->data.release();
					obj->data.resize(size);
				}
			}
			else obj->data.release();
		}
		obj->row = static_cast<Size32>(row);
		obj->col = static_cast<Size32>(col);
		if (data) freestanding::copy_n(obj->data.mData, data, size);
		return obj;
	}

	void f_deallocate_matrix(Object* obj) noexcept {
		auto& pool{ static_cast<MatrixStaticData*>(obj->type->v_static)->pool };
		pool.emplace_back(obj_cast<MatrixObject>(obj));
	}

	bool f_bool_matrix(Object* obj) noexcept {
		return !obj_cast<MatrixObject>(obj)->data.empty();
	}

	IResult<void> f_string_matrix(HVM, Object* obj, String* str) noexcept {
		fast_io::u16ostring_ref strRef{ str };
		str->push_back(u'[');
		auto mobj{ obj_cast<MatrixObject>(obj) };
		auto data{ mobj->data.data() };
		for (Size32 i{ }; i < mobj->row; ++i) {
			str->push_back(u'[');
			for (Size32 j{ }; j < mobj->col; ++j)
				print(strRef, data[i * mobj->col + j], fast_io::mnp::chvw(u','));
			str->back() = u']';
			str->push_back(u',');
		}
		if (mobj->data.empty()) str->push_back(u']');
		else str->back() = u']';
		return IResult<void>(true);
	}

	IResult<Size> f_hash_matrix(HVM, Object* obj) noexcept {
		std::hash<Float64> hasher;
		auto hash{ 0ULL };
		auto mobj{ obj_cast<MatrixObject>(obj) };
		for (auto item : mobj->data) hash ^= hasher(item);
		return IResult<Size>(hash);
	}

	IResult<Size> f_len_matrix(HVM, Object* obj) noexcept {
		return IResult<Size>(obj_cast<MatrixObject>(obj)->data.size());
	}

	IResult<void> f_member_matrix(HVM hvm, bool isLV, Object* obj, const StringView member) noexcept {
		auto vm{ vm_cast(hvm) };
		auto& ost{ vm->objectStack };
		auto mobj{ obj_cast<MatrixObject>(obj) };
		if (!isLV) {
			auto intType{ vm->getType(TypeId::Int) };
			if (member == u"row") ost.push_link(obj_allocate(intType, arg_cast(static_cast<Int64>(mobj->row))));
			else if (member == u"col") ost.push_link(obj_allocate(intType, arg_cast(static_cast<Int64>(mobj->col))));
			else return SetError(&SetError_UnmatchedMember, vm, obj->type, member);
			return IResult<void>(true);
		}
		else return SetError(&SetError_NotLeftValue, vm, member);
	}

	constexpr auto MATRIX_LVID_ROW_ELEM{ 0ULL };
	constexpr auto MATRIX_LVID_ELEM{ 1ULL };

	IResult<void> f_index_matrix(HVM hvm, bool isLV, Object* obj, ObjArgsView args) noexcept {
		auto vm{ vm_cast(hvm) };
		SetError_UnmatchedIndex(vm, obj->type->v_name, args);
		return IResult<void>();
	}

	IResult<void> f_construct_matrix(HVM hvm, TypeObject* type, ObjArgsView args) noexcept {
		auto vm{ vm_cast(hvm) };
		if (auto argc{ args.size() }; argc == 0ULL) {
			vm->objectStack.push_link(obj_allocate(type));
			return IResult<void>(true);
		}
		return SetError(&SetError_UnmatchedCall, vm, type->v_name, args);
	}

	Object* f_full_copy_matrix(Object* obj) noexcept {
		auto mobj{ obj_cast<MatrixObject>(obj) };
		Size sizeData[] { static_cast<Size>(mobj->row), static_cast<Size>(mobj->col) };
		return obj_allocate(obj->type, arg_cast(mobj->data.data()), sizeData);
	}
}

namespace hy {
	TypeObject* AddMatrixPrototype(TypeObject* prototype) noexcept {
		auto type{ new TypeObject(prototype) };
		type->v_id = TypeId::Matrix;
		type->v_name = u"matrix";

		type->v_static = nullptr;
		type->v_cls = nullptr;
		type->v_cps = nullptr;

		type->a_mutable = true;
		type->a_def = true;
		type->a_unused1 = type->a_unused2 = false;

		type->f_class_create = &f_class_create_matrix;
		type->f_class_delete = &f_class_delete_matrix;
		type->f_class_clean = &f_class_clean_matrix;
		type->f_allocate = &f_allocate_matrix;
		type->f_deallocate = &f_deallocate_matrix;
		type->f_implement = nullptr;

		type->f_float = nullptr;
		type->f_bool = &f_bool_matrix;
		type->f_string = &f_string_matrix;

		type->f_hash = &f_hash_matrix;
		type->f_len = &f_len_matrix;
		type->f_member = &f_member_matrix;
		type->f_index = &f_index_matrix;
		type->f_unpack = nullptr;

		type->f_construct = &f_construct_matrix;
		type->f_full_copy = &f_full_copy_matrix;
		type->f_copy = &f_full_copy_matrix;
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
}