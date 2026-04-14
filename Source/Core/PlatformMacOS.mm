#include "Platform.h"

#if BK_PLATFORM_MACOS
#include "PlatformCommon.inl"
#include "PlatformCommonPosix.inl"

#include "Application.h"
#include "Pool.h"

#include <Cocoa/Cocoa.h>
#include <QuartzCore/CAMetalLayer.h>
#include <Metal/Metal.h>

asm(".linker_option \"-framework\", \"Cocoa\"");
asm(".linker_option \"-framework\", \"QuartzCore\"");
asm(".linker_option \"-framework\", \"Metal\"");

@interface BkWindowDelegate : NSObject<NSWindowDelegate>
@property(nonatomic, assign) uint32 windowHandle;
@end

@implementation BkWindowDelegate
- (void)windowWillClose:(NSNotification*)notification
{
	using namespace Bk;

	AppEvent appEvent = {};
	appEvent.type = AppEventType::WindowClose;
	appEvent.target = self.windowHandle;

	ProcessAppEvent(appEvent);
}

- (void)windowDidResize:(NSNotification*)notification
{
	using namespace Bk;

	NSWindow* window = notification.object;
	NSView* contentView = window.contentView;

	CAMetalLayer* metalLayer = (CAMetalLayer*)contentView.layer;
	metalLayer.drawableSize = [contentView convertSizeToBacking:contentView.frame.size];

	AppEvent appEvent = {};
	appEvent.type = AppEventType::WindowResize;
	appEvent.target = self.windowHandle;
	appEvent.windowWidth = static_cast<int32>(contentView.frame.size.width);
	appEvent.windowHeight = static_cast<int32>(contentView.frame.size.height);

	ProcessAppEvent(appEvent);
}
@end

namespace Bk
{
	struct Window
	{
		NSWindow* handle;
	};

	struct
	{
		Arena arena;

		TPool<Window> windows;
	} platformContext;

	Platform GetPlatform()
	{
		return Platform::MacOS;
	}

	uint32 CreateWindow(const WindowParams& params)
	{
		if (platformContext.windows.capacity == 0)
		{
			// #TODO: Move to a platform init routine
			Allocate(platformContext.arena, platformContext.windows, 128);

			[NSApplication sharedApplication];
			[NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
			[NSApp activateIgnoringOtherApps:YES];
		}

		uint32 handle = 0;
		Window* window = AcquireSlot(platformContext.windows, &handle);

		if (window)
		{
			NSScreen* screen = [NSScreen mainScreen];

			NSRect contentRect = NSMakeRect(0, 0, params.width, params.height);
			contentRect.origin.x = (screen.frame.size.width - contentRect.size.width) / 2;
			contentRect.origin.y = (screen.frame.size.height - contentRect.size.height) / 2;

			window->handle = [[NSWindow alloc] initWithContentRect:contentRect
				styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskResizable)
				backing:NSBackingStoreBuffered
				defer:NO
				screen:screen];

			// [window->handle setTitle:@"Brink"]; // #TODO
			[window->handle makeKeyAndOrderFront:nil];

			BkWindowDelegate* delegate = [[BkWindowDelegate alloc] init];
			delegate.windowHandle = handle;

			[window->handle setDelegate:delegate];

			NSView* contentView = window->handle.contentView;

			CAMetalLayer* metalLayer = [[CAMetalLayer alloc] init];
			metalLayer.device = MTLCreateSystemDefaultDevice();
			metalLayer.pixelFormat = MTLPixelFormatBGRA8Unorm;
			metalLayer.opaque = YES;
			metalLayer.drawableSize = [contentView convertSizeToBacking:contentView.frame.size];

			contentView.layer = metalLayer;
		}

		return handle;
	}

	void* GetWindowSurfaceTarget(uint32 handle)
	{
		void* result = nullptr;

		Window* window = GetSlot(platformContext.windows, handle);
		if (window)
		{
			CAMetalLayer* metalLayer = (CAMetalLayer*)window->handle.contentView.layer;
			result = metalLayer;
		}

		return result;
	}

	bool GetWindowSurfaceSize(uint32 handle, uint32& width, uint32& height)
	{
		bool result = false;

		Window* window = GetSlot(platformContext.windows, handle);
		if (window)
		{
			CAMetalLayer* metalLayer = (CAMetalLayer*)window->handle.contentView.layer;

			width = static_cast<uint32>(metalLayer.drawableSize.width);
			height = static_cast<uint32>(metalLayer.drawableSize.height);

			result = true;
		}

		return result;
	}

	void PumpEvents()
	{
		NSEvent* event;
		do
		{
			event = [NSApp nextEventMatchingMask:NSEventMaskAny
						untilDate:nil
						inMode:NSDefaultRunLoopMode
						dequeue:YES];

			if (event)
			{
				[NSApp sendEvent:event];
				[NSApp updateWindows];

				// #TODO: Process events
			}
		} while(event);
	}
}
#endif
