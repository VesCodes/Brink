#pragma once

#include "Core.h"
#include "Span.h"
#include "String.h"

namespace Bk
{
	enum class Platform : uint8
	{
		Windows,
		MacOS,
		Emscripten,
	};

	Platform GetPlatform();
	String GetPlatformName();
	String GetPlatformName(Platform platform);

	// Time

	uint64 GetCpuTicks();
	uint64 GetCpuFrequency();

	uint64 GetTimeMs();
	double GetTimeSec();

	DateTime GetUtcTime();
	DateTime GetLocalTime();
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

	bool FileExists(String path);
	bool MoveFile(String srcPath, String dstPath);
	bool DeleteFile(String path);

	bool CreateDirectory(String path);

	using FileIteratorHandle = struct FileIterator*;

	struct FileIteratorEntry
	{
		String path;
		FileProperties properties;
	};

	FileIteratorHandle CreateFileIterator(Arena& arena, String path);
	bool AdvanceFileIterator(FileIteratorHandle iterator, FileIteratorEntry& entry);
	void DestroyFileIterator(FileIteratorHandle iterator);

	// Process

	using ProcessHandle = uintptr_t;

	struct ProcessParams
	{
		String executable;
		String arguments;
	};

	ProcessHandle CreateProcess(const ProcessParams& params);
	void DetachProcess(ProcessHandle handle);
	bool WaitForProcess(ProcessHandle handle, int32* exitCode = nullptr);
	bool TerminateProcess(ProcessHandle handle);

	// Window

	struct WindowParams
	{
		String title;
		int32 width;
		int32 height;
	};

	uint32 CreateWindow(const WindowParams& params);
	void DestroyWindow(uint32 handle);

	void* GetWindowSurfaceTarget(uint32 handle);
	bool GetWindowSurfaceSize(uint32 handle, uint32& width, uint32& height);
}
