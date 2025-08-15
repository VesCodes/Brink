#include "Application.h"

#if BK_PLATFORM_EMSCRIPTEN
#include <emscripten/dom_pk_codes.h>
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
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

#if BK_PLATFORM_EMSCRIPTEN
	KeyCode ConvertKeyCode(const char* code)
	{
		switch (emscripten_compute_dom_pk_code(code))
		{
#define CASE(to, from) case from: return KeyCode::to

			CASE(Backspace, DOM_PK_BACKSPACE);
			CASE(Tab, DOM_PK_TAB);
			CASE(Enter, DOM_PK_ENTER);
			CASE(Escape, DOM_PK_ESCAPE);
			CASE(Spacebar, DOM_PK_SPACE);
			CASE(Apostrophe, DOM_PK_QUOTE);
			CASE(Comma, DOM_PK_COMMA);
			CASE(Minus, DOM_PK_MINUS);
			CASE(Period, DOM_PK_PERIOD);
			CASE(Slash, DOM_PK_SLASH);

			CASE(Zero, DOM_PK_0);
			CASE(One, DOM_PK_1);
			CASE(Two, DOM_PK_2);
			CASE(Three, DOM_PK_3);
			CASE(Four, DOM_PK_4);
			CASE(Five, DOM_PK_5);
			CASE(Six, DOM_PK_6);
			CASE(Seven, DOM_PK_7);
			CASE(Eight, DOM_PK_8);
			CASE(Nine, DOM_PK_9);

			CASE(Semicolon, DOM_PK_SEMICOLON);
			CASE(Equal, DOM_PK_EQUAL);

			CASE(A, DOM_PK_A);
			CASE(B, DOM_PK_B);
			CASE(C, DOM_PK_C);
			CASE(D, DOM_PK_D);
			CASE(E, DOM_PK_E);
			CASE(F, DOM_PK_F);
			CASE(G, DOM_PK_G);
			CASE(H, DOM_PK_H);
			CASE(I, DOM_PK_I);
			CASE(J, DOM_PK_J);
			CASE(K, DOM_PK_K);
			CASE(L, DOM_PK_L);
			CASE(M, DOM_PK_M);
			CASE(N, DOM_PK_N);
			CASE(O, DOM_PK_O);
			CASE(P, DOM_PK_P);
			CASE(Q, DOM_PK_Q);
			CASE(R, DOM_PK_R);
			CASE(S, DOM_PK_S);
			CASE(T, DOM_PK_T);
			CASE(U, DOM_PK_U);
			CASE(V, DOM_PK_V);
			CASE(W, DOM_PK_W);
			CASE(X, DOM_PK_X);
			CASE(Y, DOM_PK_Y);
			CASE(Z, DOM_PK_Z);

			CASE(LeftBracket, DOM_PK_BRACKET_LEFT);
			CASE(Backslash, DOM_PK_BACKSLASH);
			CASE(RightBracket, DOM_PK_BRACKET_RIGHT);
			CASE(Grave, DOM_PK_BACKQUOTE);

			CASE(Delete, DOM_PK_DELETE);

			CASE(ArrowUp, DOM_PK_ARROW_UP);
			CASE(ArrowDown, DOM_PK_ARROW_DOWN);
			CASE(ArrowLeft, DOM_PK_ARROW_LEFT);
			CASE(ArrowRight, DOM_PK_ARROW_RIGHT);

			CASE(PrintScreen, DOM_PK_PRINT_SCREEN);
			CASE(ScrollLock, DOM_PK_SCROLL_LOCK);
			CASE(Pause, DOM_PK_PAUSE);

			CASE(Insert, DOM_PK_INSERT);
			CASE(Home, DOM_PK_HOME);
			CASE(End, DOM_PK_END);
			CASE(PageUp, DOM_PK_PAGE_UP);
			CASE(PageDown, DOM_PK_PAGE_DOWN);

			CASE(LeftCtrl, DOM_PK_CONTROL_LEFT);
			CASE(LeftShift, DOM_PK_SHIFT_LEFT);
			CASE(LeftAlt, DOM_PK_ALT_LEFT);
			CASE(LeftSuper, DOM_PK_OS_LEFT);

			CASE(RightCtrl, DOM_PK_CONTROL_RIGHT);
			CASE(RightShift, DOM_PK_SHIFT_RIGHT);
			CASE(RightAlt, DOM_PK_ALT_RIGHT);
			CASE(RightSuper, DOM_PK_OS_RIGHT);

			CASE(CapsLock, DOM_PK_CAPS_LOCK);
			CASE(NumLock, DOM_PK_NUM_LOCK);

			CASE(F1, DOM_PK_F1);
			CASE(F2, DOM_PK_F2);
			CASE(F3, DOM_PK_F3);
			CASE(F4, DOM_PK_F4);
			CASE(F5, DOM_PK_F5);
			CASE(F6, DOM_PK_F6);
			CASE(F7, DOM_PK_F7);
			CASE(F8, DOM_PK_F8);
			CASE(F9, DOM_PK_F9);
			CASE(F10, DOM_PK_F10);
			CASE(F11, DOM_PK_F11);
			CASE(F12, DOM_PK_F12);

#undef CASE

			default: return KeyCode::Unknown;
		}
	}

	KeyCode ConvertMouseButton(const uint16 button)
	{
		switch (button)
		{
			case 0: return KeyCode::LeftMouseButton;
			case 1: return KeyCode::MiddleMouseButton;
			case 2: return KeyCode::RightMouseButton;

			default: return KeyCode::Unknown;
		}
	}

	bool ProcessKeyEvent(int32 type, const EmscriptenKeyboardEvent* event, void* userData)
	{
		if (!appContext.eventCallback)
		{
			return false;
		}

		bool result = false;

		if (type == EMSCRIPTEN_EVENT_KEYDOWN || type == EMSCRIPTEN_EVENT_KEYUP)
		{
			AppEvent appEvent = {};
			appEvent.type = AppEventType::Key;
			appEvent.keyCode = ConvertKeyCode(event->code);
			appEvent.keyPressed = type == EMSCRIPTEN_EVENT_KEYDOWN;

			result = appContext.eventCallback(appEvent);
		}

		return result;
	}

	bool ProcessMouseEvent(int32 type, const EmscriptenMouseEvent* event, void* userData)
	{
		if (!appContext.eventCallback)
		{
			return false;
		}

		bool result = false;

		if (type == EMSCRIPTEN_EVENT_MOUSEDOWN || type == EMSCRIPTEN_EVENT_MOUSEUP)
		{
			AppEvent appEvent = {};
			appEvent.type = AppEventType::Key;
			appEvent.keyCode = ConvertMouseButton(event->button);
			appEvent.keyPressed = type == EMSCRIPTEN_EVENT_MOUSEDOWN;

			result = appContext.eventCallback(appEvent);
		}
		else if (type == EMSCRIPTEN_EVENT_MOUSEMOVE)
		{
			AppEvent appEvent = {};
			appEvent.type = AppEventType::MouseMove;
			appEvent.mouseX = static_cast<float>(event->targetX);
			appEvent.mouseY = static_cast<float>(event->targetY);

			result = appContext.eventCallback(appEvent);
		}

		return result;
	}

	bool ProcessWheelEvent(int32 type, const EmscriptenWheelEvent* event, void* userData)
	{
		if (!appContext.eventCallback)
		{
			return false;
		}

		bool result = false;

		if (type == EMSCRIPTEN_EVENT_WHEEL)
		{
			AppEvent appEvent = {};
			appEvent.type = AppEventType::MouseWheel;
			appEvent.wheelDelta = static_cast<float>(event->deltaY);

			switch (event->deltaMode)
			{
				case DOM_DELTA_PIXEL: appEvent.wheelDelta *= 0.01f; break;
				case DOM_DELTA_LINE: appEvent.wheelDelta *= 0.3f; break;
				case DOM_DELTA_PAGE: appEvent.wheelDelta *= 80.0f; break;

				default: break;
			}

			result = appContext.eventCallback(appEvent);
		}

		return result;
	}
#endif

	void AppLoop()
	{
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
	emscripten_set_keydown_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, true, ProcessKeyEvent);
	emscripten_set_keyup_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, true, ProcessKeyEvent);
	emscripten_set_mousedown_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, true, ProcessMouseEvent);
	emscripten_set_mouseup_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, true, ProcessMouseEvent);
	emscripten_set_mousemove_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, true, ProcessMouseEvent);
	emscripten_set_wheel_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, true, ProcessWheelEvent);

	emscripten_set_main_loop(AppLoop, 0, true);
#else
	do
	{
		AppLoop();
	} while (appContext.running);
#endif

	return result;
}
