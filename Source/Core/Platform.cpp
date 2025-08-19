#include "Platform.h"

#include "Application.h"
#include "Memory.h"
#include "Pool.h"

#if BK_PLATFORM_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <shellapi.h>

#undef MoveFile
#undef DeleteFile
#undef CreateDirectory
#undef CreateProcess
#undef CreateWindow

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "shell32.lib")
#else
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <spawn.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

extern char** environ;
#endif

#if BK_PLATFORM_EMSCRIPTEN
#include <emscripten/dom_pk_codes.h>
#include <emscripten/html5.h>
#endif

namespace Bk
{
	struct Window
	{
#if BK_PLATFORM_WINDOWS
		HWND handle;
#elif BK_PLATFORM_EMSCRIPTEN
		const char* target;
#else
		uint64 placeholder;
#endif
	};

	struct
	{
		Arena arena;

		TPool<Window> windows;
	} platformContext;

	Platform GetPlatform()
	{
#if BK_PLATFORM_WINDOWS
		return Platform::Windows;
#elif BK_PLATFORM_MACOS
		return Platform::MacOS;
#elif BK_PLATFORM_EMSCRIPTEN
		return Platform::Emscripten;
#endif
	}

	String GetPlatformName()
	{
		Platform platform = GetPlatform();
		return GetPlatformName(platform);
	}

	String GetPlatformName(Platform platform)
	{
		switch (platform)
		{
			case Platform::Windows: return "Windows";
			case Platform::MacOS: return "MacOS";
			case Platform::Emscripten: return "Emscripten";
		}
	}

	uint64 GetCpuTicks()
	{
#if BK_PLATFORM_WINDOWS
		LARGE_INTEGER counter;
		QueryPerformanceCounter(&counter);

		return static_cast<uint64>(counter.QuadPart);
#else
		timespec ts = {};
		clock_gettime(CLOCK_MONOTONIC, &ts);

		return static_cast<uint64>(ts.tv_sec * 1'000'000'000ll + ts.tv_nsec);
#endif
	}

	uint64 GetCpuFrequency()
	{
#if BK_PLATFORM_WINDOWS
		LARGE_INTEGER frequency;
		QueryPerformanceFrequency(&frequency);

		return static_cast<uint64>(frequency.QuadPart);
#else
		return 1'000'000'000ull;
#endif
	}

	uint64 GetTimeMs()
	{
		uint64 period = GetCpuFrequency();
		return (GetCpuTicks() * 1'000) / period;
	}

	double GetTimeSec()
	{
		double period = 1.0 / static_cast<double>(GetCpuFrequency());
		return static_cast<double>(GetCpuTicks()) * period;
	}

	DateTime GetUtcTime()
	{
		DateTime result = {};

#if BK_PLATFORM_WINDOWS
		SYSTEMTIME systemTime;
		GetSystemTime(&systemTime);

		result.year = systemTime.wYear;
		result.month = static_cast<uint8>(systemTime.wMonth);
		result.weekday = static_cast<uint8>(systemTime.wDayOfWeek);
		result.day = static_cast<uint8>(systemTime.wDay);
		result.hour = static_cast<uint8>(systemTime.wHour);
		result.minute = static_cast<uint8>(systemTime.wMinute);
		result.second = static_cast<uint8>(systemTime.wSecond);
		result.millisecond = systemTime.wMilliseconds;
#else
		timeval time = {};
		gettimeofday(&time, nullptr);

		tm utcTime = {};
		gmtime_r(&time.tv_sec, &utcTime);

		result.year = static_cast<uint16>(utcTime.tm_year + 1900);
		result.month = static_cast<uint8>(utcTime.tm_mon + 1);
		result.weekday = static_cast<uint8>(utcTime.tm_wday);
		result.day = static_cast<uint8>(utcTime.tm_mday);
		result.hour = static_cast<uint8>(utcTime.tm_hour);
		result.minute = static_cast<uint8>(utcTime.tm_min);
		result.second = static_cast<uint8>(utcTime.tm_sec);
		result.millisecond = static_cast<uint16>(time.tv_usec / 1000);
#endif

		return result;
	}

	DateTime GetLocalTime()
	{
		DateTime result = {};

#if BK_PLATFORM_WINDOWS
		SYSTEMTIME systemTime;
		GetLocalTime(&systemTime);

		result.year = systemTime.wYear;
		result.month = static_cast<uint8>(systemTime.wMonth);
		result.weekday = static_cast<uint8>(systemTime.wDayOfWeek);
		result.day = static_cast<uint8>(systemTime.wDay);
		result.hour = static_cast<uint8>(systemTime.wHour);
		result.minute = static_cast<uint8>(systemTime.wMinute);
		result.second = static_cast<uint8>(systemTime.wSecond);
		result.millisecond = systemTime.wMilliseconds;
#else
		timeval time = {};
		gettimeofday(&time, nullptr);

		tm localTime = {};
		localtime_r(&time.tv_sec, &localTime);

		result.year = static_cast<uint16>(localTime.tm_year + 1900);
		result.month = static_cast<uint8>(localTime.tm_mon + 1);
		result.weekday = static_cast<uint8>(localTime.tm_wday);
		result.day = static_cast<uint8>(localTime.tm_mday);
		result.hour = static_cast<uint8>(localTime.tm_hour);
		result.minute = static_cast<uint8>(localTime.tm_min);
		result.second = static_cast<uint8>(localTime.tm_sec);
		result.millisecond = static_cast<uint16>(time.tv_usec / 1000);
#endif

		return result;
	}

	uint64 GetUnixTime()
	{
#if BK_PLATFORM_WINDOWS
		FILETIME fileTime = {};
		GetSystemTimeAsFileTime(&fileTime);

		constexpr uint64 epochOffset = 0x019DB1DED53E8000ull;
		constexpr uint64 period = 10'000'000ull; // 1sec in 100ns intervals

		return ((static_cast<uint64>(fileTime.dwHighDateTime) << 32 | fileTime.dwLowDateTime) - epochOffset) / period;
#else
		time_t result = time(nullptr);
		return static_cast<uint64>(result);
#endif
	}

#if BK_PLATFORM_WINDOWS
	wchar_t* ConvertString(Arena& arena, String string)
	{
		int32 length = MultiByteToWideChar(CP_UTF8, 0, string.data, string.length, nullptr, 0);
		if (length == 0)
		{
			return nullptr;
		}

		wchar_t* result = arena.Push<wchar_t>(length + 1);
		MultiByteToWideChar(CP_UTF8, 0, string.data, string.length, result, length);
		result[length] = '\0';

		return result;
	}

	String ConvertString(Arena& arena, const wchar_t* string)
	{
		size_t stringLength = wcslen(string);

		int32 length = WideCharToMultiByte(CP_UTF8, 0, string, stringLength, nullptr, 0, nullptr, nullptr);
		if (length == 0)
		{
			return String::Empty;
		}

		TSpan<char> result = arena.Push<char>(length);
		WideCharToMultiByte(CP_UTF8, 0, string, stringLength, result.data, result.length, nullptr, nullptr);

		return String(result.data, result.length);
	}

	DateTime ConvertFileTime(FILETIME time)
	{
		DateTime result = {};

		SYSTEMTIME systemTime = {};
		if (FileTimeToSystemTime(&time, &systemTime))
		{
			result.year = systemTime.wYear;
			result.month = static_cast<uint8>(systemTime.wMonth);
			result.weekday = static_cast<uint8>(systemTime.wDayOfWeek);
			result.day = static_cast<uint8>(systemTime.wDay);
			result.hour = static_cast<uint8>(systemTime.wHour);
			result.minute = static_cast<uint8>(systemTime.wMinute);
			result.second = static_cast<uint8>(systemTime.wSecond);
			result.millisecond = systemTime.wMilliseconds;
		}

		return result;
	}
#else
	char* ConvertString(Arena& arena, String string)
	{
		if (string.length == 0)
		{
			return nullptr;
		}

		char* result = arena.Push<char>(string.length + 1);
		MemoryCopy(result, string.data, string.length);
		result[string.length] = '\0';

		return result;
	}

	DateTime ConvertFileTime(time_t time)
	{
		DateTime result = {};

		tm utcTime = {};
		gmtime_r(&time, &utcTime);

		result.year = static_cast<uint16>(utcTime.tm_year + 1900);
		result.month = static_cast<uint8>(utcTime.tm_mon + 1);
		result.weekday = static_cast<uint8>(utcTime.tm_wday);
		result.day = static_cast<uint8>(utcTime.tm_mday);
		result.hour = static_cast<uint8>(utcTime.tm_hour);
		result.minute = static_cast<uint8>(utcTime.tm_min);
		result.second = static_cast<uint8>(utcTime.tm_sec);
		result.millisecond = 0;

		return result;
	}
#endif

	FileHandle OpenFile(String path, FileAccess access)
	{
		ArenaScope scratch = GetScratchArena();

#if BK_PLATFORM_WINDOWS
		wchar_t* filePath = ConvertString(scratch.arena, path);

		DWORD accessFlags = 0;
		DWORD disposition = OPEN_EXISTING;

		if (EnumHasAnyFlags(access, FileAccess::Read))
		{
			accessFlags |= GENERIC_READ;
		}

		if (EnumHasAnyFlags(access, FileAccess::Write))
		{
			accessFlags |= GENERIC_WRITE;
			disposition = CREATE_ALWAYS;
		}

		if (EnumHasAnyFlags(access, FileAccess::Append))
		{
			accessFlags |= FILE_APPEND_DATA;
			disposition = OPEN_ALWAYS;
		}

		HANDLE fileHandle = CreateFileW(filePath, accessFlags, 0, nullptr, disposition, FILE_ATTRIBUTE_NORMAL, nullptr);
		return (fileHandle != INVALID_HANDLE_VALUE) ? reinterpret_cast<FileHandle>(fileHandle) : 0;
#else
		char* filePath = ConvertString(scratch.arena, path);

		int flags = 0;
		if (EnumHasAllFlags(access, FileAccess::Read | FileAccess::Write))
		{
			flags = O_RDWR;
		}
		else if (EnumHasAnyFlags(access, FileAccess::Read))
		{
			flags = O_RDONLY;
		}
		else if (EnumHasAnyFlags(access, FileAccess::Write))
		{
			flags = O_WRONLY;
		}

		if (EnumHasAnyFlags(access, FileAccess::Write | FileAccess::Append))
		{
			flags |= O_CREAT;
		}

		if (EnumHasAnyFlags(access, FileAccess::Append))
		{
			flags |= O_APPEND;
		}

		int fileHandle = open(filePath, flags, 0755);
		return (fileHandle != -1) ? static_cast<FileHandle>(fileHandle) : 0;
#endif
	}

	void CloseFile(FileHandle handle)
	{
		if (!handle)
		{
			return;
		}

#if BK_PLATFORM_WINDOWS
		HANDLE fileHandle = reinterpret_cast<HANDLE>(handle);
		CloseHandle(fileHandle);
#else
		int fileHandle = static_cast<int>(handle);
		close(fileHandle);
#endif
	}

	size_t ReadFile(FileHandle handle, TSpan<uint8> buffer)
	{
		if (!handle)
		{
			return 0;
		}

#if BK_PLATFORM_WINDOWS
		HANDLE fileHandle = reinterpret_cast<HANDLE>(handle);

		size_t totalBytesRead = 0;
		size_t bytesLeft = buffer.length;

		while (bytesLeft > 0)
		{
			// OVERLAPPED overlapped = {};
			// overlapped.Offset = static_cast<DWORD>(totalBytesRead);
			// overlapped.OffsetHigh = static_cast<DWORD>(totalBytesRead >> 32);

			DWORD bytesRead;
			BOOL result = ::ReadFile(fileHandle, buffer.data + totalBytesRead, bytesLeft, &bytesRead, nullptr);

			if (result)
			{
				totalBytesRead += bytesRead;
				bytesLeft -= bytesRead;
			}
			else
			{
				break;
			}
		}

		return totalBytesRead;
#else
		int fileHandle = static_cast<int>(handle);

		size_t totalBytesRead = 0;
		size_t bytesLeft = buffer.length;

		while (bytesLeft > 0)
		{
			ssize_t bytesRead = read(fileHandle, buffer.data + totalBytesRead, bytesLeft);
			if (bytesRead > 0)
			{
				totalBytesRead += static_cast<size_t>(bytesRead);
				bytesLeft -= static_cast<size_t>(bytesRead);
			}
			else if (errno != EINTR)
			{
				break;
			}
		}

		return totalBytesRead;
#endif
	}

	size_t WriteFile(FileHandle handle, TSpan<uint8> buffer)
	{
		if (!handle)
		{
			return 0;
		}

#if BK_PLATFORM_WINDOWS
		HANDLE fileHandle = reinterpret_cast<HANDLE>(handle);

		size_t totalBytesWritten = 0;
		size_t bytesLeft = buffer.length;

		while (bytesLeft > 0)
		{
			// OVERLAPPED overlapped = {};
			// overlapped.Offset = static_cast<DWORD>(totalBytesWritten);
			// overlapped.OffsetHigh = static_cast<DWORD>(totalBytesWritten >> 32);

			DWORD bytesWritten;
			BOOL result = ::WriteFile(fileHandle, buffer.data + totalBytesWritten, bytesLeft, &bytesWritten, nullptr);

			if (result)
			{
				totalBytesWritten += bytesWritten;
				bytesLeft -= bytesWritten;
			}
			else
			{
				break;
			}
		}

		return totalBytesWritten;
#else
		int fileHandle = static_cast<int>(handle);

		size_t totalBytesWritten = 0;
		size_t bytesLeft = buffer.length;

		while (bytesLeft > 0)
		{
			ssize_t bytesWritten = write(fileHandle, buffer.data + totalBytesWritten, bytesLeft);
			if (bytesWritten > 0)
			{
				totalBytesWritten += static_cast<size_t>(bytesWritten);
				bytesLeft -= static_cast<size_t>(bytesWritten);
			}
			else if (errno != EINTR)
			{
				break;
			}
		}

		return totalBytesWritten;
#endif
	}

	size_t GetFileSize(FileHandle handle)
	{
		if (!handle)
		{
			return 0;
		}

#if BK_PLATFORM_WINDOWS
		HANDLE fileHandle = reinterpret_cast<HANDLE>(handle);

		LARGE_INTEGER fileSize = {};
		GetFileSizeEx(fileHandle, &fileSize);

		return static_cast<size_t>(fileSize.QuadPart);
#else
		int fileHandle = static_cast<int>(handle);

		struct stat fileStat = {};
		fstat(fileHandle, &fileStat);

		return static_cast<size_t>(fileStat.st_size);
#endif
	}

	FileProperties GetFileProperties(FileHandle handle)
	{
		FileProperties result = {};

		if (!handle)
		{
			return result;
		}

#if BK_PLATFORM_WINDOWS
		HANDLE fileHandle = reinterpret_cast<HANDLE>(handle);

		BY_HANDLE_FILE_INFORMATION fileInfo;
		if (GetFileInformationByHandle(fileHandle, &fileInfo))
		{
			result.size = static_cast<size_t>(fileInfo.nFileSizeHigh) << 32 | fileInfo.nFileSizeLow;
			result.createdTime = ConvertFileTime(fileInfo.ftCreationTime);
			result.modifiedTime = ConvertFileTime(fileInfo.ftLastWriteTime);

			if ((fileInfo.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
			{
				result.attributes |= FileAttributes::Directory;
			}

			if ((fileInfo.dwFileAttributes & FILE_ATTRIBUTE_READONLY) != 0)
			{
				result.attributes |= FileAttributes::ReadOnly;
			}
		}
#else
		int fileHandle = static_cast<int>(handle);

		struct stat fileStat = {};
		if (fstat(fileHandle, &fileStat) != -1)
		{
			result.size = static_cast<size_t>(fileStat.st_size);
			result.createdTime = ConvertFileTime(fileStat.st_ctime);
			result.modifiedTime = ConvertFileTime(fileStat.st_mtime);

			if ((fileStat.st_mode & S_IFDIR) != 0)
			{
				result.attributes |= FileAttributes::Directory;
			}

			if ((fileStat.st_mode & S_IWUSR) == 0)
			{
				result.attributes |= FileAttributes::ReadOnly;
			}
		}
#endif

		return result;
	}

	FileProperties GetFileProperties(String path)
	{
		ArenaScope scratch = GetScratchArena();

		FileProperties result = {};

#if BK_PLATFORM_WINDOWS
		wchar_t* filePath = ConvertString(scratch.arena, path);

		WIN32_FIND_DATAW findInfo = {};
		HANDLE findHandle = FindFirstFileW(filePath, &findInfo);

		if (findHandle != INVALID_HANDLE_VALUE)
		{
			result.size = static_cast<size_t>(findInfo.nFileSizeHigh) << 32 | findInfo.nFileSizeLow;
			result.createdTime = ConvertFileTime(findInfo.ftCreationTime);
			result.modifiedTime = ConvertFileTime(findInfo.ftLastWriteTime);

			if ((findInfo.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
			{
				result.attributes |= FileAttributes::Directory;
			}

			if ((findInfo.dwFileAttributes & FILE_ATTRIBUTE_READONLY) != 0)
			{
				result.attributes |= FileAttributes::ReadOnly;
			}

			FindClose(findHandle);
		}
#else
		char* filePath = ConvertString(scratch.arena, path);

		struct stat fileStat = {};
		if (stat(filePath, &fileStat) != -1)
		{
			result.size = static_cast<size_t>(fileStat.st_size);
			result.createdTime = ConvertFileTime(fileStat.st_ctime);
			result.modifiedTime = ConvertFileTime(fileStat.st_mtime);

			if ((fileStat.st_mode & S_IFDIR) != 0)
			{
				result.attributes |= FileAttributes::Directory;
			}

			if ((fileStat.st_mode & S_IWUSR) == 0)
			{
				result.attributes |= FileAttributes::ReadOnly;
			}
		}
#endif

		return result;
	}

	bool FileExists(String path)
	{
		ArenaScope scratch = GetScratchArena();

#if BK_PLATFORM_WINDOWS
		wchar_t* filePath = ConvertString(scratch.arena, path);
		uint32 fileAttributes = GetFileAttributesW(filePath);

		return fileAttributes != INVALID_FILE_ATTRIBUTES && (fileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
#else
		char* filePath = ConvertString(scratch.arena, path);
		return access(filePath, F_OK) == 0;
#endif
	}

	bool CopyFile(String srcPath, String dstPath)
	{
		ArenaScope scratch = GetScratchArena();

#if BK_PLATFORM_WINDOWS
		wchar_t* srcFilePath = ConvertString(scratch.arena, srcPath);
		wchar_t* dstFilePath = ConvertString(scratch.arena, dstPath);

		return CopyFileW(srcFilePath, dstFilePath, false);
#else
		FileHandle srcFile = OpenFile(srcPath, FileAccess::Read);
		FileHandle dstFile = OpenFile(dstPath, FileAccess::Write);

		size_t bytesLeft = GetFileSize(srcFile);

		size_t bufferCapacity = Min(bytesLeft, BK_MEGABYTES(1));
		TSpan<uint8> buffer = scratch.arena.Push<uint8>(bufferCapacity);

		while (bytesLeft > 0)
		{
			buffer.length = Min(bufferCapacity, bytesLeft);

			size_t bytesRead = ReadFile(srcFile, buffer);
			if (bytesRead != buffer.length)
			{
				break;
			}

			size_t bytesWritten = WriteFile(dstFile, buffer);
			if (bytesWritten != buffer.length)
			{
				break;
			}

			bytesLeft -= bytesWritten;
		}

		CloseFile(srcFile);
		CloseFile(dstFile);

		return bytesLeft == 0;
#endif
	}

	bool MoveFile(String srcPath, String dstPath)
	{
		ArenaScope scratch = GetScratchArena();

#if BK_PLATFORM_WINDOWS
		wchar_t* srcFilePath = ConvertString(scratch.arena, srcPath);
		wchar_t* dstFilePath = ConvertString(scratch.arena, dstPath);

		return MoveFileW(srcFilePath, dstFilePath);
#else
		char* srcFilePath = ConvertString(scratch.arena, srcPath);
		char* dstFilePath = ConvertString(scratch.arena, dstPath);

		return rename(srcFilePath, dstFilePath) == 0;
#endif
	}

	bool DeleteFile(String path)
	{
		ArenaScope scratch = GetScratchArena();

#if BK_PLATFORM_WINDOWS
		wchar_t* filePath = ConvertString(scratch.arena, path);
		return DeleteFileW(filePath);
#else
		char* filePath = ConvertString(scratch.arena, path);
		return unlink(filePath) == 0;
#endif
	}

	bool CreateDirectory(String path)
	{
		ArenaScope scratch = GetScratchArena();

#if BK_PLATFORM_WINDOWS
		wchar_t* filePath = ConvertString(scratch.arena, path);
		return CreateDirectoryW(filePath, nullptr) || GetLastError() == ERROR_ALREADY_EXISTS;
#else
		char* filePath = ConvertString(scratch.arena, path);
		return mkdir(filePath, 0755) == 0 || errno == EEXIST;
#endif
	}

	bool CopyDirectory(String srcPath, String dstPath)
	{
		ArenaScope scratch = GetScratchArena();

		StringBuilder builder(scratch.arena);
		builder.AppendPath(dstPath);
		size_t dstPathLength = builder.length;

		FileIteratorHandle fileIt = CreateFileIterator(scratch.arena, srcPath);
		for (FileIteratorEntry entry; AdvanceFileIterator(fileIt, entry);)
		{
			if (EnumHasAllFlags(entry.properties.attributes, FileAttributes::Directory))
			{
				continue;
			}

			builder.Reset(dstPathLength);
			builder.AppendPath(GetFileName(entry.path));

			String srcFilePath = entry.path;
			String dstFilePath = builder.ToString(scratch.arena);

			if (!CopyFile(srcFilePath, dstFilePath))
			{
				return false;
			}
		}

		return true;
	}

	struct FileIterator
	{
		Arena* arena;

		StringBuilder pathBuilder;
		size_t pathLength;

#if BK_PLATFORM_WINDOWS
		HANDLE findHandle;
		WIN32_FIND_DATAW findInfo;
#else
		DIR* dirHandle;
#endif
	};

	FileIteratorHandle CreateFileIterator(Arena& arena, String path)
	{
		ArenaScope scratch = GetScratchArena(&arena);

		FileIterator* iterator = arena.Push<FileIterator>();
		iterator->arena = &arena;
		iterator->pathBuilder = StringBuilder(arena);
		iterator->pathBuilder.AppendPath(path);
		iterator->pathLength = iterator->pathBuilder.length;

#if BK_PLATFORM_WINDOWS
		iterator->pathBuilder.AppendPath("*");

		wchar_t* filePath = ConvertString(scratch.arena, iterator->pathBuilder.ToString(scratch.arena));
		iterator->findHandle = FindFirstFileW(filePath, &iterator->findInfo);
#else
		char* filePath = ConvertString(scratch.arena, path);
		iterator->dirHandle = opendir(filePath);
#endif

		return iterator;
	}

	bool AdvanceFileIterator(FileIteratorHandle iterator, FileIteratorEntry& entry)
	{
		if (!iterator)
		{
			return false;
		}

		bool result = false;

#if BK_PLATFORM_WINDOWS
		ArenaScope scratch = GetScratchArena(iterator->arena);
		while (iterator->findHandle != INVALID_HANDLE_VALUE)
		{
			String fileName = ConvertString(scratch.arena, iterator->findInfo.cFileName);
			if (fileName != "." && fileName != "..")
			{
				iterator->pathBuilder.Reset(iterator->pathLength);
				iterator->pathBuilder.AppendPath(fileName);

				entry.path = iterator->pathBuilder.ToString(*iterator->arena);
				entry.properties = {};

				entry.properties.size = static_cast<size_t>(iterator->findInfo.nFileSizeHigh) << 32 | iterator->findInfo.nFileSizeLow;
				entry.properties.createdTime = ConvertFileTime(iterator->findInfo.ftCreationTime);
				entry.properties.modifiedTime = ConvertFileTime(iterator->findInfo.ftLastWriteTime);

				if ((iterator->findInfo.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
				{
					entry.properties.attributes |= FileAttributes::Directory;
				}

				if ((iterator->findInfo.dwFileAttributes & FILE_ATTRIBUTE_READONLY) != 0)
				{
					entry.properties.attributes |= FileAttributes::ReadOnly;
				}

				result = true;
			}

			if (!FindNextFileW(iterator->findHandle, &iterator->findInfo))
			{
				FindClose(iterator->findHandle);
				iterator->findHandle = INVALID_HANDLE_VALUE;
			}

			if (result)
			{
				break;
			}
		}
#else
		dirent* entryHandle;
		while ((entryHandle = readdir(iterator->dirHandle)) != nullptr)
		{
			String fileName = entryHandle->d_name;
			if (fileName == "." || fileName == "..")
			{
				continue;
			}

			iterator->pathBuilder.Reset(iterator->pathLength);
			iterator->pathBuilder.AppendPath(fileName);

			entry.path = iterator->pathBuilder.ToString(*iterator->arena, true);
			entry.properties = {};

			struct stat fileStat = {};
			if (stat(entry.path.data, &fileStat) != -1)
			{
				entry.properties.size = static_cast<size_t>(fileStat.st_size);
				entry.properties.createdTime = ConvertFileTime(fileStat.st_ctime);
				entry.properties.modifiedTime = ConvertFileTime(fileStat.st_mtime);

				if ((fileStat.st_mode & S_IFDIR) != 0)
				{
					entry.properties.attributes |= FileAttributes::Directory;
				}

				if ((fileStat.st_mode & S_IWUSR) == 0)
				{
					entry.properties.attributes |= FileAttributes::ReadOnly;
				}
			}

			result = true;
			break;
		}
#endif

		return result;
	}

	void DestroyFileIterator(FileIteratorHandle iterator)
	{
		if (!iterator)
		{
			return;
		}

#if BK_PLATFORM_WINDOWS
		if (iterator->findHandle != INVALID_HANDLE_VALUE)
		{
			FindClose(iterator->findHandle);
			iterator->findHandle = INVALID_HANDLE_VALUE;
		}
#else
		if (iterator->dirHandle)
		{
			closedir(iterator->dirHandle);
			iterator->dirHandle = nullptr;
		}
#endif
	}

	ProcessHandle CreateProcess(const ProcessParams& params)
	{
		ArenaScope scratch = GetScratchArena();

#if BK_PLATFORM_WINDOWS
		int32 executableLength = MultiByteToWideChar(CP_UTF8, 0, params.executable.data, params.executable.length, nullptr, 0);
		int32 argumentsLength = MultiByteToWideChar(CP_UTF8, 0, params.arguments.data, params.arguments.length, nullptr, 0);

		size_t commandLineLength = executableLength + 3 + argumentsLength;
		wchar_t* commandLine = scratch.arena.Push<wchar_t>(commandLineLength + 1);

		MultiByteToWideChar(CP_UTF8, 0, params.executable.data, params.executable.length, commandLine + 1, executableLength);
		commandLine[0] = '\"';
		commandLine[executableLength + 1] = '\"';
		commandLine[executableLength + 2] = ' ';

		MultiByteToWideChar(CP_UTF8, 0, params.arguments.data, params.arguments.length, commandLine + executableLength + 3, argumentsLength);
		commandLine[commandLineLength] = '\0';

		STARTUPINFOW startupInfo = {};
		startupInfo.cb = sizeof(startupInfo);

		uint32 flags = CREATE_UNICODE_ENVIRONMENT;
		wchar_t* environment = nullptr;

		PROCESS_INFORMATION processInfo = {};

		if (!CreateProcessW(nullptr, commandLine, nullptr, nullptr, false, flags, environment, nullptr, &startupInfo, &processInfo))
		{
			return 0;
		}

		CloseHandle(processInfo.hThread);

		return reinterpret_cast<ProcessHandle>(processInfo.hProcess);
#else
		TSpan<char*> arguments = scratch.arena.Push<char*>(1024);
		arguments[0] = ConvertString(scratch.arena, params.executable.TrimQuotes());

		String argumentStream = params.arguments;
		for (size_t i = 1; i < arguments.length; ++i)
		{
			String argument = {};
			if (!ParseToken(argumentStream, argument))
			{
				arguments[i] = nullptr;
				break;
			}

			arguments[i] = ConvertString(scratch.arena, argument.TrimQuotes());
		}

		int processHandle;
		if (posix_spawnp(&processHandle, arguments[0], nullptr, nullptr, arguments, environ) != 0)
		{
			return 0;
		}

		return static_cast<ProcessHandle>(processHandle);
#endif
	}

	void DetachProcess(ProcessHandle handle)
	{
		if (!handle)
		{
			return;
		}

#if BK_PLATFORM_WINDOWS
		HANDLE processHandle = reinterpret_cast<HANDLE>(handle);
		CloseHandle(processHandle);
#endif
	}

	bool WaitForProcess(ProcessHandle handle, int32* exitCode)
	{
		if (!handle)
		{
			return false;
		}

#if BK_PLATFORM_WINDOWS
		HANDLE processHandle = reinterpret_cast<HANDLE>(handle);

		bool result = WaitForSingleObject(processHandle, INFINITE) == WAIT_OBJECT_0;
		if (result && exitCode != nullptr)
		{
			result = GetExitCodeProcess(processHandle, reinterpret_cast<DWORD*>(exitCode));
		}

		return result;
#else
		int processHandle = static_cast<int>(handle);

		int processStatus;
		bool result = waitpid(processHandle, &processStatus, 0) == processHandle;
		if (result && exitCode != nullptr)
		{
			*exitCode = WIFEXITED(processStatus) ? WEXITSTATUS(processStatus) : EXIT_FAILURE;
		}

		return result;
#endif
	}

	bool TerminateProcess(ProcessHandle handle)
	{
		if (!handle)
		{
			return false;
		}

#if BK_PLATFORM_WINDOWS
		HANDLE processHandle = reinterpret_cast<HANDLE>(handle);
		return ::TerminateProcess(processHandle, 0);
#else
		int processHandle = static_cast<int>(handle);
		return kill(processHandle, SIGKILL) == 0;
#endif
	}

#if BK_PLATFORM_WINDOWS
	KeyCode ConvertScanCode(uint32 scanCode)
	{
		switch (scanCode)
		{
#define CASE(to, from) case from: return KeyCode::to

			CASE(Backspace, 0x0E);
			CASE(Tab, 0x0F);
			CASE(Enter, 0x1C);
			CASE(Escape, 0x01);
			CASE(Spacebar, 0x39);
			CASE(Apostrophe, 0x28);
			CASE(Comma, 0x33);
			CASE(Minus, 0x0C);
			CASE(Period, 0x34);
			CASE(Slash, 0x35);

			CASE(Zero, 0x0B);
			CASE(One, 0x02);
			CASE(Two, 0x03);
			CASE(Three, 0x04);
			CASE(Four, 0x05);
			CASE(Five, 0x06);
			CASE(Six, 0x07);
			CASE(Seven, 0x08);
			CASE(Eight, 0x09);
			CASE(Nine, 0x0A);

			CASE(Semicolon, 0x27);
			CASE(Equal, 0x0D);

			CASE(A, 0x1E);
			CASE(B, 0x30);
			CASE(C, 0x2E);
			CASE(D, 0x20);
			CASE(E, 0x12);
			CASE(F, 0x21);
			CASE(G, 0x22);
			CASE(H, 0x23);
			CASE(I, 0x17);
			CASE(J, 0x24);
			CASE(K, 0x25);
			CASE(L, 0x26);
			CASE(M, 0x32);
			CASE(N, 0x31);
			CASE(O, 0x18);
			CASE(P, 0x19);
			CASE(Q, 0x10);
			CASE(R, 0x13);
			CASE(S, 0x1F);
			CASE(T, 0x14);
			CASE(U, 0x16);
			CASE(V, 0x2F);
			CASE(W, 0x11);
			CASE(X, 0x2D);
			CASE(Y, 0x15);
			CASE(Z, 0x2C);

			CASE(LeftBracket, 0x1A);
			CASE(Backslash, 0x2B);
			CASE(RightBracket, 0x1B);
			CASE(Grave, 0x29);

			CASE(Delete, 0xE053);

			CASE(ArrowUp, 0xE048);
			CASE(ArrowDown, 0xE050);
			CASE(ArrowLeft, 0xE04B);
			CASE(ArrowRight, 0xE04D);

			CASE(PrintScreen, 0xE037);
			CASE(ScrollLock, 0x46);
			CASE(Pause, 0xE11D45);

			CASE(Insert, 0xE052);
			CASE(Home, 0xE047);
			CASE(End, 0xE04F);
			CASE(PageUp, 0xE049);
			CASE(PageDown, 0xE051);

			CASE(LeftCtrl, 0x1D);
			CASE(LeftShift, 0x2A);
			CASE(LeftAlt, 0x38);
			CASE(LeftSuper, 0xE05B);

			CASE(RightCtrl, 0xE01D);
			CASE(RightShift, 0x36);
			CASE(RightAlt, 0xE038);
			CASE(RightSuper, 0xE05C);

			CASE(CapsLock, 0x3A);
			CASE(NumLock, 0x45);

			CASE(F1, 0x3B);
			CASE(F2, 0x3C);
			CASE(F3, 0x3D);
			CASE(F4, 0x3E);
			CASE(F5, 0x3F);
			CASE(F6, 0x40);
			CASE(F7, 0x41);
			CASE(F8, 0x42);
			CASE(F9, 0x43);
			CASE(F10, 0x44);
			CASE(F11, 0x57);
			CASE(F12, 0x58);

#undef CASE

			default: return KeyCode::Unknown;
		}
	}

	LRESULT WindowProc(HWND handle, UINT msg, WPARAM wParam, LPARAM lParam)
	{
		ArenaScope scratch = GetScratchArena();
		Window* window = reinterpret_cast<Window*>(GetWindowLongPtr(handle, GWLP_USERDATA));

		switch (msg)
		{
			case WM_CREATE:
			{
				CREATESTRUCT* createStruct = reinterpret_cast<CREATESTRUCT*>(lParam);
				SetWindowLongPtr(handle, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(createStruct->lpCreateParams));

				return 0;
			}

			case WM_CLOSE:
			{
				AppEvent appEvent = {};
				appEvent.type = AppEventType::WindowClose;
				appEvent.target = platformContext.windows.GetHandle(window);

				ProcessAppEvent(appEvent);

				break;
			}

			case WM_KEYDOWN:
			case WM_SYSKEYDOWN:
			case WM_KEYUP:
			case WM_SYSKEYUP:
			{
				uint16 flags = HIWORD(lParam);

				uint32 scanCode = LOBYTE(flags);
				if ((flags & KF_EXTENDED) != 0)
				{
					// NumLock/Clear comes through as 0xE045, keep as 0x45
					if (scanCode != 0x45)
					{
						scanCode |= 0xE000;
					}
				}
				else
				{
					// Pause/Break comes through as 0x45, fix up to 0xE11D45
					if (scanCode == 0x45)
					{
						scanCode = 0xE11D45;
					}
					// Alt+PrintScreen comes through as 0x54, fix up to 0xE037
					else if (scanCode == 0x54)
					{
						scanCode = 0xE037;
					}
				}

				AppEvent appEvent = {};
				appEvent.type = AppEventType::Key;
				appEvent.target = platformContext.windows.GetHandle(window);
				appEvent.keyCode = ConvertScanCode(scanCode);
				appEvent.keyPressed = (flags & KF_UP) == 0;

				ProcessAppEvent(appEvent);

				return 0;
			}

			case WM_LBUTTONDOWN:
			case WM_LBUTTONUP:
			{
				AppEvent appEvent = {};
				appEvent.type = AppEventType::Key;
				appEvent.target = platformContext.windows.GetHandle(window);
				appEvent.keyCode = KeyCode::LeftMouseButton;
				appEvent.keyPressed = msg == WM_LBUTTONDOWN;

				ProcessAppEvent(appEvent);

				return 0;
			}

			case WM_MBUTTONDOWN:
			case WM_MBUTTONUP:
			{
				AppEvent appEvent = {};
				appEvent.type = AppEventType::Key;
				appEvent.target = platformContext.windows.GetHandle(window);
				appEvent.keyCode = KeyCode::MiddleMouseButton;
				appEvent.keyPressed = msg == WM_MBUTTONDOWN;

				ProcessAppEvent(appEvent);

				return 0;
			}

			case WM_RBUTTONDOWN:
			case WM_RBUTTONUP:
			{
				AppEvent appEvent = {};
				appEvent.type = AppEventType::Key;
				appEvent.target = platformContext.windows.GetHandle(window);
				appEvent.keyCode = KeyCode::RightMouseButton;
				appEvent.keyPressed = msg == WM_RBUTTONDOWN;

				ProcessAppEvent(appEvent);

				return 0;
			}

			case WM_MOUSEMOVE:
			{
				AppEvent appEvent = {};

				appEvent.type = AppEventType::MouseMove;
				appEvent.target = platformContext.windows.GetHandle(window);
				appEvent.mouseX = static_cast<float>(LOWORD(lParam));
				appEvent.mouseY = static_cast<float>(HIWORD(lParam));

				ProcessAppEvent(appEvent);

				return 0;
			}

			case WM_MOUSEWHEEL:
			{
				AppEvent appEvent = {};
				appEvent.type = AppEventType::MouseWheel;
				appEvent.target = platformContext.windows.GetHandle(window);
				appEvent.wheelDelta = GET_WHEEL_DELTA_WPARAM(wParam) / static_cast<float>(WHEEL_DELTA);

				ProcessAppEvent(appEvent);

				return 0;
			}

			case WM_DROPFILES:
			{
				HDROP drop = reinterpret_cast<HDROP>(wParam);

				size_t numFiles = DragQueryFileW(drop, 0xFFFFFFFF, 0, 0);
				for (size_t i = 0; i < numFiles; ++i)
				{
					size_t fileNameLength = DragQueryFileW(drop, i, 0, 0) + 1;
					wchar_t* fileName = scratch.arena.Push<wchar_t>(fileNameLength);
					DragQueryFileW(drop, i, fileName, fileNameLength);

					AppEvent appEvent = {};
					appEvent.type = AppEventType::DropFile;
					appEvent.target = platformContext.windows.GetHandle(window);
					appEvent.dropFilePath = ConvertString(scratch.arena, fileName);

					ProcessAppEvent(appEvent);
				}

				DragFinish(drop);

				return 0;
			}

			default: break;
		}

		return DefWindowProc(handle, msg, wParam, lParam);
	}
#elif BK_PLATFORM_EMSCRIPTEN
	KeyCode ConvertKeyCode(const char* code)
	{
		switch (emscripten_compute_dom_pk_code(code))
		{
#define CASE(to, from) case from: return KeyCode::to

			CASE(Backspace, DOM_PK_BACKSPACE);
			CASE(Tab, DOM_PK_TAB);
			CASE(Enter, DOM_PK_ENTER);
			CASE(Escape, DOM_PK_ESCAPE);
			CASE(Spacebar, DOM_PK_SPACE);
			CASE(Apostrophe, DOM_PK_QUOTE);
			CASE(Comma, DOM_PK_COMMA);
			CASE(Minus, DOM_PK_MINUS);
			CASE(Period, DOM_PK_PERIOD);
			CASE(Slash, DOM_PK_SLASH);

			CASE(Zero, DOM_PK_0);
			CASE(One, DOM_PK_1);
			CASE(Two, DOM_PK_2);
			CASE(Three, DOM_PK_3);
			CASE(Four, DOM_PK_4);
			CASE(Five, DOM_PK_5);
			CASE(Six, DOM_PK_6);
			CASE(Seven, DOM_PK_7);
			CASE(Eight, DOM_PK_8);
			CASE(Nine, DOM_PK_9);

			CASE(Semicolon, DOM_PK_SEMICOLON);
			CASE(Equal, DOM_PK_EQUAL);

			CASE(A, DOM_PK_A);
			CASE(B, DOM_PK_B);
			CASE(C, DOM_PK_C);
			CASE(D, DOM_PK_D);
			CASE(E, DOM_PK_E);
			CASE(F, DOM_PK_F);
			CASE(G, DOM_PK_G);
			CASE(H, DOM_PK_H);
			CASE(I, DOM_PK_I);
			CASE(J, DOM_PK_J);
			CASE(K, DOM_PK_K);
			CASE(L, DOM_PK_L);
			CASE(M, DOM_PK_M);
			CASE(N, DOM_PK_N);
			CASE(O, DOM_PK_O);
			CASE(P, DOM_PK_P);
			CASE(Q, DOM_PK_Q);
			CASE(R, DOM_PK_R);
			CASE(S, DOM_PK_S);
			CASE(T, DOM_PK_T);
			CASE(U, DOM_PK_U);
			CASE(V, DOM_PK_V);
			CASE(W, DOM_PK_W);
			CASE(X, DOM_PK_X);
			CASE(Y, DOM_PK_Y);
			CASE(Z, DOM_PK_Z);

			CASE(LeftBracket, DOM_PK_BRACKET_LEFT);
			CASE(Backslash, DOM_PK_BACKSLASH);
			CASE(RightBracket, DOM_PK_BRACKET_RIGHT);
			CASE(Grave, DOM_PK_BACKQUOTE);

			CASE(Delete, DOM_PK_DELETE);

			CASE(ArrowUp, DOM_PK_ARROW_UP);
			CASE(ArrowDown, DOM_PK_ARROW_DOWN);
			CASE(ArrowLeft, DOM_PK_ARROW_LEFT);
			CASE(ArrowRight, DOM_PK_ARROW_RIGHT);

			CASE(PrintScreen, DOM_PK_PRINT_SCREEN);
			CASE(ScrollLock, DOM_PK_SCROLL_LOCK);
			CASE(Pause, DOM_PK_PAUSE);

			CASE(Insert, DOM_PK_INSERT);
			CASE(Home, DOM_PK_HOME);
			CASE(End, DOM_PK_END);
			CASE(PageUp, DOM_PK_PAGE_UP);
			CASE(PageDown, DOM_PK_PAGE_DOWN);

			CASE(LeftCtrl, DOM_PK_CONTROL_LEFT);
			CASE(LeftShift, DOM_PK_SHIFT_LEFT);
			CASE(LeftAlt, DOM_PK_ALT_LEFT);
			CASE(LeftSuper, DOM_PK_OS_LEFT);

			CASE(RightCtrl, DOM_PK_CONTROL_RIGHT);
			CASE(RightShift, DOM_PK_SHIFT_RIGHT);
			CASE(RightAlt, DOM_PK_ALT_RIGHT);
			CASE(RightSuper, DOM_PK_OS_RIGHT);

			CASE(CapsLock, DOM_PK_CAPS_LOCK);
			CASE(NumLock, DOM_PK_NUM_LOCK);

			CASE(F1, DOM_PK_F1);
			CASE(F2, DOM_PK_F2);
			CASE(F3, DOM_PK_F3);
			CASE(F4, DOM_PK_F4);
			CASE(F5, DOM_PK_F5);
			CASE(F6, DOM_PK_F6);
			CASE(F7, DOM_PK_F7);
			CASE(F8, DOM_PK_F8);
			CASE(F9, DOM_PK_F9);
			CASE(F10, DOM_PK_F10);
			CASE(F11, DOM_PK_F11);
			CASE(F12, DOM_PK_F12);

#undef CASE

			default: return KeyCode::Unknown;
		}
	}

	KeyCode ConvertMouseButton(const uint16 button)
	{
		switch (button)
		{
			case 0: return KeyCode::LeftMouseButton;
			case 1: return KeyCode::MiddleMouseButton;
			case 2: return KeyCode::RightMouseButton;

			default: return KeyCode::Unknown;
		}
	}

	bool ProcessKeyEvent(int32 type, const EmscriptenKeyboardEvent* event, void* userData)
	{
		bool result = false;

		if (type == EMSCRIPTEN_EVENT_KEYDOWN || type == EMSCRIPTEN_EVENT_KEYUP)
		{
			AppEvent appEvent = {};
			appEvent.type = AppEventType::Key;
			appEvent.target = platformContext.windows.GetHandle(static_cast<Window*>(userData));
			appEvent.keyCode = ConvertKeyCode(event->code);
			appEvent.keyPressed = type == EMSCRIPTEN_EVENT_KEYDOWN;

			result = ProcessAppEvent(appEvent);
		}

		return result;
	}

	bool ProcessMouseEvent(int32 type, const EmscriptenMouseEvent* event, void* userData)
	{
		bool result = false;

		if (type == EMSCRIPTEN_EVENT_MOUSEDOWN || type == EMSCRIPTEN_EVENT_MOUSEUP)
		{
			AppEvent appEvent = {};
			appEvent.type = AppEventType::Key;
			appEvent.target = platformContext.windows.GetHandle(static_cast<Window*>(userData));
			appEvent.keyCode = ConvertMouseButton(event->button);
			appEvent.keyPressed = type == EMSCRIPTEN_EVENT_MOUSEDOWN;

			result = ProcessAppEvent(appEvent);
		}
		else if (type == EMSCRIPTEN_EVENT_MOUSEMOVE)
		{
			AppEvent appEvent = {};
			appEvent.type = AppEventType::MouseMove;
			appEvent.target = platformContext.windows.GetHandle(static_cast<Window*>(userData));
			appEvent.mouseX = static_cast<float>(event->targetX);
			appEvent.mouseY = static_cast<float>(event->targetY);

			result = ProcessAppEvent(appEvent);
		}

		return result;
	}

	bool ProcessWheelEvent(int32 type, const EmscriptenWheelEvent* event, void* userData)
	{
		bool result = false;

		if (type == EMSCRIPTEN_EVENT_WHEEL)
		{
			AppEvent appEvent = {};
			appEvent.type = AppEventType::MouseWheel;
			appEvent.target = platformContext.windows.GetHandle(static_cast<Window*>(userData));
			appEvent.wheelDelta = static_cast<float>(event->deltaY);

			switch (event->deltaMode)
			{
				case DOM_DELTA_PIXEL: appEvent.wheelDelta *= 0.01f; break;
				case DOM_DELTA_LINE: appEvent.wheelDelta *= 0.3f; break;
				case DOM_DELTA_PAGE: appEvent.wheelDelta *= 80.0f; break;

				default: break;
			}

			result = ProcessAppEvent(appEvent);
		}

		return result;
	}

	bool ProcessResizeEvent(int32 type, const EmscriptenUiEvent* event, void* userData)
	{
		bool result = false;

		if (type == EMSCRIPTEN_EVENT_RESIZE)
		{
			AppEvent appEvent = {};
			appEvent.type = AppEventType::WindowResize;

			// #TODO: Notify all windows; update usage to check target
			// appEvent.target = ...;

			appEvent.windowWidth = event->windowInnerWidth;
			appEvent.windowHeight = event->windowInnerHeight;

			result = ProcessAppEvent(appEvent);
		}

		return result;
	}

	extern "C" EMSCRIPTEN_KEEPALIVE void ProcessDropEvent(Window* window, const char* filePath)
	{
		AppEvent appEvent = {};
		appEvent.type = AppEventType::DropFile;
		appEvent.target = platformContext.windows.GetHandle(window);
		appEvent.dropFilePath = filePath;

		ProcessAppEvent(appEvent);
	}
#endif

	uint32 CreateWindow(const WindowParams& params)
	{
		if (platformContext.windows.capacity == 0)
		{
			// #TODO: Move to a platform init routine
			platformContext.windows.Initialize(platformContext.arena, 128);

#if BK_PLATFORM_WINDOWS
			WNDCLASSW windowClass = {};
			windowClass.lpszClassName = L"BrinkWindowClass";
			windowClass.lpfnWndProc = WindowProc;
			windowClass.hInstance = GetModuleHandleW(nullptr);
			windowClass.hCursor = LoadCursorA(nullptr, IDC_ARROW);

			BK_ASSERT(RegisterClassW(&windowClass) != 0);
#elif BK_PLATFORM_EMSCRIPTEN
			emscripten_set_resize_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, false, ProcessResizeEvent);
#endif
		}

		uint32 handle = 0;
		Window* window = platformContext.windows.Acquire(&handle);

		if (window)
		{
#if BK_PLATFORM_WINDOWS
			wchar_t* windowName = ConvertString(platformContext.arena, params.title);

			uint32 windowStyle = WS_OVERLAPPEDWINDOW;
			uint32 windowStyleEx = WS_EX_APPWINDOW | WS_EX_ACCEPTFILES;

			window->handle = CreateWindowExW(
				windowStyleEx, L"BrinkWindowClass", windowName, windowStyle,
				CW_USEDEFAULT, CW_USEDEFAULT, params.width, params.height,
				nullptr, nullptr, nullptr, window);

			ShowWindow(window->handle, SW_SHOW);
#elif BK_PLATFORM_EMSCRIPTEN
			window->target = ConvertString(platformContext.arena, params.title);

			if (params.width != 0 || params.height != 0)
			{
				emscripten_set_canvas_element_size(window->target, params.width, params.height);
			}

			emscripten_set_keydown_callback(window->target, window, false, ProcessKeyEvent);
			emscripten_set_keyup_callback(window->target, window, false, ProcessKeyEvent);
			emscripten_set_mousedown_callback(window->target, window, false, ProcessMouseEvent);
			emscripten_set_mouseup_callback(window->target, window, false, ProcessMouseEvent);
			emscripten_set_mousemove_callback(window->target, window, false, ProcessMouseEvent);
			emscripten_set_wheel_callback(window->target, window, false, ProcessWheelEvent);

			EM_ASM({
				const target = document.querySelector(UTF8ToString($0));
				const window = $1;

				target.tabIndex = 0;
				target.focus();

				target.oncontextmenu = (e) => e.preventDefault();
				target.ondragover = (e) => e.preventDefault();
				target.ondrop = (e) =>
				{
					e.preventDefault();

					for (let i = 0; i < e.dataTransfer.files.length; ++i)
					{
						const file = e.dataTransfer.files[i];
						const fileReader = new FileReader();

						fileReader.onload = function()
						{
							const filePath = 'drop_' + i + '_' + file.name;
							const fileContent = new Uint8Array(fileReader.result);
							FS.writeFile(filePath, fileContent);

							const filePathUtf8 = stringToUTF8OnStack(filePath);
							_ProcessDropEvent(window, filePathUtf8);
						};

						fileReader.readAsArrayBuffer(file);
					}
				};
			}, window->target, window);
#endif
		}

		return handle;
	}

	void DestroyWindow(uint32 handle)
	{
		Window* window = platformContext.windows.Get(handle);
		if (window)
		{
#if BK_PLATFORM_WINDOWS
			DestroyWindow(window->handle);
#elif BK_PLATFORM_EMSCRIPTEN
			emscripten_set_keydown_callback(window->target, nullptr, false, nullptr);
			emscripten_set_keyup_callback(window->target, nullptr, false, nullptr);
			emscripten_set_mousedown_callback(window->target, nullptr, false, nullptr);
			emscripten_set_mouseup_callback(window->target, nullptr, false, nullptr);
			emscripten_set_mousemove_callback(window->target, nullptr, false, nullptr);
			emscripten_set_wheel_callback(window->target, nullptr, false, nullptr);

			EM_ASM({
				const target = document.querySelector(UTF8ToString($0));

				target.oncontextmenu = null;
				target.ondragover = null;
				target.ondrop = null;
			}, window->target);
#endif

			platformContext.windows.Release(handle);
		}
	}

	void* GetWindowSurfaceTarget(uint32 handle)
	{
		void* result = nullptr;

		Window* window = platformContext.windows.Get(handle);
		if (window)
		{
#if BK_PLATFORM_WINDOWS
			result = window->handle;
#elif BK_PLATFORM_EMSCRIPTEN
			result = const_cast<char*>(window->target);
#endif
		}

		return result;
	}

	bool GetWindowSurfaceSize(uint32 handle, uint32& width, uint32& height)
	{
		bool result = false;

		Window* window = platformContext.windows.Get(handle);
		if (window)
		{
#if BK_PLATFORM_WINDOWS
			RECT surfaceRect = {};
			GetClientRect(window->handle, &surfaceRect);

			width = surfaceRect.right - surfaceRect.left;
			height = surfaceRect.bottom - surfaceRect.top;

			result = true;
#elif BK_PLATFORM_EMSCRIPTEN
			double surfaceWidth, surfaceHeight;
			emscripten_get_element_css_size(window->target, &surfaceWidth, &surfaceHeight);

			width = static_cast<uint32>(surfaceWidth);
			height = static_cast<uint32>(surfaceHeight);

			result = true;
#endif
		}

		return result;
	}

	void PumpEvents()
	{
#if BK_PLATFORM_WINDOWS
		MSG message;
		while (PeekMessage(&message, NULL, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&message);
			DispatchMessage(&message);
		}
#endif
	}
}
