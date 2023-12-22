#pragma once

#include "hy.vm.h"

namespace hy::platform {
	LIB_EXPORT bool Platform_ReadFile(util::Path* path, util::ByteArray* ba) noexcept;
	LIB_EXPORT Memory Platform_LoadDll(const StringView fp) noexcept;
	LIB_EXPORT Memory Platform_GetDll(const StringView fp) noexcept;
	LIB_EXPORT bool Platform_FreeDll(Memory handle) noexcept;
	LIB_EXPORT Memory Platform_GetDllFunction(Memory handle, const char* name) noexcept;
	LIB_EXPORT void Platform_UTF8ToString(util::ByteArray* ba, String* str) noexcept;
	LIB_EXPORT void Platform_GB2312ToString(util::ByteArray* ba, String* str) noexcept;
	LIB_EXPORT void Platform_StringToUTF8(String* str, util::ByteArray* ba) noexcept;
	LIB_EXPORT void Platform_StringToGB2312(String* str, util::ByteArray* ba) noexcept;
}

namespace hy::impl {
	IResult<void> String_Concat(VM* vm, String* str, ObjArgsView args, bool newLine) noexcept;
	IResult<MapObject::ItemPointer> Map_Set(VM* vm, MapObject* obj, Object* key, Object* value) noexcept;
}

namespace hy {
	TypeObject* AddTypePrototype() noexcept;
	TypeObject* AddNullPrototype(TypeObject*) noexcept;
	TypeObject* AddIntPrototype(TypeObject*) noexcept;
	TypeObject* AddFloatPrototype(TypeObject*) noexcept;
	TypeObject* AddComplexPrototype(TypeObject*) noexcept;
	TypeObject* AddBoolPrototype(TypeObject*) noexcept;
	TypeObject* AddLVPrototype(TypeObject*) noexcept;
	TypeObject* AddIteratorPrototype(TypeObject*) noexcept;
	TypeObject* AddFunctionPrototype(TypeObject*) noexcept;
	TypeObject* AddMemberFunctionPrototype(TypeObject*) noexcept;
	TypeObject* AddStringPrototype(TypeObject*) noexcept;
	TypeObject* AddListPrototype(TypeObject*) noexcept;
	TypeObject* AddMapPrototype(TypeObject*) noexcept;
	TypeObject* AddVectorPrototype(TypeObject*) noexcept;
	TypeObject* AddMatrixPrototype(TypeObject*) noexcept;
	TypeObject* AddRangePrototype(TypeObject*) noexcept;
	TypeObject* AddArrayPrototype(TypeObject*) noexcept;
	TypeObject* AddBinPrototype(TypeObject*) noexcept;
	TypeObject* AddHashSetPrototype(TypeObject*) noexcept;

	TypeObject* AddObjectPrototype(VM* vm, const StringView name) noexcept;
	void SetObjectPrototype(VM* vm, TypeObject* type, TypeClassStruct* classStruct) noexcept;
	TypeObject* AddInterfacePrototype(VM* vm, const StringView name, TypeConceptStruct* conceptStruct) noexcept;

	void AddBuiltinConcept(VM* vm) noexcept;
	void AddBuiltinFunction(VM* vm) noexcept;

	LIB_EXPORT void SetError_CompileError(VM* vm, CodeResult cr, const StringView name) noexcept;
	LIB_EXPORT void SetError_ByteCodeBroken(VM* vm) noexcept;
	LIB_EXPORT void SetError_StackOverflow(VM* vm) noexcept;
	LIB_EXPORT void SetError_FileNotExists(VM* vm, const StringView name) noexcept;
	LIB_EXPORT void SetError_UnmatchedCall(VM* vm, const StringView name, ObjArgsView args) noexcept;
	LIB_EXPORT void SetError_UnmatchedIndex(VM* vm, const StringView name, ObjArgsView args) noexcept;
	LIB_EXPORT void SetError_UndefinedID(VM* vm, const StringView name) noexcept;
	LIB_EXPORT void SetError_UndefinedRef(VM* vm, RefView refView) noexcept;
	LIB_EXPORT void SetError_RedefinedID(VM* vm, const StringView name) noexcept;
	LIB_EXPORT void SetError_UnmatchedVersion(VM* vm, Byte* v1, const StringView v2) noexcept;
	LIB_EXPORT void SetError_UnmatchedPlatform(VM* vm, PlatformType v1, PlatformType v2) noexcept;
	LIB_EXPORT void SetError_IncompatibleAssign(VM* vm, TypeObject* t1, TypeObject* t2) noexcept;
	LIB_EXPORT void SetError_IncompatibleCalcAssign(VM* vm, TypeObject* t1, TypeObject* t2, AssignType opt) noexcept;
	LIB_EXPORT void SetError_UnsupportedSOPT(VM* vm, TypeObject* t, SOPTType opt) noexcept;
	LIB_EXPORT void SetError_UnsupportedBOPT(VM* vm, TypeObject* t1, TypeObject* t2, BOPTType opt) noexcept;
	LIB_EXPORT void SetError_UnmatchedMember(VM* vm, TypeObject* t1, const StringView name) noexcept;
	LIB_EXPORT void SetError_UnsupportedUnpack(VM* vm, TypeObject* t) noexcept;
	LIB_EXPORT void SetError_UnmatchedUnpack(VM* vm, Size s1, Size s2) noexcept;
	LIB_EXPORT void SetError_NotLeftValue(VM* vm, const StringView name) noexcept;
	LIB_EXPORT void SetError_IndexOutOfRange(VM* vm, Int64 index, Int64 boundary) noexcept;
	LIB_EXPORT void SetError_NotBoolean(VM* vm, TypeObject* t) noexcept;
	LIB_EXPORT void SetError_UndefinedConst(VM* vm, RefView refView) noexcept;
	LIB_EXPORT void SetError_RedefinedConst(VM* vm, const StringView name) noexcept;
	LIB_EXPORT void SetError_NotType(VM* vm, RefView refView) noexcept;
	LIB_EXPORT void SetError_UnmatchedType(VM* vm, TypeObject* t1, TypeObject* t2) noexcept;
	LIB_EXPORT void SetError_UnmatchedLen(VM* vm, Size s1, Size s2) noexcept;
	LIB_EXPORT void SetError_IllegalRange(VM* vm, Int64 start, Int64 step, Int64 end) noexcept;
	LIB_EXPORT void SetError_UnsupportedIterator(VM* vm, TypeObject* t) noexcept;
	LIB_EXPORT void SetError_UnsupportedKeyType(VM* vm, TypeObject* t) noexcept;
	LIB_EXPORT void SetError_NoKey(VM* vm, const StringView keyName) noexcept;
	LIB_EXPORT void SetError_IllegalMemberType(VM* vm, TypeObject* t) noexcept;
	LIB_EXPORT void SetError_DllError(VM* vm, const StringView path) noexcept;
	LIB_EXPORT void SetError_NegativeNumber(VM* vm, Int64 value) noexcept;
	LIB_EXPORT void SetError_EmptyContainer(VM* vm, const StringView name, TypeObject* t) noexcept;
	LIB_EXPORT void SetError_AssertFalse(VM* vm, Index pos) noexcept;
	LIB_EXPORT void SetError_UnsupportedScan(VM* vm, TypeObject* t) noexcept;
	LIB_EXPORT void SetError_IODeviceError(VM* vm) noexcept;
	LIB_EXPORT void SetError_ScanError(VM* vm, TypeObject* t) noexcept;

	IResult<void> CallFunction(VM* vm, FunctionObject* fobj, ObjArgsView args, Object* thisObject) noexcept;
	IResult<void> RunCallStack(VM* vm, Size cstCount) noexcept;
	LIB_EXPORT void RunByteCode(VM* vm, Module* mod, util::ByteArray* ba, bool movebc) noexcept;
}