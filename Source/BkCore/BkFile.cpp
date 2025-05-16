#include "BkFile.h"

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

namespace Bk
{
	FileHandle OpenFile(const char* path, FileAccess access)
	{
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

		int fd = open(path, flags, 0755);
		return (fd != -1) ? static_cast<FileHandle>(fd) : 0;
	}

	void CloseFile(FileHandle handle)
	{
		if (!handle)
		{
			return;
		}

		int fd = static_cast<int>(handle);
		close(fd);
	}

	size_t ReadFile(FileHandle handle, TSpan<uint8> buffer)
	{
		if (!handle)
		{
			return 0;
		}

		int fd = static_cast<int>(handle);

		size_t bytesRead = 0;
		size_t bytesLeft = buffer.length;

		while (bytesLeft > 0)
		{
			ssize_t result = read(fd, buffer.data + bytesRead, bytesLeft);
			if (result > 0)
			{
				bytesRead += static_cast<size_t>(result);
				bytesLeft -= static_cast<size_t>(result);
			}
			else if (errno != EINTR)
			{
				break;
			}
		}

		return bytesRead;
	}

	size_t WriteFile(FileHandle handle, TSpan<uint8> buffer)
	{
		if (!handle)
		{
			return 0;
		}

		int fd = static_cast<int>(handle);

		size_t bytesWritten = 0;
		size_t bytesLeft = buffer.length;

		while (bytesLeft > 0)
		{
			ssize_t result = write(fd, buffer.data + bytesWritten, bytesLeft);
			if (result > 0)
			{
				bytesWritten += static_cast<size_t>(result);
				bytesLeft -= static_cast<size_t>(result);
			}
			else if (errno != EINTR)
			{
				break;
			}
		}

		return bytesWritten;
	}
}
