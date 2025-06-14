#pragma once

#include "BkCore.h"
#include "BkSpan.h"

namespace Bk
{
	using FileHandle = uintptr_t;

	enum class FileAccess : uint8
	{
		Read = (1 << 0),
		Write = (1 << 1),
		Append = (1 << 2),
	};

	BK_ENUM_CLASS_FLAGS(FileAccess);

	FileHandle OpenFile(const char* path, FileAccess access);
	void CloseFile(FileHandle handle);

	size_t ReadFile(FileHandle handle, TSpan<uint8> buffer);
	size_t WriteFile(FileHandle handle, TSpan<uint8> buffer);

	size_t GetFileSize(FileHandle handle);
}
