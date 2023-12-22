#include "hy.cli.platform.h"
#include "../public/hy.strings.h"
#include <fast_io/fast_io.h>

namespace hy::platform::details {
	inline constexpr auto MAX_PATH{ 261ULL };
	inline constexpr auto INVALID_HANDLE_VALUE{ static_cast<Uint64>(-1LL) };
	inline constexpr auto CP_UTF8{ 65001U };

	extern "C" {
		__declspec(dllimport) Int32 __stdcall SetConsoleOutputCP(Uint32 codePage);

		__declspec(dllimport) Uint32 __stdcall GetModuleFileNameW(Memory hModule, CStr lpFilename, Uint32 nSize);
		__declspec(dllimport) Uint32 __stdcall GetCurrentDirectoryW(Uint32 nBufferLength, Str lpBuffer);
		__declspec(dllimport) Int32 __stdcall SetCurrentDirectoryW(CStr lpPathName);

		__declspec(dllimport) Memory __stdcall CreateFileMappingFromApp(Memory hFile, Memory SecurityAttributes,
			Uint32 PageProtection, Uint64 MaximumSize, CStr Name);
		__declspec(dllimport) Memory __stdcall MapViewOfFileFromApp(Memory hFileMappingObject,
			Uint32 DesiredAccess, Uint64 FileOffset, Uint64 NumberOfBytesToMap);
		__declspec(dllimport) Int32 __stdcall UnmapViewOfFile(CMemory lpBaseAddress);

		__declspec(dllimport) Memory __stdcall CreateFileW(CStr lpFileName, Uint32 dwDesiredAccess,
			Uint32 dwShareMode, Memory lpSecurityAttributes, Uint32 dwCreationDisposition,
			Uint32 dwFlagsAndAttributes, Memory hTemplateFile);
		__declspec(dllimport) Uint32 __stdcall GetFileSize(Memory hFile, Uint32* lpFileSizeHigh);
		__declspec(dllimport) Int32 __stdcall WriteFile(Memory hFile, CMemory lpBuffer, Uint32 nSize,
			Uint32* wSize, Memory lpOverlapped);
		__declspec(dllimport) Int32 __stdcall CloseHandle(Memory hObject);

		__declspec(dllimport) Memory __stdcall LoadLibraryW(CStr lpLibFileName);
		__declspec(dllimport) Int32 __stdcall FreeLibrary(Memory hLibModule);
		__declspec(dllimport) Memory __stdcall GetProcAddress(Memory hModule, const char* lpProcName);
		__declspec(dllimport) Memory __stdcall GetModuleHandleW(CStr lpLibFileName);
	}
}

namespace hy::platform {
	void SetConsoleUTF8() noexcept {
		details::SetConsoleOutputCP(details::CP_UTF8);
	}

	bool BrowserFile(const util::Path& path, String& str) noexcept {
		auto ret{ false };
		fast_io::u16ostring_ref strRef{ &str };
		if (auto hFile{ details::CreateFileW(path.toView().data(), 0x80000000L, 1,
			nullptr, 3, 128, nullptr) };
			hFile != (Memory)details::INVALID_HANDLE_VALUE) {
			auto size{ details::GetFileSize(hFile, nullptr) };
			if (auto hFileMap{ details::CreateFileMappingFromApp(hFile, nullptr, 2, 0, nullptr) }) {
				if (auto data{ details::MapViewOfFileFromApp(hFileMap, 4, 0, 0) }) {
					print(strRef, fast_io::mnp::code_cvt_os_c_str(static_cast<Str8>(data), size));
					details::UnmapViewOfFile(data);
					ret = true;
				}
				details::CloseHandle(hFileMap);
			}
			details::CloseHandle(hFile);
		}
		return ret;
	}

	bool BrowserFile(const util::Path& path, util::ByteArray& ba) noexcept {
		auto ret{ false };
		if (auto hFile{ details::CreateFileW(path.toView().data(), 0x80000000L, 1,
			nullptr, 3, 128, nullptr) };
			hFile != (Memory)details::INVALID_HANDLE_VALUE) {
			auto size{ details::GetFileSize(hFile, nullptr) };
			if (auto hFileMap{ details::CreateFileMappingFromApp(hFile, nullptr, 2, 0, nullptr) }) {
				if (auto data{ details::MapViewOfFileFromApp(hFileMap, 4, 0, 0) }) {
					ba.assign(data, size);
					details::UnmapViewOfFile(data);
					ret = true;
				}
				details::CloseHandle(hFileMap);
			}
			details::CloseHandle(hFile);
		}
		return ret;
	}

	bool SaveFile(const util::Path& path, const util::ByteArray& ba) noexcept {
		if (auto hFile{ details::CreateFileW(path.toView().data(), 0x40000000L, 1,
			nullptr, 2, 128, nullptr) };
			hFile != (Memory)details::INVALID_HANDLE_VALUE) {
			Uint32 nBytes;
			details::WriteFile(hFile, ba.data(), static_cast<Size32>(ba.size()), &nBytes, nullptr);
			details::CloseHandle(hFile);
			return true;
		}
		return true;
	}

	void AutoCurrentFolder() noexcept {
		Char buf[details::MAX_PATH]{ };
		details::GetModuleFileNameW(nullptr, buf, details::MAX_PATH);
		details::SetCurrentDirectoryW(util::Path(buf).getParent().toView().data());
	}

	util::Path GetWorkPath() noexcept {
		Char buf[details::MAX_PATH]{ };
		details::GetCurrentDirectoryW(details::MAX_PATH, buf);
		return util::Path(buf).addSlash();
	}

	void SetWorkPath(const util::Path& path) noexcept {
		details::SetCurrentDirectoryW(path.toView().data());
	}

	util::Path GetLibPath() noexcept {
		Char buf[details::MAX_PATH]{ };
		details::GetModuleFileNameW(nullptr, buf, details::MAX_PATH);
		return util::Path(buf).getParent() + strings::LIB_PATH_NAME;
	}

	Memory LoadDll(const StringView fp) noexcept {
		return details::LoadLibraryW(fp.data());
	}

	Memory GetDll(const StringView fp) noexcept {
		return details::GetModuleHandleW(fp.data());
	}

	bool FreeDll(Memory handle) noexcept {
		return details::FreeLibrary(handle);
	}

	Memory GetDllFunction(Memory handle, const char* name) noexcept {
		return details::GetProcAddress(handle, name);
	}
}