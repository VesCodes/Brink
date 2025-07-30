#pragma once

#include "Core.h"
#include "Span.h"
#include "String.h"

namespace Bk
{
	// Time

	uint64 GetCpuTicks();
	uint64 GetCpuFrequency();

	uint64 GetTimeMs();
	double GetTimeSec();

	DateTime GetUtcTime();
	uint64 GetUnixTime();

	// File system

	using FileHandle = uintptr_t;

	enum class FileAccess : uint8
	{
		Read = (1 << 0),
		Write = (1 << 1),
		Append = (1 << 2),
	};

	BK_ENUM_FLAGS(FileAccess);

	enum class FileAttributes : uint8
	{
		Directory = (1 << 0),
		ReadOnly = (1 << 1),
	};

	BK_ENUM_FLAGS(FileAttributes);

	struct FileProperties
	{
		size_t size;
		DateTime createdTime;
		DateTime modifiedTime;
		FileAttributes attributes;
	};

	FileHandle OpenFile(String path, FileAccess access);
	void CloseFile(FileHandle handle);

	size_t ReadFile(FileHandle handle, TSpan<uint8> buffer);
	size_t WriteFile(FileHandle handle, TSpan<uint8> buffer);
	size_t GetFileSize(FileHandle handle);

	FileProperties GetFileProperties(FileHandle handle);
	FileProperties GetFileProperties(String path);

	bool MoveFile(String srcPath, String dstPath);
	bool DeleteFile(String path);

	bool CreateDirectory(String path);

	typedef bool (*EnumerateDirectoryCb)(String path, const FileProperties& properties);
	void EnumerateDirectory(String path, EnumerateDirectoryCb callback, bool recursive = false);

	// Process

	using ProcessHandle = uintptr_t;

	struct ProcessParams
	{
		String executable;
		String arguments;
	};

	ProcessHandle CreateProcess(const ProcessParams& params);
	void DetachProcess(ProcessHandle handle);
	bool WaitForProcess(ProcessHandle handle);
	bool TerminateProcess(ProcessHandle handle);
}
