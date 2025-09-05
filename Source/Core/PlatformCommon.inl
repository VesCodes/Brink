#pragma once

#include "Memory.h"

namespace Bk
{
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

	uint64 GetTimeMs()
	{
		uint64 period = GetCpuFrequency();
		return GetCpuTicks() * 1'000 / period;
	}

	double GetTimeSec()
	{
		double period = 1.0 / static_cast<double>(GetCpuFrequency());
		return static_cast<double>(GetCpuTicks()) * period;
	}

	bool CopyDirectory(String srcPath, String dstPath)
	{
		ArenaScope scratch = GetScratchArena();

		StringBuilder builder(scratch.arena);
		AppendPath(builder, dstPath);
		size_t dstPathLength = builder.length;

		bool result = true;

		FileIteratorHandle fileIt = CreateFileIterator(scratch.arena, srcPath);
		for (FileIteratorEntry file; AdvanceFileIterator(fileIt, file);)
		{
			if (EnumHasAllFlags(file.properties.attributes, FileAttributes::Directory))
			{
				continue;
			}

			Reset(builder, dstPathLength);
			AppendPath(builder, GetFileName(file.path));

			String srcFilePath = file.path;
			String dstFilePath = ToString(builder, scratch.arena);

			if (!CopyFile(srcFilePath, dstFilePath))
			{
				result = false;
				break;
			}
		}

		DestroyFileIterator(fileIt);

		return result;
	}
}
