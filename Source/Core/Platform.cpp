#include "Platform.h"

#include "Memory.h"

#if BK_PLATFORM_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
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

namespace Bk
{
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
			return String();
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
	char* ConvertString(Arena& arena, String path)
	{
		if (path.length == 0)
		{
			return nullptr;
		}

		char* result = arena.Push<char>(path.length + 1);
		MemoryCopy(result, path.data, path.length);
		result[path.length] = '\0';

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

			if (fileInfo.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			{
				result.attributes |= FileAttributes::Directory;
			}
			if (fileInfo.dwFileAttributes & FILE_ATTRIBUTE_READONLY)
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

			if (findInfo.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			{
				result.attributes |= FileAttributes::Directory;
			}
			if (findInfo.dwFileAttributes & FILE_ATTRIBUTE_READONLY)
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

	void EnumerateDirectory(String path, EnumerateDirectoryCb callback, bool recursive)
	{
		ArenaScope scratch = GetScratchArena();

#if BK_PLATFORM_WINDOWS
		StringBuilder builder(scratch.arena);
		builder.Append(path);
		builder.Append("\\*");

		wchar_t* searchPath = ConvertString(scratch.arena, builder.ToString(scratch.arena));

		WIN32_FIND_DATAW findInfo = {};
		HANDLE findHandle = FindFirstFileW(searchPath, &findInfo);

		if (findHandle != INVALID_HANDLE_VALUE)
		{
			do
			{
				String fileName = ConvertString(scratch.arena, findInfo.cFileName);
				if (fileName == "." || fileName == "..")
				{
					continue;
				}

				builder.Reset();
				builder.Append(path);
				builder.Append('/');
				builder.Append(fileName);

				String filePath = builder.ToString(scratch.arena);
				FileProperties fileProps = {};

				fileProps.size = static_cast<size_t>(findInfo.nFileSizeHigh) << 32 | findInfo.nFileSizeLow;
				fileProps.createdTime = ConvertFileTime(findInfo.ftCreationTime);
				fileProps.modifiedTime = ConvertFileTime(findInfo.ftLastWriteTime);

				if (findInfo.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
				{
					fileProps.attributes |= FileAttributes::Directory;
				}

				if (findInfo.dwFileAttributes & FILE_ATTRIBUTE_READONLY)
				{
					fileProps.attributes |= FileAttributes::ReadOnly;
				}

				if (!callback(filePath, fileProps))
				{
					break;
				}

				if (recursive && EnumHasAnyFlags(fileProps.attributes, FileAttributes::Directory))
				{
					EnumerateDirectory(filePath, callback, true);
				}
			} while (FindNextFileW(findHandle, &findInfo));

			FindClose(findHandle);
		}
#else
		StringBuilder builder(scratch.arena);

		char* searchPath = ConvertString(scratch.arena, path);

		DIR* dirHandle = opendir(searchPath);
		if (dirHandle)
		{
			dirent* entryHandle;
			while ((entryHandle = readdir(dirHandle)) != nullptr)
			{
				String fileName = entryHandle->d_name;
				if (fileName == "." || fileName == "..")
				{
					continue;
				}

				builder.Reset();
				builder.Append(path);
				builder.Append('/');
				builder.Append(fileName);

				String filePath = builder.ToString(scratch.arena, true);
				FileProperties fileProps = {};

				struct stat fileStat = {};
				if (stat(filePath.data, &fileStat) != -1)
				{
					fileProps.size = static_cast<size_t>(fileStat.st_size);
					fileProps.createdTime = ConvertFileTime(fileStat.st_ctime);
					fileProps.modifiedTime = ConvertFileTime(fileStat.st_mtime);

					if ((fileStat.st_mode & S_IFDIR) != 0)
					{
						fileProps.attributes |= FileAttributes::Directory;
					}

					if ((fileStat.st_mode & S_IWUSR) == 0)
					{
						fileProps.attributes |= FileAttributes::ReadOnly;
					}
				}

				if (!callback(filePath, fileProps))
				{
					break;
				}

				if (recursive && EnumHasAnyFlags(fileProps.attributes, FileAttributes::Directory))
				{
					EnumerateDirectory(filePath, callback, true);
				}
			}

			closedir(dirHandle);
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

	void DestroyProcess(ProcessHandle handle)
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

	bool WaitForProcess(ProcessHandle handle)
	{
		if (!handle)
		{
			return false;
		}

#if BK_PLATFORM_WINDOWS
		HANDLE processHandle = reinterpret_cast<HANDLE>(handle);
		return WaitForSingleObject(processHandle, INFINITE) == WAIT_OBJECT_0;
#else
		int processHandle = static_cast<int>(handle);
		return waitpid(processHandle, nullptr, 0) == processHandle;
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
}
