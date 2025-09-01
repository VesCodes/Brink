#include "Platform.h"

#if BK_PLATFORM_WINDOWS
#include "PlatformCommon.inl"

#include "Application.h"
#include "Memory.h"
#include "Pool.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <shellapi.h>

#undef MoveFile
#undef CopyFile
#undef DeleteFile
#undef CreateDirectory
#undef GetCurrentDirectory
#undef CreateProcess
#undef CreateWindow

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "shell32.lib")

namespace Bk
{
	struct Window
	{
		HWND handle;
	};

	struct
	{
		Arena arena;

		TPool<Window> windows;
	} platformContext;

	Platform GetPlatform()
	{
		return Platform::Windows;
	}

	uint64 GetCpuTicks()
	{
		LARGE_INTEGER counter;
		QueryPerformanceCounter(&counter);

		return static_cast<uint64>(counter.QuadPart);
	}

	uint64 GetCpuFrequency()
	{
		LARGE_INTEGER frequency;
		QueryPerformanceFrequency(&frequency);

		return static_cast<uint64>(frequency.QuadPart);
	}

	DateTime GetUtcTime()
	{
		DateTime result = {};

		SYSTEMTIME systemTime;
		GetSystemTime(&systemTime);

		result.year = systemTime.wYear;
		result.month = static_cast<uint8>(systemTime.wMonth);
		result.weekday = static_cast<uint8>(systemTime.wDayOfWeek);
		result.day = static_cast<uint8>(systemTime.wDay);
		result.hour = static_cast<uint8>(systemTime.wHour);
		result.minute = static_cast<uint8>(systemTime.wMinute);
		result.second = static_cast<uint8>(systemTime.wSecond);
		result.millisecond = systemTime.wMilliseconds;

		return result;
	}

	DateTime GetLocalTime()
	{
		DateTime result = {};

		SYSTEMTIME systemTime;
		GetLocalTime(&systemTime);

		result.year = systemTime.wYear;
		result.month = static_cast<uint8>(systemTime.wMonth);
		result.weekday = static_cast<uint8>(systemTime.wDayOfWeek);
		result.day = static_cast<uint8>(systemTime.wDay);
		result.hour = static_cast<uint8>(systemTime.wHour);
		result.minute = static_cast<uint8>(systemTime.wMinute);
		result.second = static_cast<uint8>(systemTime.wSecond);
		result.millisecond = systemTime.wMilliseconds;

		return result;
	}

	uint64 GetUnixTime()
	{
		FILETIME fileTime = {};
		GetSystemTimeAsFileTime(&fileTime);

		constexpr uint64 epochOffset = 0x019DB1DED53E8000ull;
		constexpr uint64 period = 10'000'000ull; // 1sec in 100ns intervals

		return ((static_cast<uint64>(fileTime.dwHighDateTime) << 32 | fileTime.dwLowDateTime) - epochOffset) / period;
	}

	wchar_t* ConvertString(Arena& arena, String string)
	{
		int32 length = MultiByteToWideChar(CP_UTF8, 0, string.data, string.length, nullptr, 0);
		if (length == 0)
		{
			return nullptr;
		}

		wchar_t* result = arena.Push<wchar_t>(length + 1);
		MultiByteToWideChar(CP_UTF8, 0, string.data, string.length, result, length);
		result[length] = '\0';

		return result;
	}

	String ConvertString(Arena& arena, const wchar_t* string)
	{
		size_t stringLength = wcslen(string);

		int32 length = WideCharToMultiByte(CP_UTF8, 0, string, stringLength, nullptr, 0, nullptr, nullptr);
		if (length == 0)
		{
			return String::Empty;
		}

		TSpan<char> result = arena.Push<char>(length);
		WideCharToMultiByte(CP_UTF8, 0, string, stringLength, result.data, result.length, nullptr, nullptr);

		return String(result.data, result.length);
	}

	DateTime ConvertFileTime(FILETIME time)
	{
		DateTime result = {};

		SYSTEMTIME systemTime = {};
		if (FileTimeToSystemTime(&time, &systemTime))
		{
			result.year = systemTime.wYear;
			result.month = static_cast<uint8>(systemTime.wMonth);
			result.weekday = static_cast<uint8>(systemTime.wDayOfWeek);
			result.day = static_cast<uint8>(systemTime.wDay);
			result.hour = static_cast<uint8>(systemTime.wHour);
			result.minute = static_cast<uint8>(systemTime.wMinute);
			result.second = static_cast<uint8>(systemTime.wSecond);
			result.millisecond = systemTime.wMilliseconds;
		}

		return result;
	}

	FileHandle OpenFile(String path, FileAccess access)
	{
		ArenaScope scratch = GetScratchArena();

		wchar_t* filePath = ConvertString(scratch.arena, path);

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

		HANDLE fileHandle = CreateFileW(filePath, accessFlags, 0, nullptr, disposition, FILE_ATTRIBUTE_NORMAL, nullptr);
		return (fileHandle != INVALID_HANDLE_VALUE) ? reinterpret_cast<FileHandle>(fileHandle) : 0;
	}

	void CloseFile(FileHandle handle)
	{
		if (!handle)
		{
			return;
		}

		HANDLE fileHandle = reinterpret_cast<HANDLE>(handle);
		CloseHandle(fileHandle);
	}

	size_t ReadFile(FileHandle handle, TSpan<uint8> buffer)
	{
		if (!handle)
		{
			return 0;
		}

		HANDLE fileHandle = reinterpret_cast<HANDLE>(handle);

		size_t totalBytesRead = 0;
		size_t bytesLeft = buffer.length;

		while (bytesLeft > 0)
		{
			// OVERLAPPED overlapped = {};
			// overlapped.Offset = static_cast<DWORD>(totalBytesRead);
			// overlapped.OffsetHigh = static_cast<DWORD>(totalBytesRead >> 32);

			DWORD bytesRead;
			BOOL result = ::ReadFile(fileHandle, buffer.data + totalBytesRead, bytesLeft, &bytesRead, nullptr);

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
	}

	size_t WriteFile(FileHandle handle, TSpan<const uint8> buffer)
	{
		if (!handle)
		{
			return 0;
		}

		HANDLE fileHandle = reinterpret_cast<HANDLE>(handle);

		size_t totalBytesWritten = 0;
		size_t bytesLeft = buffer.length;

		while (bytesLeft > 0)
		{
			// OVERLAPPED overlapped = {};
			// overlapped.Offset = static_cast<DWORD>(totalBytesWritten);
			// overlapped.OffsetHigh = static_cast<DWORD>(totalBytesWritten >> 32);

			DWORD bytesWritten;
			BOOL result = ::WriteFile(fileHandle, buffer.data + totalBytesWritten, bytesLeft, &bytesWritten, nullptr);

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
	}

	size_t GetFileSize(FileHandle handle)
	{
		if (!handle)
		{
			return 0;
		}

		HANDLE fileHandle = reinterpret_cast<HANDLE>(handle);

		LARGE_INTEGER fileSize = {};
		GetFileSizeEx(fileHandle, &fileSize);

		return static_cast<size_t>(fileSize.QuadPart);
	}

	FileProperties GetFileProperties(FileHandle handle)
	{
		FileProperties result = {};

		if (!handle)
		{
			return result;
		}

		HANDLE fileHandle = reinterpret_cast<HANDLE>(handle);

		BY_HANDLE_FILE_INFORMATION fileInfo;
		if (GetFileInformationByHandle(fileHandle, &fileInfo))
		{
			result.size = static_cast<size_t>(fileInfo.nFileSizeHigh) << 32 | fileInfo.nFileSizeLow;
			result.createdTime = ConvertFileTime(fileInfo.ftCreationTime);
			result.modifiedTime = ConvertFileTime(fileInfo.ftLastWriteTime);

			if ((fileInfo.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
			{
				result.attributes |= FileAttributes::Directory;
			}

			if ((fileInfo.dwFileAttributes & FILE_ATTRIBUTE_READONLY) != 0)
			{
				result.attributes |= FileAttributes::ReadOnly;
			}
		}

		return result;
	}

	FileProperties GetFileProperties(String path)
	{
		ArenaScope scratch = GetScratchArena();

		FileProperties result = {};

		wchar_t* filePath = ConvertString(scratch.arena, path);

		WIN32_FIND_DATAW findInfo = {};
		HANDLE findHandle = FindFirstFileW(filePath, &findInfo);

		if (findHandle != INVALID_HANDLE_VALUE)
		{
			result.size = static_cast<size_t>(findInfo.nFileSizeHigh) << 32 | findInfo.nFileSizeLow;
			result.createdTime = ConvertFileTime(findInfo.ftCreationTime);
			result.modifiedTime = ConvertFileTime(findInfo.ftLastWriteTime);

			if ((findInfo.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
			{
				result.attributes |= FileAttributes::Directory;
			}

			if ((findInfo.dwFileAttributes & FILE_ATTRIBUTE_READONLY) != 0)
			{
				result.attributes |= FileAttributes::ReadOnly;
			}

			FindClose(findHandle);
		}

		return result;
	}

	bool FileExists(String path)
	{
		ArenaScope scratch = GetScratchArena();

		wchar_t* filePath = ConvertString(scratch.arena, path);
		uint32 fileAttributes = GetFileAttributesW(filePath);

		return fileAttributes != INVALID_FILE_ATTRIBUTES && (fileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
	}

	bool CopyFile(String srcPath, String dstPath)
	{
		ArenaScope scratch = GetScratchArena();

		wchar_t* srcFilePath = ConvertString(scratch.arena, srcPath);
		wchar_t* dstFilePath = ConvertString(scratch.arena, dstPath);

		return CopyFileW(srcFilePath, dstFilePath, false);
	}

	bool MoveFile(String srcPath, String dstPath)
	{
		ArenaScope scratch = GetScratchArena();

		wchar_t* srcFilePath = ConvertString(scratch.arena, srcPath);
		wchar_t* dstFilePath = ConvertString(scratch.arena, dstPath);

		return MoveFileW(srcFilePath, dstFilePath);
	}

	bool DeleteFile(String path)
	{
		ArenaScope scratch = GetScratchArena();

		wchar_t* filePath = ConvertString(scratch.arena, path);
		return DeleteFileW(filePath);
	}

	bool CreateDirectory(String path)
	{
		ArenaScope scratch = GetScratchArena();

		wchar_t* filePath = ConvertString(scratch.arena, path);
		return CreateDirectoryW(filePath, nullptr) || GetLastError() == ERROR_ALREADY_EXISTS;
	}

	struct FileIterator
	{
		Arena* arena;

		StringBuilder pathBuilder;
		size_t pathLength;

		HANDLE findHandle;
		WIN32_FIND_DATAW findInfo;
	};

	FileIteratorHandle CreateFileIterator(Arena& arena, String path)
	{
		ArenaScope scratch = GetScratchArena(&arena);

		FileIterator* iterator = arena.Push<FileIterator>();
		iterator->arena = &arena;
		iterator->pathBuilder = StringBuilder(arena);
		iterator->pathBuilder.AppendPath(path);
		iterator->pathLength = iterator->pathBuilder.length;

		iterator->pathBuilder.AppendPath("*");

		wchar_t* filePath = ConvertString(scratch.arena, iterator->pathBuilder.ToString(scratch.arena));
		iterator->findHandle = FindFirstFileW(filePath, &iterator->findInfo);

		return iterator;
	}

	bool AdvanceFileIterator(FileIteratorHandle iterator, FileIteratorEntry& entry)
	{
		if (!iterator)
		{
			return false;
		}

		bool result = false;

		ArenaScope scratch = GetScratchArena(iterator->arena);
		while (iterator->findHandle != INVALID_HANDLE_VALUE)
		{
			String fileName = ConvertString(scratch.arena, iterator->findInfo.cFileName);
			if (fileName != "." && fileName != "..")
			{
				iterator->pathBuilder.Reset(iterator->pathLength);
				iterator->pathBuilder.AppendPath(fileName);

				entry.path = iterator->pathBuilder.ToString(*iterator->arena);
				entry.properties = {};

				entry.properties.size = static_cast<size_t>(iterator->findInfo.nFileSizeHigh) << 32 | iterator->findInfo.nFileSizeLow;
				entry.properties.createdTime = ConvertFileTime(iterator->findInfo.ftCreationTime);
				entry.properties.modifiedTime = ConvertFileTime(iterator->findInfo.ftLastWriteTime);

				if ((iterator->findInfo.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
				{
					entry.properties.attributes |= FileAttributes::Directory;
				}

				if ((iterator->findInfo.dwFileAttributes & FILE_ATTRIBUTE_READONLY) != 0)
				{
					entry.properties.attributes |= FileAttributes::ReadOnly;
				}

				result = true;
			}

			if (!FindNextFileW(iterator->findHandle, &iterator->findInfo))
			{
				FindClose(iterator->findHandle);
				iterator->findHandle = INVALID_HANDLE_VALUE;
			}

			if (result)
			{
				break;
			}
		}

		return result;
	}

	void DestroyFileIterator(FileIteratorHandle iterator)
	{
		if (!iterator)
		{
			return;
		}

		if (iterator->findHandle != INVALID_HANDLE_VALUE)
		{
			FindClose(iterator->findHandle);
			iterator->findHandle = INVALID_HANDLE_VALUE;
		}
	}

	String GetCurrentDirectory(Arena& arena)
	{
		ArenaScope scratch = GetScratchArena(&arena);

		DWORD pathLength = GetCurrentDirectoryW(0, nullptr);
		wchar_t* path = scratch.arena.Push<wchar_t>(pathLength);
		GetCurrentDirectoryW(pathLength, path);

		StringBuilder builder(scratch.arena);
		builder.Append(ConvertString(scratch.arena, path));
		builder.NormalizePath();

		return builder.ToString(arena);
	}

	ProcessHandle CreateProcess(const ProcessParams& params)
	{
		ArenaScope scratch = GetScratchArena();

		int32 executableLength = MultiByteToWideChar(CP_UTF8, 0, params.executable.data, params.executable.length, nullptr, 0);
		int32 argumentsLength = MultiByteToWideChar(CP_UTF8, 0, params.arguments.data, params.arguments.length, nullptr, 0);

		size_t commandLineLength = executableLength + 3 + argumentsLength;
		wchar_t* commandLine = scratch.arena.Push<wchar_t>(commandLineLength + 1);

		MultiByteToWideChar(CP_UTF8, 0, params.executable.data, params.executable.length, commandLine + 1, executableLength);
		commandLine[0] = '\"';
		commandLine[executableLength + 1] = '\"';
		commandLine[executableLength + 2] = ' ';

		MultiByteToWideChar(CP_UTF8, 0, params.arguments.data, params.arguments.length, commandLine + executableLength + 3, argumentsLength);
		commandLine[commandLineLength] = '\0';

		STARTUPINFOW startupInfo = {};
		startupInfo.cb = sizeof(startupInfo);

		uint32 flags = CREATE_UNICODE_ENVIRONMENT;
		wchar_t* environment = nullptr;

		PROCESS_INFORMATION processInfo = {};

		if (!CreateProcessW(nullptr, commandLine, nullptr, nullptr, false, flags, environment, nullptr, &startupInfo, &processInfo))
		{
			return 0;
		}

		CloseHandle(processInfo.hThread);

		return reinterpret_cast<ProcessHandle>(processInfo.hProcess);
	}

	void DetachProcess(ProcessHandle handle)
	{
		if (!handle)
		{
			return;
		}

		HANDLE processHandle = reinterpret_cast<HANDLE>(handle);
		CloseHandle(processHandle);
	}

	bool WaitForProcess(ProcessHandle handle, int32* exitCode)
	{
		if (!handle)
		{
			return false;
		}

		HANDLE processHandle = reinterpret_cast<HANDLE>(handle);

		bool result = WaitForSingleObject(processHandle, INFINITE) == WAIT_OBJECT_0;
		if (result && exitCode != nullptr)
		{
			result = GetExitCodeProcess(processHandle, reinterpret_cast<DWORD*>(exitCode));
		}

		return result;
	}

	bool TerminateProcess(ProcessHandle handle)
	{
		if (!handle)
		{
			return false;
		}

		HANDLE processHandle = reinterpret_cast<HANDLE>(handle);
		return ::TerminateProcess(processHandle, 0);
	}

	KeyCode ConvertScanCode(uint32 scanCode)
	{
		switch (scanCode)
		{
#define CASE(to, from) case from: return KeyCode::to

			CASE(Backspace, 0x0E);
			CASE(Tab, 0x0F);
			CASE(Enter, 0x1C);
			CASE(Escape, 0x01);
			CASE(Spacebar, 0x39);
			CASE(Apostrophe, 0x28);
			CASE(Comma, 0x33);
			CASE(Minus, 0x0C);
			CASE(Period, 0x34);
			CASE(Slash, 0x35);

			CASE(Zero, 0x0B);
			CASE(One, 0x02);
			CASE(Two, 0x03);
			CASE(Three, 0x04);
			CASE(Four, 0x05);
			CASE(Five, 0x06);
			CASE(Six, 0x07);
			CASE(Seven, 0x08);
			CASE(Eight, 0x09);
			CASE(Nine, 0x0A);

			CASE(Semicolon, 0x27);
			CASE(Equal, 0x0D);

			CASE(A, 0x1E);
			CASE(B, 0x30);
			CASE(C, 0x2E);
			CASE(D, 0x20);
			CASE(E, 0x12);
			CASE(F, 0x21);
			CASE(G, 0x22);
			CASE(H, 0x23);
			CASE(I, 0x17);
			CASE(J, 0x24);
			CASE(K, 0x25);
			CASE(L, 0x26);
			CASE(M, 0x32);
			CASE(N, 0x31);
			CASE(O, 0x18);
			CASE(P, 0x19);
			CASE(Q, 0x10);
			CASE(R, 0x13);
			CASE(S, 0x1F);
			CASE(T, 0x14);
			CASE(U, 0x16);
			CASE(V, 0x2F);
			CASE(W, 0x11);
			CASE(X, 0x2D);
			CASE(Y, 0x15);
			CASE(Z, 0x2C);

			CASE(LeftBracket, 0x1A);
			CASE(Backslash, 0x2B);
			CASE(RightBracket, 0x1B);
			CASE(Grave, 0x29);

			CASE(Delete, 0xE053);

			CASE(ArrowUp, 0xE048);
			CASE(ArrowDown, 0xE050);
			CASE(ArrowLeft, 0xE04B);
			CASE(ArrowRight, 0xE04D);

			CASE(PrintScreen, 0xE037);
			CASE(ScrollLock, 0x46);
			CASE(Pause, 0xE11D45);

			CASE(Insert, 0xE052);
			CASE(Home, 0xE047);
			CASE(End, 0xE04F);
			CASE(PageUp, 0xE049);
			CASE(PageDown, 0xE051);

			CASE(LeftCtrl, 0x1D);
			CASE(LeftShift, 0x2A);
			CASE(LeftAlt, 0x38);
			CASE(LeftSuper, 0xE05B);

			CASE(RightCtrl, 0xE01D);
			CASE(RightShift, 0x36);
			CASE(RightAlt, 0xE038);
			CASE(RightSuper, 0xE05C);

			CASE(CapsLock, 0x3A);
			CASE(NumLock, 0x45);

			CASE(F1, 0x3B);
			CASE(F2, 0x3C);
			CASE(F3, 0x3D);
			CASE(F4, 0x3E);
			CASE(F5, 0x3F);
			CASE(F6, 0x40);
			CASE(F7, 0x41);
			CASE(F8, 0x42);
			CASE(F9, 0x43);
			CASE(F10, 0x44);
			CASE(F11, 0x57);
			CASE(F12, 0x58);

#undef CASE

			default: return KeyCode::Unknown;
		}
	}

	LRESULT WindowProc(HWND handle, UINT msg, WPARAM wParam, LPARAM lParam)
	{
		ArenaScope scratch = GetScratchArena();
		Window* window = reinterpret_cast<Window*>(GetWindowLongPtr(handle, GWLP_USERDATA));

		switch (msg)
		{
			case WM_CREATE:
			{
				CREATESTRUCT* createStruct = reinterpret_cast<CREATESTRUCT*>(lParam);
				SetWindowLongPtr(handle, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(createStruct->lpCreateParams));

				return 0;
			}

			case WM_CLOSE:
			{
				AppEvent appEvent = {};
				appEvent.type = AppEventType::WindowClose;
				appEvent.target = platformContext.windows.GetHandle(window);

				ProcessAppEvent(appEvent);

				break;
			}

			case WM_KEYDOWN:
			case WM_SYSKEYDOWN:
			case WM_KEYUP:
			case WM_SYSKEYUP:
			{
				uint16 flags = HIWORD(lParam);

				uint32 scanCode = LOBYTE(flags);
				if ((flags & KF_EXTENDED) != 0)
				{
					// NumLock/Clear comes through as 0xE045, keep as 0x45
					if (scanCode != 0x45)
					{
						scanCode |= 0xE000;
					}
				}
				else
				{
					// Pause/Break comes through as 0x45, fix up to 0xE11D45
					if (scanCode == 0x45)
					{
						scanCode = 0xE11D45;
					}
					// Alt+PrintScreen comes through as 0x54, fix up to 0xE037
					else if (scanCode == 0x54)
					{
						scanCode = 0xE037;
					}
				}

				AppEvent appEvent = {};
				appEvent.type = AppEventType::Key;
				appEvent.target = platformContext.windows.GetHandle(window);
				appEvent.keyCode = ConvertScanCode(scanCode);
				appEvent.keyPressed = (flags & KF_UP) == 0;

				ProcessAppEvent(appEvent);

				return 0;
			}

			case WM_LBUTTONDOWN:
			case WM_LBUTTONUP:
			{
				AppEvent appEvent = {};
				appEvent.type = AppEventType::Key;
				appEvent.target = platformContext.windows.GetHandle(window);
				appEvent.keyCode = KeyCode::LeftMouseButton;
				appEvent.keyPressed = msg == WM_LBUTTONDOWN;

				ProcessAppEvent(appEvent);

				return 0;
			}

			case WM_MBUTTONDOWN:
			case WM_MBUTTONUP:
			{
				AppEvent appEvent = {};
				appEvent.type = AppEventType::Key;
				appEvent.target = platformContext.windows.GetHandle(window);
				appEvent.keyCode = KeyCode::MiddleMouseButton;
				appEvent.keyPressed = msg == WM_MBUTTONDOWN;

				ProcessAppEvent(appEvent);

				return 0;
			}

			case WM_RBUTTONDOWN:
			case WM_RBUTTONUP:
			{
				AppEvent appEvent = {};
				appEvent.type = AppEventType::Key;
				appEvent.target = platformContext.windows.GetHandle(window);
				appEvent.keyCode = KeyCode::RightMouseButton;
				appEvent.keyPressed = msg == WM_RBUTTONDOWN;

				ProcessAppEvent(appEvent);

				return 0;
			}

			case WM_MOUSEMOVE:
			{
				AppEvent appEvent = {};

				appEvent.type = AppEventType::MouseMove;
				appEvent.target = platformContext.windows.GetHandle(window);
				appEvent.mouseX = static_cast<float>(LOWORD(lParam));
				appEvent.mouseY = static_cast<float>(HIWORD(lParam));

				ProcessAppEvent(appEvent);

				return 0;
			}

			case WM_MOUSEWHEEL:
			{
				AppEvent appEvent = {};
				appEvent.type = AppEventType::MouseWheel;
				appEvent.target = platformContext.windows.GetHandle(window);
				appEvent.wheelDelta = GET_WHEEL_DELTA_WPARAM(wParam) / static_cast<float>(WHEEL_DELTA);

				ProcessAppEvent(appEvent);

				return 0;
			}

			case WM_DROPFILES:
			{
				HDROP drop = reinterpret_cast<HDROP>(wParam);

				size_t numFiles = DragQueryFileW(drop, 0xFFFFFFFF, nullptr, 0);
				for (size_t i = 0; i < numFiles; ++i)
				{
					size_t fileNameLength = DragQueryFileW(drop, i, nullptr, 0) + 1;
					wchar_t* fileName = scratch.arena.Push<wchar_t>(fileNameLength);
					DragQueryFileW(drop, i, fileName, fileNameLength);

					AppEvent appEvent = {};
					appEvent.type = AppEventType::DropFile;
					appEvent.target = platformContext.windows.GetHandle(window);
					appEvent.dropFilePath = ConvertString(scratch.arena, fileName);

					ProcessAppEvent(appEvent);
				}

				DragFinish(drop);

				return 0;
			}

			default: break;
		}

		return DefWindowProc(handle, msg, wParam, lParam);
	}

	uint32 CreateWindow(const WindowParams& params)
	{
		if (platformContext.windows.capacity == 0)
		{
			// #TODO: Move to a platform init routine
			platformContext.windows.Initialize(platformContext.arena, 128);

			WNDCLASSW windowClass = {};
			windowClass.lpszClassName = L"BrinkWindowClass";
			windowClass.lpfnWndProc = WindowProc;
			windowClass.hInstance = GetModuleHandleW(nullptr);
			windowClass.hCursor = LoadCursorA(nullptr, IDC_ARROW);

			BK_ASSERT(RegisterClassW(&windowClass) != 0);
		}

		uint32 handle = 0;
		Window* window = platformContext.windows.Acquire(&handle);

		if (window)
		{
			wchar_t* windowName = ConvertString(platformContext.arena, params.title);

			uint32 windowStyle = WS_OVERLAPPEDWINDOW;
			uint32 windowStyleEx = WS_EX_APPWINDOW | WS_EX_ACCEPTFILES;

			window->handle = CreateWindowExW(
				windowStyleEx, L"BrinkWindowClass", windowName, windowStyle,
				CW_USEDEFAULT, CW_USEDEFAULT, params.width, params.height,
				nullptr, nullptr, nullptr, window);

			ShowWindow(window->handle, SW_SHOW);
		}

		return handle;
	}

	void DestroyWindow(uint32 handle)
	{
		Window* window = platformContext.windows.Get(handle);
		if (window)
		{
			DestroyWindow(window->handle);
			platformContext.windows.Release(handle);
		}
	}

	void* GetWindowSurfaceTarget(uint32 handle)
	{
		void* result = nullptr;

		Window* window = platformContext.windows.Get(handle);
		if (window)
		{
			result = window->handle;
		}

		return result;
	}

	bool GetWindowSurfaceSize(uint32 handle, uint32& width, uint32& height)
	{
		bool result = false;

		Window* window = platformContext.windows.Get(handle);
		if (window)
		{
			RECT surfaceRect = {};
			GetClientRect(window->handle, &surfaceRect);

			width = surfaceRect.right - surfaceRect.left;
			height = surfaceRect.bottom - surfaceRect.top;

			result = true;
		}

		return result;
	}

	void PumpEvents()
	{
		MSG message;
		while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&message);
			DispatchMessageW(&message);
		}
	}
}
#endif
