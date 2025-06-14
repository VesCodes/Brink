#include "BkFile.h"

#if defined(BK_PLATFORM_WINDOWS)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace Bk
{
	FileHandle OpenFile(const char* path, FileAccess access)
	{
#if defined(BK_PLATFORM_WINDOWS)
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

		HANDLE osHandle = CreateFileA(path, accessFlags, 0, nullptr, disposition, FILE_ATTRIBUTE_NORMAL, nullptr);
		return (osHandle != INVALID_HANDLE_VALUE) ? reinterpret_cast<FileHandle>(osHandle) : 0;
#else
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

		int osHandle = open(path, flags, 0755);
		return (osHandle != -1) ? static_cast<FileHandle>(osHandle) : 0;
#endif
	}

	void CloseFile(FileHandle handle)
	{
		if (!handle)
		{
			return;
		}
#if defined(BK_PLATFORM_WINDOWS)
		HANDLE osHandle = reinterpret_cast<HANDLE>(handle);
		CloseHandle(osHandle);
#else
		int osHandle = static_cast<int>(handle);
		close(osHandle);
#endif
	}

	size_t ReadFile(FileHandle handle, TSpan<uint8> buffer)
	{
		if (!handle)
		{
			return 0;
		}

#if defined(BK_PLATFORM_WINDOWS)
		HANDLE osHandle = reinterpret_cast<HANDLE>(handle);

		size_t totalBytesRead = 0;
		size_t bytesLeft = buffer.length;

		while (bytesLeft > 0)
		{
			// OVERLAPPED overlapped = {};
			// overlapped.Offset = static_cast<DWORD>(totalBytesRead);
			// overlapped.OffsetHigh = static_cast<DWORD>(totalBytesRead >> 32);

			DWORD bytesRead;
			BOOL result = ::ReadFile(osHandle, buffer.data + totalBytesRead, bytesLeft, &bytesRead, nullptr);

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
		int osHandle = static_cast<int>(handle);

		size_t totalBytesRead = 0;
		size_t bytesLeft = buffer.length;

		while (bytesLeft > 0)
		{
			ssize_t bytesRead = read(osHandle, buffer.data + totalBytesRead, bytesLeft);
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

#if defined(BK_PLATFORM_WINDOWS)
		HANDLE osHandle = reinterpret_cast<HANDLE>(handle);

		size_t totalBytesWritten = 0;
		size_t bytesLeft = buffer.length;

		while (bytesLeft > 0)
		{
			// OVERLAPPED overlapped = {};
			// overlapped.Offset = static_cast<DWORD>(totalBytesWritten);
			// overlapped.OffsetHigh = static_cast<DWORD>(totalBytesWritten >> 32);

			DWORD bytesWritten;
			BOOL result = ::WriteFile(osHandle, buffer.data + totalBytesWritten, bytesLeft, &bytesWritten, nullptr);

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
		int osHandle = static_cast<int>(handle);

		size_t totalBytesWritten = 0;
		size_t bytesLeft = buffer.length;

		while (bytesLeft > 0)
		{
			ssize_t bytesWritten = write(osHandle, buffer.data + totalBytesWritten, bytesLeft);
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

#if defined(BK_PLATFORM_WINDOWS)
		HANDLE osHandle = reinterpret_cast<HANDLE>(handle);

		LARGE_INTEGER fileSize = {};
		GetFileSizeEx(osHandle, &fileSize);

		return static_cast<size_t>(fileSize.QuadPart);
#else
		int osHandle = static_cast<int>(handle);

		struct stat fileStat = {};
		fstat(osHandle, &fileStat);

		return static_cast<size_t>(fileStat.st_size);
#endif
	}
}
