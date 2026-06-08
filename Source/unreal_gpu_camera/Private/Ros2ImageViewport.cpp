#include "Ros2ImageViewport.h"

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <windows.h>
#include <gl/gl.h>
#include "Windows/HideWindowsPlatformTypes.h"
#endif

FRos2ImageViewport::FRos2ImageViewport() = default;

FRos2ImageViewport::~FRos2ImageViewport()
{
	StopViewport();
}

bool FRos2ImageViewport::StartViewport(const FString& Title, int32 Width, int32 Height)
{
#if PLATFORM_WINDOWS
	WindowTitle = Title;
	WindowWidth = Width;
	WindowHeight = Height;
	StopTask.Reset();
	Thread = FRunnableThread::Create(this, TEXT("Ros2ImageViewport"), 0, TPri_Normal);
	return Thread != nullptr;
#else
	return false;
#endif
}

void FRos2ImageViewport::StopViewport()
{
	StopTask.Set(1);
	if (Thread)
	{
		Thread->WaitForCompletion();
		delete Thread;
		Thread = nullptr;
	}
}

void FRos2ImageViewport::SubmitImage(const uint8* Rgb, int32 Width, int32 Height)
{
	if (!Rgb || Width <= 0 || Height <= 0)
	{
		return;
	}
	FScopeLock Lock(&BufferLock);
	BackBuffer.SetNumUninitialized(Width * Height * 3);
	FMemory::Memcpy(BackBuffer.GetData(), Rgb, BackBuffer.Num());
	PendingWidth = Width;
	PendingHeight = Height;
	Swap(FrontBuffer, BackBuffer);
}

bool FRos2ImageViewport::Init()
{
	return true;
}

void FRos2ImageViewport::Stop()
{
	StopTask.Set(1);
}

#if PLATFORM_WINDOWS
static LRESULT CALLBACK Ros2ImageWndProc(HWND Hwnd, UINT Msg, WPARAM WParam, LPARAM LParam)
{
	if (Msg == WM_CLOSE)
	{
		DestroyWindow(Hwnd);
		return 0;
	}
	if (Msg == WM_DESTROY)
	{
		PostQuitMessage(0);
		return 0;
	}
	return DefWindowProc(Hwnd, Msg, WParam, LParam);
}
#endif

uint32 FRos2ImageViewport::Run()
{
#if PLATFORM_WINDOWS
	WNDCLASS Wc = {};
	Wc.lpfnWndProc = Ros2ImageWndProc;
	Wc.hInstance = GetModuleHandle(nullptr);
	Wc.lpszClassName = TEXT("Ros2ImageViewport");
	RegisterClass(&Wc);
	HWND Hwnd = CreateWindow(
		Wc.lpszClassName, *WindowTitle, WS_OVERLAPPEDWINDOW | WS_VISIBLE,
		CW_USEDEFAULT, CW_USEDEFAULT, WindowWidth, WindowHeight,
		nullptr, nullptr, Wc.hInstance, nullptr);
	if (!Hwnd) { return 1; }
	HDC Hdc = GetDC(Hwnd);
	PIXELFORMATDESCRIPTOR Pfd = {};
	Pfd.nSize = sizeof(Pfd);
	Pfd.nVersion = 1;
	Pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
	Pfd.iPixelType = PFD_TYPE_RGBA;
	Pfd.cColorBits = 32;
	SetPixelFormat(Hdc, ChoosePixelFormat(Hdc, &Pfd), &Pfd);
	HGLRC Glrc = wglCreateContext(Hdc);
	wglMakeCurrent(Hdc, Glrc);
	MSG Msg = {};
	while (StopTask.GetValue() == 0)
	{
		while (PeekMessage(&Msg, nullptr, 0, 0, PM_REMOVE))
		{
			if (Msg.message == WM_QUIT) { StopTask.Set(1); break; }
			TranslateMessage(&Msg);
			DispatchMessage(&Msg);
		}
		RenderLocked();
		SwapBuffers(Hdc);
		Sleep(1);
	}
	wglMakeCurrent(nullptr, nullptr);
	wglDeleteContext(Glrc);
	ReleaseDC(Hwnd, Hdc);
	DestroyWindow(Hwnd);
#endif
	return 0;
}

void FRos2ImageViewport::RenderLocked()
{
#if PLATFORM_WINDOWS
	TArray<uint8> Local;
	int32 W = 0, H = 0;
	{
		FScopeLock Lock(&BufferLock);
		Local = FrontBuffer;
		W = PendingWidth;
		H = PendingHeight;
	}
	glViewport(0, 0, WindowWidth, WindowHeight);
	glClearColor(0.05f, 0.05f, 0.08f, 1.f);
	glClear(GL_COLOR_BUFFER_BIT);
	if (W <= 0 || H <= 0 || Local.Num() < W * H * 3) { return; }
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, 1, 0, 1, -1, 1);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glRasterPos2f(0.f, 1.f);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glPixelZoom(static_cast<float>(WindowWidth) / W, -static_cast<float>(WindowHeight) / H);
	glDrawPixels(W, H, GL_RGB, GL_UNSIGNED_BYTE, Local.GetData());
	glPixelZoom(1.f, 1.f);
#endif
}
