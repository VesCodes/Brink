#pragma once

#include "Core.h"
#include "String.h"

extern int32 AppMain(int32 argc, char** argv);

namespace Bk
{
	struct AppEvent;
	enum class KeyCode : uint32;

	typedef bool (*AppUpdateCallback)();
	typedef bool (*AppEventCallback)(const AppEvent& event);
	typedef void (*AppExitCallback)();

	struct AppConfig
	{
		AppUpdateCallback updateCallback;
		AppEventCallback eventCallback;
		AppExitCallback exitCallback;
	};

	void ConfigureApp(const AppConfig& config);

	enum class AppEventType : uint8
	{
		Key,
		MouseMove,
		MouseWheel,
		DropFile,
		WindowResize,
	};

	struct AppEvent
	{
		AppEventType type;
		uint32 target;

		union
		{
			struct // AppEventType::Key
			{
				KeyCode keyCode;
				bool keyPressed;
			};

			struct // AppEventType::MouseMove
			{
				float mouseX;
				float mouseY;
			};

			struct // AppEventType::MouseWheel
			{
				float wheelDelta;
			};

			struct // AppEventType::DropFile
			{
				String dropFilePath;
			};

			struct // AppEventType::WindowResize
			{
				int32 windowWidth;
				int32 windowHeight;
			};
		};
	};

	bool ProcessAppEvent(const AppEvent& event);

	enum class KeyCode : uint32
	{
		Unknown,

		Backspace = 8,
		Tab = 9,
		Enter = 13,
		Escape = 27,
		Spacebar = 32,
		Apostrophe = 39,
		Comma = 44,
		Minus = 45,
		Period = 46,
		Slash = 47,

		Zero = 48,
		One = 49,
		Two = 50,
		Three = 51,
		Four = 52,
		Five = 53,
		Six = 54,
		Seven = 55,
		Eight = 56,
		Nine = 57,

		Semicolon = 59,
		Equal = 61,

		A = 65,
		B = 66,
		C = 67,
		D = 68,
		E = 69,
		F = 70,
		G = 71,
		H = 72,
		I = 73,
		J = 74,
		K = 75,
		L = 76,
		M = 77,
		N = 78,
		O = 79,
		P = 80,
		Q = 81,
		R = 82,
		S = 83,
		T = 84,
		U = 85,
		V = 86,
		W = 87,
		X = 88,
		Y = 89,
		Z = 90,

		LeftBracket = 91,
		Backslash = 92,
		RightBracket = 93,
		Grave = 96,

		Delete = 127,

		ArrowUp,
		ArrowDown,
		ArrowLeft,
		ArrowRight,

		PrintScreen,
		ScrollLock,
		Pause,

		Insert,
		Home,
		End,
		PageUp,
		PageDown,

		LeftCtrl,
		LeftShift,
		LeftAlt,
		LeftSuper,

		RightCtrl,
		RightShift,
		RightAlt,
		RightSuper,

		CapsLock,
		NumLock,

		F1,
		F2,
		F3,
		F4,
		F5,
		F6,
		F7,
		F8,
		F9,
		F10,
		F11,
		F12,

		LeftMouseButton,
		MiddleMouseButton,
		RightMouseButton,

		Count
	};
}
