#include "hy.vm.impl.h"

namespace hy::platform {
	namespace details {
		constexpr auto INVALID_HANDLE_VALUE{ static_cast<Uint64>(-1LL) };

		constexpr auto CP_GB2312{ 936U };
		constexpr auto CP_UTF8{ 65001U };

		extern "C" {
			__declspec(dllimport) Memory __stdcall CreateFileMappingFromApp(Memory hFile, Memory SecurityAttributes,
				Uint32 PageProtection, Uint64 MaximumSize, CStr Name);
			__declspec(dllimport) Memory __stdcall MapViewOfFileFromApp(Memory hFileMappingObject,
				Uint32 DesiredAccess, Uint64 FileOffset, Uint64 NumberOfBytesToMap);
			__declspec(dllimport) Int32 __stdcall UnmapViewOfFile(CMemory lpBaseAddress);

			__declspec(dllimport) Memory __stdcall CreateFileW(CStr lpFileName, Uint32 dwDesiredAccess,
				Uint32 dwShareMode, Memory lpSecurityAttributes, Uint32 dwCreationDisposition,
				Uint32 dwFlagsAndAttributes, Memory hTemplateFile);
			__declspec(dllimport) Uint32 __stdcall GetFileSize(Memory hFile, Uint32* lpFileSizeHigh);
			__declspec(dllimport) Int32 __stdcall CloseHandle(Memory hObject);

			__declspec(dllimport) Memory __stdcall LoadLibraryW(CStr lpLibFileName);
			__declspec(dllimport) Int32 __stdcall FreeLibrary(Memory hLibModule);
			__declspec(dllimport) Memory __stdcall GetProcAddress(Memory hModule, const char* lpProcName);
			__declspec(dllimport) Memory __stdcall GetModuleHandleW(CStr lpLibFileName);

			__declspec(dllimport) Int32 __stdcall MultiByteToWideChar(Uint32 CodePage, Uint32 dwFlags,
					const Byte* lpMultiByteStr, Int32 cbMultiByte, Str lpWideCharStr, Int32 cchWideChar);

			__declspec(dllimport) Int32 __stdcall WideCharToMultiByte(Uint32 CodePage, Uint32 dwFlags,
					CStr lpWideCharStr, Int32 cchWideChar, Byte* lpMultiByteStr, Uint32 cbMultiByte,
				const char* lpDefaultChar, Int32* lpUsedDefaultChar);
		}
	}


	bool Platform_ReadFile(util::Path* path, util::ByteArray* ba) noexcept {
		auto ret{ false };
		if (auto hFile{ details::CreateFileW(path->toView().data(), 0x80000000L, 1,
			nullptr, 3, 128, nullptr) };
			hFile != (Memory)details::INVALID_HANDLE_VALUE) {
			auto size{ details::GetFileSize(hFile, nullptr) };
			if (auto hFileMap{ details::CreateFileMappingFromApp(hFile, nullptr, 2, 0, nullptr) }) {
				if (auto data{ details::MapViewOfFileFromApp(hFileMap, 4, 0, 0) }) {
					ba->assign(data, size);
					details::UnmapViewOfFile(data);
					ret = true;
				}
				details::CloseHandle(hFileMap);
			}
			details::CloseHandle(hFile);
		}
		return ret;
	}

	Memory Platform_LoadDll(const StringView fp) noexcept {
		return details::LoadLibraryW(fp.data());
	}

	Memory Platform_GetDll(const StringView fp) noexcept {
		return details::GetModuleHandleW(fp.data());
	}

	bool Platform_FreeDll(Memory handle) noexcept {
		return details::FreeLibrary(handle);
	}

	Memory Platform_GetDllFunction(Memory handle, const char* name) noexcept {
		return details::GetProcAddress(handle, name);
	}

	void Platform_UTF8ToString(util::ByteArray* ba, String* str) noexcept {
		auto u8data{ ba->data() };
		auto u8size{ static_cast<Size32>(ba->size()) };
		auto u16len{ details::MultiByteToWideChar(details::CP_UTF8, 0,
			u8data, u8size, nullptr, 0) };
		str->resize(u16len);
		details::MultiByteToWideChar(details::CP_UTF8, 0,
			u8data, u8size, str->data(), u16len);
	}

	void Platform_GB2312ToString(util::ByteArray* ba, String* str) noexcept {
		auto gb2312data{ ba->data() };
		auto gb2312size{ static_cast<Size32>(ba->size()) };
		auto u16len{ details::MultiByteToWideChar(details::CP_GB2312, 0,
			gb2312data, gb2312size, nullptr, 0) };
		str->resize(u16len);
		details::MultiByteToWideChar(details::CP_GB2312, 0,
			gb2312data, gb2312size, str->data(), u16len);
	}

	void Platform_StringToUTF8(String* str, util::ByteArray* ba) noexcept {
		auto u16data{ static_cast<CStr>(str->data()) };
		auto u16size{ static_cast<Size32>(str->size()) };
		auto u8len{ details::WideCharToMultiByte(details::CP_UTF8, 0, u16data, u16size,
			nullptr, 0, nullptr, nullptr) };
		ba->resize(u8len);
		details::WideCharToMultiByte(details::CP_UTF8, 0, u16data, u16size,
			ba->data(), u8len, nullptr, nullptr);
	}

	void Platform_StringToGB2312(String* str, util::ByteArray* ba) noexcept {
		auto u16data{ static_cast<CStr>(str->data()) };
		auto u16size{ static_cast<Size32>(str->size()) };
		auto gb2312len{ details::WideCharToMultiByte(details::CP_GB2312, 0, u16data, u16size,
			nullptr, 0, nullptr, nullptr) };
		ba->resize(gb2312len);
		details::WideCharToMultiByte(details::CP_GB2312, 0, u16data, u16size,
			ba->data(), gb2312len, nullptr, nullptr);
	}
}