#pragma once

#include "Memory.h"

#include <dirent.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <spawn.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

extern char** environ;

namespace Bk
{
	uint64 GetCpuTicks()
	{
		timespec ts = {};
		clock_gettime(CLOCK_MONOTONIC, &ts);

		return static_cast<uint64>(ts.tv_sec * 1'000'000'000ll + ts.tv_nsec);
	}

	uint64 GetCpuFrequency()
	{
		return 1'000'000'000ull;
	}

	DateTime GetUtcTime()
	{
		DateTime result = {};

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

		return result;
	}

	DateTime GetLocalTime()
	{
		DateTime result = {};

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

		return result;
	}

	uint64 GetUnixTime()
	{
		time_t result = time(nullptr);
		return static_cast<uint64>(result);
	}

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

	FileHandle OpenFile(String path, FileAccess access)
	{
		ArenaScope scratch = GetScratchArena();

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
	}

	void CloseFile(FileHandle handle)
	{
		if (!handle)
		{
			return;
		}

		int fileHandle = static_cast<int>(handle);
		close(fileHandle);
	}

	size_t ReadFile(FileHandle handle, TSpan<uint8> buffer)
	{
		if (!handle)
		{
			return 0;
		}

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
	}

	size_t WriteFile(FileHandle handle, TSpan<const uint8> buffer)
	{
		if (!handle)
		{
			return 0;
		}

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
	}

	size_t GetFileSize(FileHandle handle)
	{
		if (!handle)
		{
			return 0;
		}

		int fileHandle = static_cast<int>(handle);

		struct stat fileStat = {};
		fstat(fileHandle, &fileStat);

		return static_cast<size_t>(fileStat.st_size);
	}

	FileProperties GetFileProperties(FileHandle handle)
	{
		FileProperties result = {};

		if (!handle)
		{
			return result;
		}

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

		return result;
	}

	FileProperties GetFileProperties(String path)
	{
		ArenaScope scratch = GetScratchArena();

		FileProperties result = {};

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

		return result;
	}

	bool FileExists(String path)
	{
		ArenaScope scratch = GetScratchArena();

		char* filePath = ConvertString(scratch.arena, path);
		return access(filePath, F_OK) == 0;
	}

	bool CopyFile(String srcPath, String dstPath)
	{
		ArenaScope scratch = GetScratchArena();

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
	}

	bool MoveFile(String srcPath, String dstPath)
	{
		ArenaScope scratch = GetScratchArena();

		char* srcFilePath = ConvertString(scratch.arena, srcPath);
		char* dstFilePath = ConvertString(scratch.arena, dstPath);

		return rename(srcFilePath, dstFilePath) == 0;
	}

	bool DeleteFile(String path)
	{
		ArenaScope scratch = GetScratchArena();

		char* filePath = ConvertString(scratch.arena, path);
		return unlink(filePath) == 0;
	}

	bool CreateDirectory(String path)
	{
		ArenaScope scratch = GetScratchArena();

		char* filePath = ConvertString(scratch.arena, path);

		bool result = mkdir(filePath, 0755) == 0 || errno == EEXIST;
		if (!result)
		{
			for (char* c = filePath; *c != '\0'; ++c)
			{
				if (*c == '/')
				{
					*c = '\0';
					mkdir(filePath, 0755);
					*c = '/';
				}
			}

			result = mkdir(filePath, 0755) == 0 || errno == EEXIST;
		}

		return result;
	}

	struct FileIterator
	{
		Arena* arena;

		StringBuilder pathBuilder;
		size_t pathLength;

		DIR* dirHandle;
	};

	FileIteratorHandle CreateFileIterator(Arena& arena, String path)
	{
		ArenaScope scratch = GetScratchArena(&arena);

		FileIterator* iterator = arena.Push<FileIterator>();
		iterator->arena = &arena;
		iterator->pathBuilder = StringBuilder(arena);
		iterator->pathBuilder.AppendPath(path);
		iterator->pathLength = iterator->pathBuilder.length;

		char* filePath = ConvertString(scratch.arena, path);
		iterator->dirHandle = opendir(filePath);

		return iterator;
	}

	bool AdvanceFileIterator(FileIteratorHandle iterator, FileIteratorEntry& entry)
	{
		if (!iterator)
		{
			return false;
		}

		bool result = false;

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

		return result;
	}

	void DestroyFileIterator(FileIteratorHandle iterator)
	{
		if (!iterator)
		{
			return;
		}

		if (iterator->dirHandle)
		{
			closedir(iterator->dirHandle);
			iterator->dirHandle = nullptr;
		}
	}

	String GetCurrentDirectory(Arena& arena)
	{
		ArenaScope scratch = GetScratchArena(&arena);

		size_t pathLength = pathconf(".", _PC_PATH_MAX);
		char* path = scratch.arena.Push<char>(pathLength);
		(void)getcwd(path, pathLength);

		StringBuilder builder(scratch.arena);
		builder.Append(path);
		builder.NormalizePath();

		return builder.ToString(arena);
	}

	ProcessHandle CreateProcess(const ProcessParams& params)
	{
		ArenaScope scratch = GetScratchArena();

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
	}

	void DetachProcess(ProcessHandle handle)
	{
	}

	bool WaitForProcess(ProcessHandle handle, int32* exitCode)
	{
		if (!handle)
		{
			return false;
		}

		int processHandle = static_cast<int>(handle);

		int processStatus;
		bool result = waitpid(processHandle, &processStatus, 0) == processHandle;
		if (result && exitCode != nullptr)
		{
			*exitCode = WIFEXITED(processStatus) ? WEXITSTATUS(processStatus) : 1;
		}

		return result;
	}

	bool TerminateProcess(ProcessHandle handle)
	{
		if (!handle)
		{
			return false;
		}

		int processHandle = static_cast<int>(handle);
		return kill(processHandle, SIGKILL) == 0;
	}

	LibraryHandle OpenLibrary(String path)
	{
		ArenaScope scratch = GetScratchArena();

		char* filePath = ConvertString(scratch.arena, path);
		void* libraryHandle = dlopen(filePath, RTLD_LAZY | RTLD_LOCAL);

		return reinterpret_cast<LibraryHandle>(libraryHandle);
	}

	void* GetLibrarySymbol(LibraryHandle handle, String name)
	{
		if (!handle)
		{
			return nullptr;
		}

		ArenaScope scratch = GetScratchArena();

		void* libraryHandle = reinterpret_cast<void*>(handle);
		char* symbolName = ConvertString(scratch.arena, name);

		return dlsym(libraryHandle, symbolName);
	}

	void CloseLibrary(LibraryHandle handle)
	{
		if (!handle)
		{
			return;
		}

		void* libraryHandle = reinterpret_cast<void*>(handle);
		dlclose(libraryHandle);
	}
}
