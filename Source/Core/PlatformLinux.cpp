#include "Platform.h"

#if BK_PLATFORM_LINUX
#include "PlatformCommon.inl"
#include "PlatformCommonPosix.inl"

namespace Bk
{
	Platform GetPlatform()
	{
		return Platform::Linux;
	}

	void PumpEvents()
	{
	}
}
#endif
