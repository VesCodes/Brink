#include "Application.h"

#include "Platform.h"

#include <stdlib.h>

#if BK_PLATFORM_EMSCRIPTEN
#include <emscripten/emscripten.h>
#endif

namespace Bk
{
	struct
	{
		AppUpdateCallback updateCallback;
		AppEventCallback eventCallback;
		AppExitCallback exitCallback;

		bool running;
	} appContext;

	void ConfigureApp(const AppConfig& config)
	{
		appContext.updateCallback = config.updateCallback;
		appContext.eventCallback = config.eventCallback;
		appContext.exitCallback = config.exitCallback;
	}

	void ExitApp(int32 exitCode)
	{
		if (appContext.exitCallback)
		{
			appContext.exitCallback();
		}

#if BK_PLATFORM_EMSCRIPTEN
		emscripten_cancel_main_loop();
#endif

		exit(exitCode);
	}

	bool ProcessAppEvent(const AppEvent& event)
	{
		bool result = false;

		if (appContext.eventCallback)
		{
			result = appContext.eventCallback(event);
		}

		return result;
	}

	void AppLoop()
	{
		PumpEvents();

		if (appContext.updateCallback)
		{
			appContext.running = appContext.updateCallback();
		}

		if (!appContext.running)
		{
			if (appContext.exitCallback)
			{
				appContext.exitCallback();
			}

#if BK_PLATFORM_EMSCRIPTEN
			emscripten_cancel_main_loop();
#endif
		}
	}
}

int32 main(int32 argc, char** argv)
{
	using namespace Bk;

	int32 result = AppMain(argc, argv);

#if BK_PLATFORM_EMSCRIPTEN
	emscripten_set_main_loop(AppLoop, 0, true);
#else
	do
	{
		AppLoop();
	} while (appContext.running);
#endif

	return result;
}
