#include "Platform.h"

#if BK_PLATFORM_MACOS
#include "PlatformCommon.inl"
#include "PlatformCommonPosix.inl"

namespace Bk
{
	Platform GetPlatform()
	{
		return Platform::MacOS;
	}

	void PumpEvents()
	{
	}
}
#endif
