#pragma once

#include "../public/hy.util.h"

namespace hy::platform {
	void SetConsoleUTF8() noexcept;
	bool BrowserFile(const util::Path& path, String& str) noexcept;
	bool BrowserFile(const util::Path& path, util::ByteArray& ba) noexcept;
	bool SaveFile(const util::Path& path, const util::ByteArray& ba) noexcept;
	void AutoCurrentFolder() noexcept;
	util::Path GetWorkPath() noexcept;
	void SetWorkPath(const util::Path& path) noexcept;
	util::Path GetLibPath() noexcept;
	Memory LoadDll(const StringView fp) noexcept;
	Memory GetDll(const StringView fp) noexcept;
	bool FreeDll(Memory handle) noexcept;
	Memory GetDllFunction(Memory handle, const char* name) noexcept;
}