#include "Lidar360Win32Viewport.h"

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <windows.h>
#include <gl/gl.h>
#include "Windows/HideWindowsPlatformTypes.h"
#endif

FLidar360Win32Viewport::FLidar360Win32Viewport() = default;
FLidar360Win32Viewport::~FLidar360Win32Viewport()
{
	StopViewport();
}

bool FLidar360Win32Viewport::StartViewport(const FString& Title, int32 Width, int32 Height)
{
#if PLATFORM_WINDOWS
	WindowTitle = Title;
	WindowWidth = Width;
	WindowHeight = Height;
	StopTask.Reset();
	Thread = FRunnableThread::Create(this, TEXT("Lidar360Viewport"), 0, TPri_Normal);
	return Thread != nullptr;
#else
	return false;
#endif
}

void FLidar360Win32Viewport::StopViewport()
{
	StopTask.Set(1);
	if (Thread)
	{
		Thread->WaitForCompletion();
		delete Thread;
		Thread = nullptr;
	}
}

void FLidar360Win32Viewport::SubmitPoints(const float* XyzIntensity, int32 PointCount, float MaxRangeM)
{
	if (!XyzIntensity || PointCount <= 0)
	{
		return;
	}
	FScopeLock Lock(&BufferLock);
	BackBuffer.SetNumUninitialized(PointCount * 4);
	FMemory::Memcpy(BackBuffer.GetData(), XyzIntensity, PointCount * 4 * sizeof(float));
	PendingPointCount = PointCount;
	PendingMaxRange = MaxRangeM;
	Swap(FrontBuffer, BackBuffer);
}

bool FLidar360Win32Viewport::Init()
{
	return true;
}

void FLidar360Win32Viewport::Stop()
{
	StopTask.Set(1);
}

#if PLATFORM_WINDOWS
static LRESULT CALLBACK Lidar360WndProc(HWND Hwnd, UINT Msg, WPARAM WParam, LPARAM LParam)
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

uint32 FLidar360Win32Viewport::Run()
{
#if PLATFORM_WINDOWS
	WNDCLASS Wc = {};
	Wc.lpfnWndProc = Lidar360WndProc;
	Wc.hInstance = GetModuleHandle(nullptr);
	Wc.lpszClassName = TEXT("Lidar360Viewport");
	RegisterClass(&Wc);
	const DWORD Style = WS_OVERLAPPEDWINDOW | WS_VISIBLE;
	HWND Hwnd = CreateWindow(
		Wc.lpszClassName,
		*WindowTitle,
		Style,
		CW_USEDEFAULT, CW_USEDEFAULT,
		WindowWidth, WindowHeight,
		nullptr, nullptr, Wc.hInstance, nullptr);
	if (!Hwnd)
	{
		return 1;
	}
	HDC Hdc = GetDC(Hwnd);
	PIXELFORMATDESCRIPTOR Pfd = {};
	Pfd.nSize = sizeof(Pfd);
	Pfd.nVersion = 1;
	Pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
	Pfd.iPixelType = PFD_TYPE_RGBA;
	Pfd.cColorBits = 32;
	Pfd.cDepthBits = 24;
	SetPixelFormat(Hdc, ChoosePixelFormat(Hdc, &Pfd), &Pfd);
	HGLRC Glrc = wglCreateContext(Hdc);
	wglMakeCurrent(Hdc, Glrc);
	glEnable(GL_PROGRAM_POINT_SIZE);
	glPointSize(2.f);

	MSG Msg = {};
	while (StopTask.GetValue() == 0)
	{
		while (PeekMessage(&Msg, nullptr, 0, 0, PM_REMOVE))
		{
			if (Msg.message == WM_QUIT)
			{
				StopTask.Set(1);
				break;
			}
			TranslateMessage(&Msg);
			DispatchMessage(&Msg);
		}
		RenderLocked();
		SwapBuffers(Hdc);
		Sleep(8);
	}
	wglMakeCurrent(nullptr, nullptr);
	wglDeleteContext(Glrc);
	ReleaseDC(Hwnd, Hdc);
	DestroyWindow(Hwnd);
#endif
	return 0;
}

void FLidar360Win32Viewport::RenderLocked()
{
#if PLATFORM_WINDOWS
	TArray<float> Local;
	int32 Count = 0;
	float Range = 200.f;
	{
		FScopeLock Lock(&BufferLock);
		Local = FrontBuffer;
		Count = PendingPointCount;
		Range = PendingMaxRange;
	}
	glViewport(0, 0, WindowWidth, WindowHeight);
	glClearColor(0.02f, 0.02f, 0.04f, 1.f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	if (Count <= 0)
	{
		return;
	}
	const float Scale = 2.f / FMath::Max(Range, 1.f);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glRotatef(-20.f, 1.f, 0.f, 0.f);
	glRotatef(35.f, 0.f, 1.f, 0.f);
	glBegin(GL_POINTS);
	for (int32 i = 0; i < Count; ++i)
	{
		const float* P = &Local[i * 4];
		const float I = FMath::Clamp(P[3], 0.f, 1.f);
		glColor3f(I, 0.35f + 0.55f * I, 1.f - I);
		glVertex3f(P[0] * Scale, P[2] * Scale, -P[1] * Scale);
	}
	glEnd();
#endif
}
