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
}
